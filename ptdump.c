/* fun with the Intel Graphics Page Tables, yay! :) */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include <sys/io.h>

#include "intelfbhw.h"
#include "find_intel.h"
#include "ringbuffer.h"
#include "util.h"
#include "mmap.h"

#if defined(__i386__)
signed long long lseek64(int fd,signed long long where,int whence);
#endif

/* we need direct access to physical mem. for this */
void *map_physical(uint32_t addr,uint32_t size) {
	void *x = mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,addr);
	if (x == ((void*)-1)) return NULL;
	return x;
}

void unmap_page(void *page,uint32_t size) {
	munmap(page,size);
}

void mem_close() {
	if (mem_fd >= 0) close(mem_fd);
	mem_fd = -1;
}

uint32_t gtt_sizes[8] = {
	512*1024,		/* 512KB */
	256*1024,		/* 256KB */
	128*1024,		/* 128KB */
	1024*1024,		/* 1MB */
	2048*1024,		/* 2MB */
	(1024+512)*1024,	/* 1.5MB */
	0,
	0
};

int pcicfg_fd = -1;

int open_intel_pcicfg(const char *dev) {
	char path[256];
	sprintf(path,"/sys/bus/pci/devices/%s/config",dev); // sysfs_intel_graphics_dev);
	printf("Opening PCI config space: %s\n",path);
	if ((pcicfg_fd = open(path,O_RDWR)) < 0) return 0;
	return 1;
}

int close_intel_pcicfg() {
	if (pcicfg_fd >= 0) close(pcicfg_fd);
	pcicfg_fd = -1;
	return 0;
}

uint8_t intel_pcicfg_u8(uint32_t o) {
	uint8_t a=~0;
	lseek(pcicfg_fd,o,SEEK_SET);
	read(pcicfg_fd,&a,1);
	return a;
}

void intel_pcicfg_u8_write(uint32_t o,uint8_t c) {
	lseek(pcicfg_fd,o,SEEK_SET);
	write(pcicfg_fd,&c,1);
}

uint16_t intel_pcicfg_u16(uint32_t o) {
	uint16_t a=~0;
	lseek(pcicfg_fd,o,SEEK_SET);
	read(pcicfg_fd,&a,2);
	return a;
}

uint32_t intel_pcicfg_u32(uint32_t o) {
	uint32_t a=~0;
	lseek(pcicfg_fd,o,SEEK_SET);
	read(pcicfg_fd,&a,4);
	return a;
}

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	/* so... where is the current page table.
	 * NOTE: I've noticed my 855GM VESA BIOS sets up these page tables at top of RAM for me when using VESA BIOS extensions */
	uint32_t PAGE_CTL = MMIO(0x2020);
	uint32_t GTT_PHYSICAL = PAGE_CTL & (~0xFFF);
	int GTT_ENABLED = PAGE_CTL & 1;
	uint32_t GTT_SIZE = gtt_sizes[(PAGE_CTL >> 1) & 7];

	printf("current page tables:\n");
	printf("   physical: 0x%08X (~%uMB mark)\n",GTT_PHYSICAL,GTT_PHYSICAL>>20U);
	printf("    enabled: %d\n",GTT_ENABLED);
	printf("       size: 0x%08X (%uKB)\n",GTT_SIZE,GTT_SIZE>>10);
	if (GTT_SIZE == 0) return 1;

	if (!open_intel_pcicfg("0000:00:00.0"))
		return 1;

	/* how big */
	uint16_t GMCH_GCR = intel_pcicfg_u16(0x52);
	uint32_t framebuffer_sz = 0;
	switch ((GMCH_GCR >> 4) & 7) {
		case 1: framebuffer_sz = 1024*1024; break;
		case 2: framebuffer_sz = 4*1024*1024; break;
		case 3: framebuffer_sz = 8*1024*1024; break;
		case 4: framebuffer_sz = 16*1024*1024; break;
		case 5: framebuffer_sz = 32*1024*1024; break;
	};

	printf("GCR(pci): 0x%04X\n",GMCH_GCR);
	printf("    framebuffer size: %uMB (0x%08X)\n",framebuffer_sz>>20,framebuffer_sz);
	printf("    IGD disable: %u\n",(GMCH_GCR >> 1) & 1);

	uint16_t DAFC = intel_pcicfg_u16(0x54);
	printf("DAFC: 0x%04X\n",DAFC);
	printf("    device #2 disable: %d\n",(DAFC >> 7) & 1);
	printf("    dev 0 func 3 disable: %d\n",(DAFC >> 2) & 1);
	printf("    dev 0 func 1 disable: %d\n",DAFC & 1);

	uint16_t FDHC = intel_pcicfg_u16(0x58);
	printf("FDHC: 0x%04X\n",FDHC);
	printf("    15-16MB ISA hole enabled: %d\n",(FDHC >> 7) & 1);

	uint32_t ATTBASE = intel_pcicfg_u32(0xB8);
	printf("ATTBASE: 0x%08X\n",ATTBASE);

	/* okay, map it and dump it */
	/* note: the VESA BIOS on 855GM cards likes to put it in the SMM area where we normally can't see it.
	 *       so we have to flip switches to make the SMM TSEG area visible */
	uint8_t SMRAM = intel_pcicfg_u8(0x60);
	printf("SMRAM: 0x%02X\n",SMRAM);
	if (!(SMRAM & 0x40)) {
		printf("Opening SMRAM in case it's stored there\n");
		intel_pcicfg_u8_write(0x60,0x40);
	}

	uint32_t tal = GTT_PHYSICAL;
	printf("0x%08X\n",tal);
	volatile uint32_t *GTT = map_physical(tal,GTT_SIZE);
	if (GTT == NULL) {
		printf("Cannot map GTT\n");
		return 1;
	}

	unsigned int page,pages = (GTT_SIZE >> 2U);
	for (page=0;page < pages;page++) {
		uint32_t ent = GTT[page];
		printf("+0x%08X: 0x%08X\n",page << 12U,ent);
	}

	close_intel_pcicfg();
	unmap_page(GTT,GTT_SIZE);
	unmap_intel_resources();
	mem_close();
	return 0;
}

