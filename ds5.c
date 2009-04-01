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

	MMIO(0x700C0) = (1 << 28); /* cursor B to pipe B, 64x64 4-color */
	MMIO(0x700C4) = 0;    
	*(intel_hw_display_plane(DISPLAY_B,DISPLAY_PLANE_SURFACE_BASE_ADDRESS)) = 0;
	usleep(1000000/5);
	printf("OK\n");

	/* play with the cursor! */
	/* warning this isn't reliable. */
	MMIO(0x700C4) = 0x400000;      /* follow the buffer (we must write this first, or it doesn't take effect) */
	MMIO(0x700C0) = (1 << 28) | 0x20 | 3; /* cursor B to pipe B, 256x256 ARGB */
	MMIO(0x700C8) = (0 << 16) | 0; /* at (0,0) */
	*(intel_hw_display_plane(DISPLAY_B,DISPLAY_PLANE_SURFACE_BASE_ADDRESS)) = 0;

	make_gradient_test_cursor_argb(fb_base + 0x400000);

	{
		int x;
		for (x=0;x < 512;x++) {
			MMIO(0x700C8) = (x << 16) | x; /* at (x,x) */
			usleep(1000000/60);
		}
	}

	unmap_intel_resources();
	return 0;
}

