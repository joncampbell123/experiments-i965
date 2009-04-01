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
		unsigned int pval = -1;
		while (1) {
			unsigned int nval = intel_hw_pipe_read_frame_count(PIPE_B);
			if (pval != nval) {
				printf("0x%08X\n",nval);
				pval = nval;
			}
		}
	}

	unmap_intel_resources();
	return 0;
}

