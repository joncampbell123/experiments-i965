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
	char c;

	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;

	if (intel_device_chip != INTEL_965)
		if (!intel_wrong_chipset_warning())
			return 1;

	/* prove that we can update the GTT as needed by twiddling with the table.
	 * notice: we never just make up variables, we make SURE to copy only what was already there.
	 * remember the table entries are literal addresses in DRAM and naturally if we write the
	 * wrong values system memory can be corrupted and mistaken as VRAM! */
	volatile uint32_t *GTT = (volatile uint32_t*)((char*)fb_mmio + (fb_size_mmio>>1));
	uint32_t gtt_first = GTT[0];
	int i;

	printf("First GTT: 0x%08lX\n",(unsigned long)gtt_first);

	for (i=1;i < 256;i++) GTT[i] = gtt_first;
	printf("OK I corrupted the first first entries. Does your SVGA graphics console look funny now? :)\n");

	do { read(0,&c,1); } while (c != 10);

	for (i=1;i < 256;i++) GTT[i] = gtt_first + (i * 0x1000);
	printf("Don't worry, I restored it.\n");

	unmap_intel_resources();
	return 0;
}

