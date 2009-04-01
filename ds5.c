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

#include "find_intel.h"
#include "util.h"

int mem_fd = -1;
unsigned char *fb_base;
uint32_t *fb_mmio;

#define MMIO(x) fb_mmio[(x)>>2]

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

	MMIO(0x700C0) = (1 << 28); /* cursor B to pipe B, 64x64 4-color */
	MMIO(0x7119C) = 0;
	usleep(1000000/5);
	printf("OK\n");

	/* play with the cursor! */
	/* warning this isn't reliable. */
	MMIO(0x700C0) = (1 << 28) | 0x20 | 3; /* cursor B to pipe B, 256x256 ARGB */
	MMIO(0x700C8) = (0 << 16) | 0; /* at (0,0) */
	MMIO(0x700C4) = 0x400000;      /* follow the buffer */
	MMIO(0x7119C) = 0;

	unsigned char *cursor = fb_base + 0x400000;
	{
		int x;
		unsigned int *p_xor = (unsigned int*)cursor;

		for (x=0;x < 256*256;x++) {
			int a = x >> 8;
			p_xor[x] = mult_rgb(x,a) | (a << 24);
		}
	}

	{
		int x;
		for (x=0;x < 512;x++) {
			MMIO(0x700C8) = (x << 16) | x; /* at (0,0) */
			usleep(1000000/60);
		}
	}

	munmap(fb_base_mmio,fb_size_mmio);
	munmap(fb_base_vis,fb_size_vis);
	close(mem_fd);

	if (0) {
		int tty_fd = open("/dev/tty0",O_RDWR);
		ioctl(tty_fd,KDSETMODE,KD_TEXT);
		close(tty_fd);
	}

	return 0;
}

