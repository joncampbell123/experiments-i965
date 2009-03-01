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
volatile uint32_t *fb_mmio;

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

	/* enable hotplug detect */
	MMIO(0x61110) = 0x00000000;
	MMIO(0x61114) = ~0;
	MMIO(0x61110) = 0x00000220;
	usleep(1000000/3);

	printf("CRT/VGA hotplug detection enabled. Unplug the VGA cable if connected, or connect the VGA cable if not connected\n");
	printf("0x%08X\n",MMIO(0x61114));

	unsigned int xx;
	while ((MMIO(0x61114) & (1 << 11)) == 0);
	printf("Saw it!\n");
	printf("0x%08X\n",xx = MMIO(0x61114));
	MMIO(0x61114) = ~0;

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

