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

int mem_fd = -1;
unsigned char *fb_base;
uint32_t *fb_mmio;

#define MMIO(x) fb_mmio[(x)>>2]

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;

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
		unsigned int pval = -1;
		while (1) {
			unsigned int nval = (MMIO(0x71044) >> 24) | ((MMIO(0x71040) & 0xFFFF) << 8); /* watch the page counter */
			if (pval != nval) {
				printf("0x%08X\n",nval);
				pval = nval;
			}
		}
	}

	munmap(fb_base_mmio,fb_size_mmio);
	munmap(fb_base_vis,fb_size_vis);
	close(mem_fd);
	return 0;
}

