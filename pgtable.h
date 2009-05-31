#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <linux/fb.h>

#include <xmmintrin.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include <sys/io.h>

#include "intelfbhw.h"
#include "find_intel.h"
#include "ringbuffer.h"
#include "util.h"
#include "mmap.h"

#include "tvbox_i8xx.h"

/* device driver to control page table mapping */
extern int i8xx_pgtable_fd;
extern struct tvbox_i8xx_info i8xx_info;
extern volatile uint32_t *i8xx_pgtable;
extern int i8xx_pgtable_size;

void restore_i8xx_pgtable();
void close_i8xx_pgtable();
int open_i8xx_pgtable();

