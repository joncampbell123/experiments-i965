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
#include "util.h"
#include "mmap.h"

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	/* play with the flat panel scaling stuff! */
	MMIO(0x61230) = 0x80000000 | (1 << 29) | (1 << 26) | (0 << 24);
	int x,y;
	for (y=0;y <= 4096;y += 16) {
		MMIO(0x61234) = (y << 16) | 4096;
		usleep(1000000/60);
	}

	unmap_intel_resources();
	return 0;
}

