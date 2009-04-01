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
	MMIO(0x203C) = 0x00000000;				/* RING_CONTROL, disable ring */
	usleep(10000);

	/* WARNING: size must be multiple of 4096 */
	ring_size = (size + 4095) & (~4095);
	ring_base = (volatile uint32_t*)(fb_base + base);
	ring_end = (volatile uint32_t*)(((unsigned char*)ring_base) + ring_size);
	ring_head = ring_base;
	ring_tail = ring_base;
	fill_no_ops(64);	/* 64 no-ops cannot fill even 4096 bytes */

	MMIO(0x2030) = 0;					/* write RING_TAIL */
	MMIO(0x2034) = 0;					/* write RING_HEAD */
	MMIO(0x2038) = 0;					/* write RING_START */

	usleep(10000);

	MMIO(0x2030) = ptr_to_fb(ring_tail) & 0x1FFFFC;		/* write RING_TAIL */
	MMIO(0x2034) = ptr_to_fb(ring_head) & 0x1FFFFC;		/* write RING_HEAD */
	MMIO(0x2038) = ptr_to_fb(ring_base) & 0xFFFFF000;	/* write RING_START */
}

void start_ring() {
	MMIO(0x20C0) = 0;
	MMIO(0x203C) = (((ring_size >> 12) - 1) << 12) | (1 << 11) | 1;	/* set size, and enable */
}

void stop_ring() {
	MMIO(0x203C) = 0x00000000;
}

volatile uint32_t read_nopid() {
	return MMIO(0x2094);
}

void mi_load_imm(uint32_t what,uint32_t with) {
	ring_emit((0x22 << 23) | 1);	/* MI_LOAD_REGISTER_IMM with 1 DWORD and all byte enables on */
	ring_emit(what);
	ring_emit(with);
}

void color_blit_fill(uint32_t dest,int width,int height,int pitch,uint32_t val) {
	ring_emit((2 << 29) | (0x40 << 22) | (3 << 20) | 3);	/* 3 DWORDs extra, fill ARGB, COLOR_BLT */
	ring_emit((1 << 24) | (0xF0 << 16) | (pitch & 0xFFFF));		/* 16bpp 565 SRCCOPY */
	ring_emit((height << 16) | (width*2));
	ring_emit(dest);
	ring_emit(val);
}

void src_copy_blit(uint32_t dest,int dw,int dh,int dp,uint32_t src,int sp) {
	ring_emit((2 << 29) | (0x43 << 22) | (3 << 20) | 4);
	ring_emit((1 << 24) | (0xCC << 16) | (dp & 0xFFFF));	/* SRCCOPY 16bpp 565 */
	ring_emit((dh << 16) | (dh * 2));
	ring_emit(dest);
	ring_emit(sp);
	ring_emit(src);
}

void wait_ring_space(unsigned int x) {
	const int tot = ring_size >> 2;
	do {
		unsigned int head = MMIO(0x2034) & 0x1FFFFC;
		volatile uint32_t *hw_ptr = ring_base + (head >> 2);
		int spc = (int)(ring_tail - hw_ptr);
		if (spc < 0) spc += tot;
		int rem = tot - (spc + 1);
		if (rem >= x) break;
		usleep(10);
	} while (1);
}

