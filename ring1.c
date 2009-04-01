#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
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

int DIE=0;

void sigma(int x) {
	if (++DIE >= 3)
		exit(1);
}

int main() {
	iopl(3);
	signal(SIGINT,sigma);
	signal(SIGTERM,sigma);
	signal(SIGQUIT,sigma);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	/* I pick the 2MB mark for the ring buffer :) */
	set_ring_area(0x200000,4096);
	start_ring();

	fprintf(stderr,"Booting 2D ringbuffer\n");

	/* play with the ring buffer. put no-ops that modify the NOPID */
	{
		uint32_t x = 0;
		int patience = 0;
		int count=0;

		/* wait for "1" to appear in NOPID */
		while (DIE == 0) {
			/* write 8 no-ops that change NOPID. you can see the results on the console.
			 * also, you can make sure it wraps around properly. Fun, huh? */
			mi_noop_id(rand());
			mi_noop_id(rand());
			mi_noop_id(rand());
			mi_noop_id(rand());
			mi_noop_id(rand());
			mi_noop_id(rand());
			mi_noop_id(rand());
			mi_noop_id(rand());

			ring_emit_finish();
			printf("head=0x%08X cur=0x%08X start=0x%08X err=0x%08X done=0x%08X id=0x%08X ipsr=0x%08X done2=0x%08X\n",MMIO(0x2030),MMIO(0x2034),MMIO(0x2038),MMIO(0x2088),MMIO(0x2090),MMIO(0x2094),MMIO(0x20C4),MMIO(0x206C));
			usleep(100000);
			count++;
		}
		printf("First NOP_ID took %d\n",count);
	}

	stop_ring();
	unmap_intel_resources();
	return 0;
}

