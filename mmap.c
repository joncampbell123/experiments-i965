#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include <sys/io.h>

#include "mmap.h"
#include "find_intel.h"

int mem_fd = -1;
unsigned char *fb_base = NULL;
volatile uint32_t *fb_mmio = NULL;
int self_pagemap_fd = -1;
int pcicfg_fd = -1;

int unmap_intel_resources() {
	if (fb_mmio) {
		munmap((void*)fb_mmio,fb_size_mmio);
		fb_mmio = NULL;
	}
	if (fb_base) {
		munmap(fb_base,fb_size_vis);
		fb_base = NULL;
	}

	if (mem_fd >= 0) {
		close(mem_fd);
		mem_fd = -1;
	}

	return 1;
}

int map_intel_resources() {
	if ((mem_fd = open("/dev/mem",O_RDWR)) < 0) {
		fprintf(stderr,"Cannot open /dev/mem\n");
		return 0;
	}

	fb_base = (unsigned char*)mmap(NULL,fb_size_vis,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,fb_base_vis);
	if (fb_base == (unsigned char*)(-1)) {
		unmap_intel_resources();
		fprintf(stderr,"Cannot mmap framebuffer\n");
		return 0;
	}

	fb_mmio = (uint32_t*)mmap(NULL,fb_size_mmio,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,fb_base_mmio);
	if (fb_mmio == (volatile uint32_t*)(-1)) {
		unmap_intel_resources();
		fprintf(stderr,"Cannot mmap MMIO\n");
		return 0;
	}

	return 1;
}

size_t virt2phys(void *p) {
	if (self_pagemap_fd < 0)
		self_pagemap_fd = open("/proc/self/pagemap",O_RDONLY);
	if (self_pagemap_fd < 0) {
		printf("virt2phys cannot open pagemap\n");
		return -1LL;
	}

	uint64_t u;
	unsigned long virt = (unsigned long)p;
	unsigned long ofs = (virt >> 12ULL) << 3UL;
	if (lseek(self_pagemap_fd,ofs,SEEK_SET) != ofs) {
		printf("virt2phys: cannot lseek()\n");
		return -1LL;
	}
	if (read(self_pagemap_fd,&u,8) != 8) {
		printf("virt2phys: cannot read()\n");
		return -1LL;
	}

	if (!(u & (1ULL << 63ULL)))
		return -1LL;	/* not present */
	if (u & (1ULL << 62LL))
		return -1LL;	/* swapped out */

	return (u & ((1ULL << 56ULL) - 1ULL)) << 12;
}

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

