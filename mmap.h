#ifndef INTEL_MMAP_H
#define INTEL_MMAP_H

#include <stdint.h>
#include "intelfbhw.h"

extern int mem_fd;
extern unsigned char *fb_base;
extern volatile uint32_t *fb_mmio;

#define MMIOp(x) ( (volatile uint32_t*) (((volatile unsigned char*)fb_mmio) + (x)) )
#define MMIO(x) *MMIOp(x)

static inline volatile uint32_t *intel_hw_pipe(unsigned int pipe,unsigned int offset) {
	return MMIOp(intel_hw_pipe_reg(pipe,offset));
}

static inline volatile uint32_t *intel_hw_display_plane(unsigned int d,unsigned int offset) {
	return MMIOp(intel_hw_display_plane_reg(d,offset));
}

int unmap_intel_resources();
int map_intel_resources();

#endif

