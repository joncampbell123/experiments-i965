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

int main() {
	iopl(3);

	fb_base_vis = 0xd0000000;
	fb_size_vis = 256*1024*1024;
	fb_base_mmio = 0xfc000000;
	fb_size_mmio = 1024*1024;

	int tty_fd = open("/dev/tty0",O_RDWR);
	ioctl(tty_fd,KDSETMODE,KD_GRAPHICS);
	close(tty_fd);

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

	{
		unsigned short *p = (unsigned short*)fb_base;
		unsigned short *f = p + 1280*800;
		int x=0,o=0,c=0,y=0;
		while (p < f) {
			int g = (x >> 5) & 0x3F;
			c = (x & 0x1F) | ((y & 0x1F) << 11) | (g << 5);
			*p++ = o & 1 ? c : c;
			if (++x >= 1280) {
				x -= 1280;
				y++;
			}
		}
	}

	{
		MMIO(0x71180) = 0x80000000 | (5 << 26) | (0 << 20); /* enable, 16bpp format, no pixel doubling */
		MMIO(0x71184) = 0; /* start at zero */
		MMIO(0x71188) = 1280*2;
		MMIO(0x7119C) = 0; /* this causes the change to occur */
	}

	munmap(fb_base_mmio,fb_size_mmio);
	munmap(fb_base_vis,fb_size_vis);
	close(mem_fd);
	return 0;
}

