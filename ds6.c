#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

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

	if (intel_device_chip != INTEL_965)
		if (!intel_wrong_chipset_warning())
			return 1;

	make_gradient_test_cursor_argb(fb_base + 0x400000);

	MMIO(0x70080) = (1 << 28); /* cursor A to pipe B, 64x64 4-color */
	MMIO(0x70084) = 0x400000;      /* follow the buffer */
	MMIO(0x7119C) = 0;

	MMIO(0x700C0) = (1 << 28); /* cursor B to pipe B, 64x64 4-color */
	MMIO(0x700C4) = 0x400000;      /* follow the buffer */
	MMIO(0x7119C) = 0;
	usleep(1000000/4);
	printf("OK\n");

	/* play with the cursor! */
	/* warning this isn't reliable. */
	MMIO(0x700C0) = (1 << 28) | 0x20 | 3; /* cursor B to pipe B, 256x256 ARGB */
	MMIO(0x700C8) = (0 << 16) | 0; /* at (0,0) */

	MMIO(0x70080) = (1 << 28) | 0x20 | 3; /* cursor A to pipe B, 256x256 ARGB */
	MMIO(0x70088) = (0 << 16) | 0; /* at (0,0) */

	MMIO(0x7119C) = 0;

	{
		int i;
		for (i=0;i < 512*4;i++) {
			double a = (((double)i) * 3.1415926 * 2) / 512;

			int x = 640 + (int)(sin(a)*320);
			int y = 400 + (int)(cos(a)*200);
			MMIO(0x700C8) = (y << 16) | x; /* at (0,0) */

			int x2 = 640 + (int)(sin(a*1.1)*320);
			int y2 = 400 + (int)(cos(a*1.1)*200);
			MMIO(0x70088) = (y2 << 16) | x2; /* at (0,0) */

			usleep(1000000/60);
		}
	}

	unmap_intel_resources();
	return 0;
}

