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

#include "mmap.h"
#include "find_intel.h"

int mem_fd = -1;
unsigned char *fb_base = NULL;
volatile uint32_t *fb_mmio = NULL;

int unmap_intel_resources() {
	if (fb_mmio) {
		munmap((void*)fb_mmio,fb_size_mmio);
		fb_mmio = NULL;
	}
	if (fb_base) {
		munmap(fb_base,fb_size_vis);
		fb_base = NULL;
	}

	if (mem_fd >= 0) {
		close(mem_fd);
		mem_fd = -1;
	}

	return 1;
}

int map_intel_resources() {
	if ((mem_fd = open("/dev/mem",O_RDWR)) < 0) {
		fprintf(stderr,"Cannot open /dev/mem\n");
		return 0;
	}

	fb_base = (unsigned char*)mmap(NULL,fb_size_vis,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,fb_base_vis);
	if (fb_base == (unsigned char*)(-1)) {
		unmap_intel_resources();
		fprintf(stderr,"Cannot mmap framebuffer\n");
		return 0;
	}

	fb_mmio = (uint32_t*)mmap(NULL,fb_size_mmio,PROT_READ|PROT_WRITE,MAP_SHARED,mem_fd,fb_base_mmio);
	if (fb_mmio == (volatile uint32_t*)(-1)) {
		unmap_intel_resources();
		fprintf(stderr,"Cannot mmap MMIO\n");
		return 0;
	}

	return 1;
}

