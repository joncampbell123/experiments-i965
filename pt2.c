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

#include <asm/mtrr.h>

const int seizure_mode = 1;

int mtrr_fd = -1;

int GTT_PAGES = 0;
uint32_t GTT_OFFSET = 0;
volatile uint32_t *GTT = NULL,*GTT_FENCE = NULL;

int self_pagemap_fd = -1;
int64_t virt2phys(void *p) {
	if (self_pagemap_fd < 0)
		self_pagemap_fd = open("/proc/self/pagemap",O_RDONLY);
	if (self_pagemap_fd < 0)
		return -1LL;

	uint64_t u;
	unsigned int virt = (unsigned int)p;
	unsigned int ofs = (virt >> 12) << 3;
	if (lseek(self_pagemap_fd,ofs,SEEK_SET) != ofs)
		return -1LL;
	if (read(self_pagemap_fd,&u,8) != 8)
		return -1LL;

	if (!(u & (1ULL << 63ULL)))
		return -1LL;	/* not present */
	if (u & (1ULL << 62LL))
		return -1LL;	/* swapped out */

	return (u & ((1ULL << 56ULL) - 1ULL)) << 12;
}

/* our sub-bitmap! */
volatile uint16_t *sub_bitmap = NULL;

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	if ((mtrr_fd = open("/proc/mtrr",O_RDWR)) < 0) {
		printf("Cannot open MTRRs\n");
		return 1;
	}

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

	/* conveniently, we can also assume Intel 855GM VESA BIOS behavior of sticking the page tables IT made at the
	 * end of the framebuffer area */
	/* TODO: get the page tables size */
	uint32_t page_tables_size = 128U << 10U;
	uint32_t vesa_bios_page_tables = framebuffer_addr + stolen - page_tables_size;
	printf("VESA BIOS page tables = 0x%08X\n",vesa_bios_page_tables);

	/* in case of messed-up tables, we do some very careful stepping around to restore sanity.
	 * first: we mmap() 4K of memory and mlock it, make that just enough of a table to cover
	 * maybe the first 4MB mapping (and we HOPE that nobody else attempts to use it!) */
	int page;
	volatile uint32_t *step1 = mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_SHARED,-1,0);
	volatile uint32_t *step2;
	{
		if (step1 == ((uint32_t*)-1)) {
			printf("Cannot mmap 4K\n");
			return 1;
		}
		if (mlock((void*)step1,4096) < 0) {
			printf("Cannot mlock page into memory\n");
			return 1;
		}
		uint32_t step1_cpuaddr = (uint32_t)virt2phys((void*)step1);
		if (step1_cpuaddr == ~0) {
			printf("Cannot translate to physical CPU addr\n");
			return 1;
		}

		printf("Step1 page:\n");
		printf("    Virtual: 0x%08X\n",(uint32_t)step1);
		printf("    Physical: 0x%08X\n",step1_cpuaddr);

		/* use the MTRRs to make our page uncacheable to ensure that the damn processor
		 * get the data out there the instant we write it. if we don't the processor will
		 * take it's sweet lazy time and you'll see it in the form of a screen full of
		 * garbage that slowly builds up over time (as the CPU flushes, of course!) */
		struct mtrr_sentry se;
		memset(&se,0,sizeof(se));
		se.base = step1_cpuaddr;
		se.size = 4096;
		if (ioctl(mtrr_fd,MTRRIOC_ADD_ENTRY,&se) < 0) {
			printf("Cannot add MTRR entry\n");
			return 1;
		}

		int fb_pages = (1280*768*2)>>12,c;
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

		if (ioctl(mtrr_fd,MTRRIOC_DEL_ENTRY,&se) < 0)
			printf("Warning: cannot kill MTRR entry\n");

		MMIO(0x2020) = step1_cpuaddr | 1;

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

	/* make that one active so we have our sanity */
	MMIO(0x2020) = vesa_bios_page_tables | 1;

	/* we can let go the 4K page now */
	munmap((void*)step1,4096);

	/* alloc subbitmap */
	sub_bitmap = (volatile uint16_t*)mmap(NULL,640*480*2,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_SHARED,-1,0);
	if (sub_bitmap == (void*)-1) {
		printf("Cannot alloc shared bitmap\n");
		return 1;
	}
	if (mlock((void*)sub_bitmap,640*480*2) < 0) {
		printf("Cannot lock bitmap\n");
		return 1;
	}
	memset(sub_bitmap,0x55,640*480*2);

	uint32_t sub_bitmap_fb_ofs;
	/* set up a third and final page table to enable blitting from host memory like we want */
	uint32_t gtt_fb_ofs = 0x500000;
	volatile uint32_t *GTT = (volatile uint32_t*)((char*)fb_base + gtt_fb_ofs);
	{
		int page,c;
		for (page=0;page < (0x500000 >> 12);page++)
			GTT[page] = (framebuffer_addr + (page << 12UL)) | 1;

		sub_bitmap_fb_ofs = page << 12U;
		for (c=0;c < ((640*480*2)>>12);c++,page++)
			GTT[page] = virt2phys(((char*)sub_bitmap) + (page << 12ULL)) | (3 << 1) | 1;
	}
	MMIO(0x2020) = (framebuffer_addr + gtt_fb_ofs) | 1;

	printf("sub bitmap bound to 0x%08X\n",sub_bitmap_fb_ofs);

	/* I pick the 2MB mark for the ring buffer. Use larger space for speed tests, about slightly less than 2MB */
	set_ring_area(0x200000,2040*1024);
	start_ring();
	make_gradient_test_cursor_argb(fb_base + 0x400000);

	fprintf(stderr,"Booting 2D ringbuffer.\n");

	/* insert no-ops that change NOPID. if the NOPID becomes the value we wrote, consider the test successful. */
	uint32_t v = ((uint32_t)((rand()*rand()*rand()))) & 0x1FFFFF;
	mi_noop_id(v);
	fill_no_ops(1);
	ring_emit_finish();

	int counter = 0;
	while (read_nopid() != v) {
		if (++counter == 1000000) {
			fprintf(stderr,"Test failed. Ring buffer failed to start.\n");
			fprintf(stderr,"Wrote 0x%08X, read back 0x%08X\n",v,read_nopid());
			return 1;
		}
	}
	printf("Ringbuffer hit NOPID no-op in %u counts. Success.\n",counter);

	/* now fill with no-ops and see how fast it really goes.
	 * this time we tell it to wait for vblank, so the pipeline will be waiting around a lot.
	 * and hey, maybe this might be a good way to measure the vertical sync rate.
	 * to avoid a race with the 2D ring we stop the ring, fill with commands, then start it again. */
	stop_ring();
	{
		unsigned int c,cmax=0;
		/* prep the cursors */
		mi_load_imm(0x70080,1 << 28);
		mi_load_imm(0x700C0,1 << 28);
		mi_load_imm(0x70084,0x400000);
		mi_load_imm(0x700C4,0x400000);
		mi_load_imm(0x71184,0);
		mi_load_imm(0x7119C,0);
		start_ring();
		/* animate */
		for (c=0;c <= cmax;c++) {	/* how many we can fill up before hitting the end of the ring */
			int x1,x2,y1,y2;
			int cx = 640,cy = 360;
			double a = (((double)c) * 6.28) / 1000;
			x1 = cx + (sin(a) * 480);
			y1 = cy + (cos(a) * 240);
			x2 = cx + (sin(a*5) * 360);
			y2 = cy + (cos(a*5) * 180);
			if (seizure_mode) { }
			else if (intel_device_chip == INTEL_965)
				ring_emit((3 << 23) | (1 << 18)); /* pipe B: wait for HBLANK */
			else
				ring_emit((3 << 23) | (1 << 3)); /* pipe B: wait for HBLANK */

			mi_load_imm(0x700C0,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x700C8,(y1 << 16) | x1);
			mi_load_imm(0x70080,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x70088,(y2 << 16) | x2);
			/* blit from host memory */
			src_copy_blit(
				0,	/* dest */
				640,480,	/* 320x240 block */
				1280*2,		/* dest pitch */
				sub_bitmap_fb_ofs,	/* src */
				640*2);
			ring_emit_finish();
			wait_ring_space(64);
		}
		wait_ring_space((ring_size>>2)-128);
	}

	stop_ring();
	MMIO(0x2020) = vesa_bios_page_tables | 1;
	unmap_intel_resources();
	close(mtrr_fd); mtrr_fd = -1;
	return 0;
}

