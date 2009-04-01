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

//#define MI_NOOP		0x00000000

int mem_fd = -1;
unsigned char *fb_base;
volatile uint32_t *fb_mmio;

#define MMIO(x) *( (volatile uint32_t*) (((unsigned char*)fb_mmio) + (x)) )

unsigned int mult_rgb(unsigned int x,int a) {
	int r = x & 0xFF;
	int g = (x >> 8) & 0xFF;
	int b = (x >> 16) & 0xFF;
	return	((r * a) >> 8) |
		(((g * a) >> 8) << 8) |
		(((b * a) >> 8) << 16);
}

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

volatile uint32_t read_nopid() {
	return MMIO(0x2094);
}

void stop_ring() {
	MMIO(0x203C) = 0x00000000;
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

double frtime() {
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return (((double)tv.tv_sec) + ((double)tv.tv_usec)/1000000);
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

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;

	if (0) {
		int tty_fd = open("/dev/tty0",O_RDWR);
		ioctl(tty_fd,KDSETMODE,KD_GRAPHICS);
		close(tty_fd);
	}

	if ((mem_fd = open("/dev/mem",O_RDWR)) < 0) {
		fprintf(stderr,"Cannot open /dev/mem\n");
		return 1;
	}

	fb_base = (unsigned char*)mmap(NULL,fb_size_vis,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,fb_base_vis);
	if (fb_base == (unsigned char*)-1) {
		fprintf(stderr,"Cannot mmap framebuffer\n");
		return 1;
	}

	fb_mmio = (uint32_t*)mmap(NULL,fb_size_mmio,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,fb_base_mmio);
	if (fb_mmio == (unsigned char*)-1) {
		fprintf(stderr,"Cannot mmap MMIO\n");
		return 1;
	}

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

	munmap(fb_base_mmio,fb_size_mmio);
	munmap(fb_base_vis,fb_size_vis);
	close(mem_fd);

	if (1) {
		int tty_fd = open("/dev/tty0",O_RDWR);
		ioctl(tty_fd,KDSETMODE,KD_TEXT);
		close(tty_fd);
	}

	return 0;
}

