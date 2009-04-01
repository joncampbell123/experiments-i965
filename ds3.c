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
#include "intelfbhw.h"
#include "mmap.h"

int main() {
	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	{
		unsigned int pval = -1,i;
		while (1) {
			unsigned int nval = intel_hw_pipe_read_scan_line(PIPE_B);
			if (pval != nval) {
				pval = nval;

				int x;
				unsigned short *p = (unsigned short*)(((unsigned char*)fb_base) + (1280*2*nval));
				for (x=0;x < 16;x++,p++)
					p[0]++;
			}
		}
	}

	unmap_intel_resources();
	return 0;
}

