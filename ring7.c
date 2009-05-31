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

#include "intelfbhw.h"
#include "find_intel.h"
#include "ringbuffer.h"
#include "util.h"
#include "mmap.h"

#include "tvbox_i8xx.h"
#include "pgtable.h"
#include "uvma.h"

static int DIE = 0;

void sigma(int x) {
	if (++DIE >= 5)
		_exit(0);
}

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;
	if (!open_i8xx_pgtable())
		return 1;
	if (!open_uvirt_to_phys())
		return 1;

	/* I pick the 2MB mark for the ring buffer. Use larger space for speed tests, about slightly less than 2MB */
	set_ring_area(0x200000,2040*1024);
	start_ring();

	fprintf(stderr,"Booting 2D ringbuffer.\n");

	/* insert no-ops that change NOPID. if the NOPID becomes the value we wrote, consider the test successful. */
	uint32_t v = ((uint32_t)((rand()*rand()*rand()))) & 0x1FFFFF;
	mi_noop_id(v);
	fill_no_ops(1);
	ring_emit_finish();

	int counter = 0;
	while (!DIE && read_nopid() != v) {
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
		unsigned int c,cmax=500;
		for (c=0;c <= cmax;c++) {	/* how many we can fill up before hitting the end of the ring */
			ring_emit((3 << 23) | (1 << 18)); /* pipe B: wait for VBLANK */
			mi_noop_id(c);
		}
		fill_no_ops(4);	/* 4 no-ops as landing area */
		ring_emit_finish();

		double fstart = frtime();
		double upd = fstart;
		unsigned int lnop,county=0;
		start_ring(); /* GO! */
		while (!DIE && (lnop = read_nopid()) != cmax) {
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
	restore_i8xx_pgtable();
	close_i8xx_pgtable();
	close_uvirt_to_phys();
	unmap_intel_resources();
	return 0;
}

