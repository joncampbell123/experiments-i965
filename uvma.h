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
#include "pgtable.h"

extern int uvirt_to_phys_fd;

int open_uvirt_to_phys();
void close_uvirt_to_phys();
unsigned long long uvirt_to_phys(unsigned long vma);

