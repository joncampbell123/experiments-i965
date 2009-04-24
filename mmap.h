#ifndef INTEL_MMAP_H
#define INTEL_MMAP_H

#include <stdint.h>
#include "intelfbhw.h"

extern int mem_fd;
extern unsigned char *fb_base;
extern volatile uint32_t *fb_mmio;
extern int self_pagemap_fd;
extern int pcicfg_fd;

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
size_t virt2phys(void *p);
int open_intel_pcicfg(const char *dev);
int close_intel_pcicfg();
uint8_t intel_pcicfg_u8(uint32_t o);
void intel_pcicfg_u8_write(uint32_t o,uint8_t c);
uint16_t intel_pcicfg_u16(uint32_t o);
uint32_t intel_pcicfg_u32(uint32_t o);

#endif

