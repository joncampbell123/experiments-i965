/* fun with the Intel Graphics Page Tables, yay! :) */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
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

#include <asm/mtrr.h>

const int seizure_mode = 0;

int mtrr_fd = -1;

void mtrr_init_entry(struct mtrr_sentry *se) {
	memset(se,0,sizeof(*se));
}

void mtrr_set(struct mtrr_sentry *se,size_t base,size_t size,int type) {
	se->base = base;
	se->size = size;
	se->type = type;
}

int mtrr_add(struct mtrr_sentry *se) {
	return ioctl(mtrr_fd,MTRRIOC_ADD_ENTRY,&se);
}

int mtrr_del(struct mtrr_sentry *se) {
	return ioctl(mtrr_fd,MTRRIOC_DEL_ENTRY,&se);
}

int mtrr_init() {
	if (mtrr_fd >= 0) return 1;
	if ((mtrr_fd = open("/proc/mtrr",O_RDWR)) < 0) return 0;
	return 1;
}

void mtrr_close() {
	if (mtrr_fd >= 0) close(mtrr_fd);
	mtrr_fd = -1;
}

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;
	if (!mtrr_init()) {
		printf("Cannot open MTRRs\n");
		return 1;
	}
#if 0
	if (!open_intel_pcicfg(sysfs_intel_graphics_dev)) {
		printf("Cannot open PCI config space\n");
		return 1;
	}
#endif

#if 0 /* Intel 855GM */
	/* TODO: ask an intel chipset where the actual top of memory is,
	 *       or else guess from Linux /proc/mem and round up. */
	uint32_t top_of_memory = (512U << 20U);
	printf("top of memory = 0x%08X\n",top_of_memory);
	/* we can assume this for now for most Intel chipsets: the TSEG area (1MB) */
	uint32_t TSEG = top_of_memory - 0x100000;
	printf("TSEG = 0x%08X\n",TSEG);
	/* TODO: ask the chipset what the "framebuffer" size is (meaning how large the "stolen" area is) */
	uint32_t stolen = (8U << 20U);
	printf("Stolen = 0x%08X\n",stolen);
	uint32_t framebuffer_addr = TSEG - stolen;
	printf("framebuffer (DRAM physical) = 0x%08X\n",framebuffer_addr);
#endif
#if 1 /* Intel 965 */
	/* Intel seems to have better documentation on this than the 855
	 * I was testing against */

	/* we need PCI configuration access */
	if (!open_intel_pcicfg("0000:00:00.0")) {
		printf("Cannot open host bridge config space\n");
		return 1;
	}
	uint32_t top_of_memory = intel_pcicfg_u16(0xA0) << 27;
	printf("top of memory = 0x%08X (%dMB)\n",top_of_memory,top_of_memory>>20);
	uint32_t tolud = intel_pcicfg_u16(0xB0) << 16;
	printf("top of usable dram = 0x%08X\n",tolud);
	close_intel_pcicfg();

	if (!open_intel_pcicfg("0000:00:02.0")) {
		printf("Cannot open VGA config space\n");
		return 1;
	}

	uint32_t top_of_stolen_memory = intel_pcicfg_u32(0x5C);
	printf("Top of stolen = 0x%08X\n",top_of_stolen_memory);

	uint32_t ctrl_info = intel_pcicfg_u16(0x52);
	static uint32_t stolt[8] = {
		0,
		1UL << 20UL,
		4UL << 20UL,
		8UL << 20UL,
		16UL << 20UL,
		32UL << 20UL,
		48UL << 20UL,
		64UL << 20UL
	};

	uint32_t stolen = stolt[(ctrl_info >> 4) & 7];
	printf("Stolen = 0x%08X\n",stolen);
	uint32_t framebuffer_addr = top_of_stolen_memory;
	printf("framebuffer (DRAM physical) = 0x%08X\n",framebuffer_addr);
#endif

	/* conveniently, we can also assume Intel 855GM VESA BIOS behavior of sticking the page tables IT made at the
	 * end of the framebuffer area */
	/* TODO: get the page tables size */
	uint32_t page_tables_size = 512U << 10U; /* 512KB */
	uint32_t vesa_bios_page_tables = framebuffer_addr + stolen - page_tables_size;
	printf("VESA BIOS page tables = 0x%08X\n",vesa_bios_page_tables);

	/* in case of messed-up tables, we do some very careful stepping around to restore sanity.
	 * first: we mmap() 4K of memory and mlock it, make that just enough of a table to cover
	 * maybe the first 4MB mapping (and we HOPE that nobody else attempts to use it!) */
	int page;
	volatile uint32_t *step1 = mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_SHARED,-1,0);
	volatile uint32_t *step2;
	{
		if (step1 == ((volatile uint32_t*)-1)) {
			printf("Cannot mmap 4K\n");
			return 1;
		}
		if (mlock((void*)step1,4096) < 0) {
			printf("Cannot mlock page into memory\n");
			return 1;
		}
		size_t step1_cpuaddr = (size_t)virt2phys((void*)step1);
		if (step1_cpuaddr == ~(0ULL)) {
			printf("Cannot translate to physical CPU addr\n");
			return 1;
		}

		printf("Step1 page:\n");
		printf("    Virtual: 0x%08lX\n",(size_t)step1);
		printf("    Physical: 0x%08lX\n",step1_cpuaddr);

		/* use the MTRRs to make our page uncacheable to ensure that the damn processor
		 * get the data out there the instant we write it. if we don't the processor will
		 * take it's sweet lazy time and you'll see it in the form of a screen full of
		 * garbage that slowly builds up over time (as the CPU flushes, of course!) */
#if 0
		struct mtrr_sentry se;

		se.base = 0;
		se.size = top_of_memory;
		se.type = MTRR_TYPE_WRBACK;
		mtrr_del(&se);

		mtrr_init_entry(&se);
		mtrr_set(&se,step1_cpuaddr,4096,MTRR_TYPE_UNCACHABLE);
		if (mtrr_add(&se) < 0) {
			printf("Cannot add MTRR entry %s\n",strerror(errno));
			return 1;
		}
#endif

		int fb_pages = (1280*800*2)>>12,c;
		for (page=0;page < fb_pages;page++)
			step1[page] = (framebuffer_addr + (page << 12UL)) | 1;

		/* and then some pages to allow us to access the VESA BIOS's table.
		 * so we can rebuild it in case it got trashed. this makes soft-booting safe again too;
		 * the VESA BIOS seems to assume it can build them once and lazily re-use them every
		 * time Linux uvesafb calls INT 10H, therefore if the tables are trashed the VESA BIOS
		 * will seriously fuck up the system trying to handle INT 10H and cause hang, crash,
		 * reboot, or severe data corruption! */
		uint32_t vesa_mapped_offset = page << 12UL;
		for (c=0;c < (page_tables_size>>12);c++,page++)
			step1[page] = (vesa_bios_page_tables + (c << 12UL)) | 1;
		/* and the rest point nowhere */
		for (;page < 1024;page++)
			step1[page] = framebuffer_addr | 1;

#if 0
		if (mtrr_del(&se) < 0)
			printf("Warning: cannot kill MTRR entry\n");
#endif

		MMIO(0x2020) = step1_cpuaddr | 1;

		sleep(10);

		/* and then, use the mapping we made for ourselves to access what the VESA BIOS uses for page tables */
		step2 = (volatile uint32_t*)(fb_base + vesa_mapped_offset);
	}

	/* rebuild the VESA BIOS copy of page tables. mimic the mapping-out of the tables the VESA BIOS does. */
	{
		int page,pages = (page_tables_size >> 2);
		int map_pages = (stolen - page_tables_size) >> 12;
		for (page=0;page < map_pages;page++)
			step2[page] = (framebuffer_addr + (page << 12UL)) | 1;
		for (;page < pages;page++)
			step2[page] = (framebuffer_addr + ((map_pages-1) << 12UL)) | 1;
	}

	MMIO(0x2020) = vesa_bios_page_tables | 1;

	unmap_intel_resources();
	mtrr_close();
	return 0;
}

