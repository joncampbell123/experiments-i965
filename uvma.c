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
#include "uvma.h"

int uvirt_to_phys_fd = -1;

int open_uvirt_to_phys() {
	if (uvirt_to_phys_fd >= 0)
		return 1;

	if ((uvirt_to_phys_fd = open("/proc/self/pagemap",O_RDONLY)) < 0) {
		fprintf(stderr,"Cannot open pagemap\n");
		return 0;
	}

	return 1;
}

void close_uvirt_to_phys() {
	if (uvirt_to_phys_fd >= 0) {
		close(uvirt_to_phys_fd);
		uvirt_to_phys_fd = -1;
	}
}

unsigned long long uvirt_to_phys(unsigned long vma) {
	uint64_t d;

	if (vma & 4095)
		return -1LL;

	vma >>= 12UL - 3UL;
	if (lseek(uvirt_to_phys_fd,vma,SEEK_SET) != vma)
		return -1LL;
	if (read(uvirt_to_phys_fd,&d,8) != 8)
		return -1LL;

	if (!(d & (1ULL << 63ULL)))
		return -1LL;	/* not present */
	if (d & (1ULL << 62ULL))
		return -1LL;	/* swapped out */

	return (d & ((1ULL << 56ULL) - 1ULL)) << 12ULL;
}

