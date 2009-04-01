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

	/* play with the image size. apparently on my laptop this causes it to only render that much :) */
	int x;
	for (x=0;x <= 1280;x += 16) {
		int w = x;
		int h = x;
		if (h > 800) h = 800;
		MMIO(0x6101C) = (w << 16) | h;
		usleep(1000000/60);
	}

	unmap_intel_resources();
	return 0;
}

