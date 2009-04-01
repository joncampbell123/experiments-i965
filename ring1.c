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
#include "util.h"
#include "mmap.h"

volatile uint32_t *ring_base,*ring_end,*ring_head,*ring_tail,ring_size;

uint32_t ptr_to_fb(volatile void *x) {
	return (uint32_t)(((volatile unsigned char*)(x)) - fb_base);
}

/* write dword to ring buffer. assumes caller verified this first */
void ring_emit(uint32_t dw) {
	*ring_tail++ = dw;
	if (ring_tail >= ring_end) ring_tail = ring_base;
}

/* write MI_NOOP with update to NOP_ID */
void mi_noop_id(uint32_t id) {
	ring_emit(MI_NOOP | (1 << 22) | (id & 0x1FFFFF));
}

/* finished emitting, i.e. we want to update the hardware's copy of the tail pointer */
void ring_emit_finish() {
	MMIO(0x2030) = ptr_to_fb(ring_tail);
}

/* fill ring buffer with no-ops. assumes caller verified first that buffer can handle that many */
void fill_no_ops(int x) {
	while (x-- > 0) ring_emit(MI_NOOP);
}

void set_ring_area(uint32_t base,uint32_t size) {
	/* WARNING: size must be multiple of 4096 */
	ring_size = (size + 4095) & (~4095);
	ring_base = (volatile uint32_t*)(fb_base + base);
	ring_end = (volatile uint32_t*)(((unsigned char*)ring_base) + ring_size);
	ring_head = ring_base;
	ring_tail = ring_base;
	fill_no_ops(64);	/* 64 no-ops cannot fill even 4096 bytes */

	MMIO(0x203C) = 0x00000000;				/* RING_CONTROL, disable ring */
	MMIO(0x2030) = ptr_to_fb(ring_tail) & 0x1FFFFC;		/* write RING_TAIL */
	MMIO(0x2034) = ptr_to_fb(ring_head) & 0x1FFFFC;		/* write RING_HEAD */
	MMIO(0x2038) = ptr_to_fb(ring_base) & 0xFFFFF000;	/* write RING_START */
}

void start_ring() {
	MMIO(0x203C) = (((ring_size >> 12) - 1) << 12) | (1 << 11) | 1;	/* set size, and enable */
}

void stop_ring() {
	MMIO(0x203C) = 0x00000000;
}

int main() {
	iopl(3);

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
		while (1) {
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
			printf("0x%08X 0x%08X\n",MMIO(0x2034),MMIO(0x2094));
			usleep(100000);
			count++;
		}
		printf("First NOP_ID took %d\n",count);
	}

	stop_ring();
	unmap_intel_resources();
	return 0;
}

