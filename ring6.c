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

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	/* I pick the 2MB mark for the ring buffer. Use larger space for speed tests, about slightly less than 2MB */
	set_ring_area(0x200000,2040*1024);
	start_ring();

	unsigned char *cursor = fb_base + 0x400000;
	{
		int x;
		unsigned int *p_xor = (unsigned int*)cursor;

		for (x=0;x < 256*256;x++) {
			int a = x >> 8;
			p_xor[x] = mult_rgb(x,a) | (a << 24);
		}
	}

#if 0
	{
		int i;
		for (i=0;i < 512*4;i++) {
			double a = (((double)i) * 3.1415926 * 2) / 512;

			int x = 640 + (int)(sin(a)*320);
			int y = 400 + (int)(cos(a)*200);
			MMIO(0x700C8) = (y << 16) | x; /* at (0,0) */

			int x2 = 640 + (int)(sin(a*1.1)*320);
			int y2 = 400 + (int)(cos(a*1.1)*200);
			MMIO(0x70088) = (y2 << 16) | x2; /* at (0,0) */

			usleep(1000000/60);
		}
	}
#endif

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
			ring_emit((3 << 23) | (1 << 18)); /* pipe B: wait for HBLANK */
			mi_load_imm(0x700C0,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x700C8,(y1 << 16) | x1);
			mi_load_imm(0x70080,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x70088,(y2 << 16) | x2);
			/* fun with the COLOR_BLIT */
			color_blit_fill((1280*2*4)+(4*2), /* start at 2nd scan line */
				160,120,	/* 640x480 block */
				1280*2,		/* pitch */
				c);		/* what to fill with */
			src_copy_blit(
				(1280*2*4)+(170*2),	/* dest */
				320,240,	/* 320x240 block */
				1280*2,		/* dest pitch */
				(1280*2*300)+(80*2),	/* src */
				1280*2);
			ring_emit_finish();
			wait_ring_space(64);
		}
		wait_ring_space((ring_size>>2)-16);
	}

	stop_ring();
	unmap_intel_resources();
	return 0;
}

