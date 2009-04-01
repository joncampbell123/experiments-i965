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
void start_ring();
void stop_ring();

