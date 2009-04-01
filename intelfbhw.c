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
#include "mmap.h"

unsigned int intel_hw_pipe_read_frame_count(unsigned int pipe) {
	return	((*intel_hw_pipe(pipe,DISPLAY_PIPE_FRAME_COUNT_LOW_PIXEL_COUNT)) >> 24) |
		(((*intel_hw_pipe(pipe,DISPLAY_PIPE_FRAME_COUNT_HIGH)) & 0xFFFF) << 8); /* watch the page counter */
}

unsigned int intel_hw_pipe_read_scan_line(unsigned int pipe) {
	return *intel_hw_pipe(PIPE_B,DISPLAY_PIPE_SCAN_LINE);
}

