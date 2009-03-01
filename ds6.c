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

int mem_fd = -1;
unsigned long long fb_base_vis=0;
unsigned long long fb_base_mmio=0;
unsigned long long fb_size_vis=0;
unsigned long long fb_size_mmio=0;

unsigned char *fb_base;
uint32_t *fb_mmio;

#define MMIO(x) fb_mmio[(x)>>2]

unsigned int mult_rgb(unsigned int x,int a) {
	int r = x & 0xFF;
	int g = (x >> 8) & 0xFF;
	int b = (x >> 16) & 0xFF;
	return	((r * a) >> 8) |
		(((g * a) >> 8) << 8) |
		(((b * a) >> 8) << 16);
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

	unsigned char *cursor = fb_base + 0x400000;
	{
		int x;
		unsigned int *p_xor = (unsigned int*)cursor;

		for (x=0;x < 256*256;x++) {
			int a = x >> 8;
			p_xor[x] = mult_rgb(x,a) | (a << 24);
		}
	}

	MMIO(0x70080) = (1 << 28); /* cursor A to pipe B, 64x64 4-color */
	MMIO(0x70084) = 0x400000;      /* follow the buffer */
	MMIO(0x7119C) = 0;

	MMIO(0x700C0) = (1 << 28); /* cursor B to pipe B, 64x64 4-color */
	MMIO(0x700C4) = 0x400000;      /* follow the buffer */
	MMIO(0x7119C) = 0;
	usleep(1000000/4);
	printf("OK\n");

	/* play with the cursor! */
	/* warning this isn't reliable. */
	MMIO(0x700C0) = (1 << 28) | 0x20 | 3; /* cursor B to pipe B, 256x256 ARGB */
	MMIO(0x700C8) = (0 << 16) | 0; /* at (0,0) */

	MMIO(0x70080) = (1 << 28) | 0x20 | 3; /* cursor A to pipe B, 256x256 ARGB */
	MMIO(0x70088) = (0 << 16) | 0; /* at (0,0) */

	MMIO(0x7119C) = 0;

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

