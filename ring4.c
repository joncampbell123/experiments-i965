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
		int stride = *(intel_hw_display_plane(DISPLAY_B,DISPLAY_PLANE_STRIDE));
		unsigned int c,cmax=10000;
		/* prep the cursors */
		mi_load_imm(0x70080,1 << 28);
		mi_load_imm(0x700C0,1 << 28);
		mi_load_imm(0x70084,0x400000);
		mi_load_imm(0x700C4,0x400000);
		/* animate */
		for (c=0;c <= cmax;c++) {	/* how many we can fill up before hitting the end of the ring */
			int x1,x2,y1,y2;
			int cx = 640,cy = 360;
			double a = (((double)c) * 6.28) / 500;
			x1 = cx + (sin(a) * 480);
			y1 = cy + (cos(a) * 240);
			x2 = cx + (sin(a*5) * 360);
			y2 = cy + (cos(a*5) * 180);
			if (intel_device_chip == INTEL_965)
				ring_emit((3 << 23) | (1 << 18)); /* pipe B: wait for VBLANK */
			else /* this works on my 855GM based laptop */
				ring_emit((3 << 23) | (1 << 3)); /* pipe B: wait for VBLANK */

			mi_load_imm(0x700C0,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x700C8,(y1 << 16) | x1);
			mi_load_imm(0x70080,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x70088,(y2 << 16) | x2);
			int py = c&255;
			if (py & 128) py = 256 - py;
			mi_load_imm(0x71184,py*stride);
			mi_load_imm(0x7119C,0);
			mi_noop_id(c);
		}
		fill_no_ops(4);	/* 4 no-ops as landing area */
		ring_emit_finish();

		printf("Muahaha. Oh yeah. If you CTRL+C this program, the animation will continue. That's the ring buffer in animation. Cool huh?\n");

		double fstart = frtime();
		double upd = fstart;
		unsigned int lnop,county=0;
		start_ring(); /* GO! */
		while ((lnop = read_nopid()) != cmax) {
			if (++county >= 10000) {
				county -= 10000;
				double cr = frtime();
				if (cr >= (upd+0.5)) {
					printf("%u/%u\n",lnop,cmax);
					upd = cr;
				}
			}
		}
		double fstop = frtime();

		double duration = fstop-fstart;
		fprintf(stderr,"Ring buffer ran through %u MI_NOOP commands in %.6f seconds\n",cmax,duration);
		fprintf(stderr,"That's %.3f instructions/sec\n",((double)cmax)/duration);
	}

	stop_ring();
	unmap_intel_resources();
	return 0;
}

