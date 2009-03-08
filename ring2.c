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

//#define MI_NOOP		0x00000000

int mem_fd = -1;
unsigned long long fb_base_vis=0;
unsigned long long fb_base_mmio=0;
unsigned long long fb_size_vis=0;
unsigned long long fb_size_mmio=0;

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

double frtime() {
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return (((double)tv.tv_sec) + ((double)tv.tv_usec)/1000000);
}

int main() {
	iopl(3);

	fb_base_vis = 0xd0000000;
	fb_size_vis = 256*1024*1024;
	fb_base_mmio = 0xfc000000;
	fb_size_mmio = 1024*1024;

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
	 * to avoid a race with the 2D ring we stop the ring, fill with commands, then start it again. */
	stop_ring();
	{
		unsigned int c,cmax=((ring_size-16384)/4);
		for (c=0;c <= cmax;c++) {	/* how many we can fill up before hitting the end of the ring */
			mi_noop_id(c);
		}
		fill_no_ops(4);	/* 4 no-ops as landing area */
		ring_emit_finish();

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

