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

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	{
		unsigned short *p = (unsigned short*)fb_base;
		unsigned short *f = p + 1280*800;
		int x=0,o=0,c=0,y=0;
		while (p < f) {
			int g = (x >> 5) & 0x3F;
			c = (x & 0x1F) | ((y & 0x1F) << 11) | (g << 5);
			*p++ = o & 1 ? c : c;
			if (++x >= 1280) {
				x -= 1280;
				y++;
			}
		}
	}

	{
		MMIO(0x71180) = 0x80000000 | (5 << 26) | (0 << 20); /* enable, 16bpp format, no pixel doubling */
		MMIO(0x71184) = 0; /* start at zero */
		MMIO(0x71188) = 1280*2;
		MMIO(0x7119C) = 0; /* this causes the change to occur */
	}

	unmap_intel_resources();
	return 0;
}

