#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

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
#include "find_intel.h"
#include "util.h"
#include "mmap.h"

extern volatile uint32_t *ring_base,*ring_end,*ring_head,*ring_tail,ring_size;

uint32_t ptr_to_fb(volatile void *x);
void ring_emit(uint32_t dw);
void mi_noop_id(uint32_t id);
void ring_emit_finish();
void fill_no_ops(int x);
void set_ring_area(uint32_t base,uint32_t size);
void mi_load_imm(uint32_t what,uint32_t with);
void color_blit_fill(uint32_t dest,int width,int height,int pitch,uint32_t val);
void src_copy_blit(uint32_t dest,int dw,int dh,int dp,uint32_t src,int sp);
void mi_store_data_index_u64(unsigned int idx,uint64_t value);
void mi_store_data_index(unsigned int idx,uint32_t value);
void mi_store_data_imm(unsigned int gtt,unsigned long addr,uint32_t value);
void mi_store_data_imm_u64(unsigned int gtt,unsigned long addr,uint64_t value);
void wait_ring_space(unsigned int x);
void start_ring();
void stop_ring();

enum {
	MAT_PHYS=0,
	MAT_GTT=1
};

#endif /* __RINGBUFFER_H */

