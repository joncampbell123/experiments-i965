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

const int seizure_mode = 0;

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

#if 0
uint32_t ptr_to_fb(volatile void *x) {
	return (uint32_t)(((volatile unsigned char*)(x)) - fb_base);
}
#endif

/* a buffer for the Graphics chip to blit from.
 * we own it in CPU space, and if programmed correctly, the graphics chipset will blit from it perfectly */

void disable_gtt() {
	MMIO(0x2020) = 0;
}

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	disable_gtt();

	/* and pick the 4MB mark for the GTT */
	GTT_OFFSET = (4UL << 20UL);
	GTT = (volatile uint32_t*)(fb_base + GTT_OFFSET);
	GTT_PAGES = (2UL << 20UL) >> 2UL;
	GTT_FENCE = GTT + GTT_PAGES;	/* and we choose 2MB GTT */
	/* make default map: first 8MB is mapped directly */
	{
		unsigned int page = 0;
		for (page=0;page < GTT_PAGES;page++)
			GTT[page] = ((page << 12UL) + GTT_OFFSET + fb_base_vis) | (0 << 1) | 1;	/* 1:1 mapping of pages, main memory (meaning graphics), valid */
	}
	/* set it */
	{
		uint32_t align = (GTT_PAGES << 2UL);
		if (align & (align - 1) == 0) { printf("0x%08X not power of 2\n",align); return 1; }
		uint32_t ofs = GTT_OFFSET;
		if ((ofs & align) != 0) { printf("0x%08X not aligned\n",ofs); return 1; }
		if ((ofs & 0xFFF) != 0) { printf("0x%08X not page aligned\n",ofs); return 1; }
		MMIO(0x2020) = (ofs + fb_base_vis) | (4 << 1) | 1;	/* offset, 2MB GTT, enabled */
	}

	usleep(500000);
	disable_gtt();
	return 1;

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
		unsigned int c,cmax=1000000;
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
			/* fun with the COLOR_BLIT */
			src_copy_blit(
				(1280*2*1)+(1*2),	/* dest */
				1200,700,	/* 320x240 block */
				1280*2,		/* dest pitch */
				(1280*2*2)+(0*2),	/* src */
				1280*2);
			color_blit_fill((1280*2*698)+(0*2), /* start at 2nd scan line */
				1000,2,	/* 640x480 block */
				1280*2,		/* pitch */
				c);		/* what to fill with */
			ring_emit_finish();
			wait_ring_space(64);
		}
		wait_ring_space((ring_size>>2)-16);
	}

	stop_ring();
	disable_gtt();
	unmap_intel_resources();
	return 0;
}

