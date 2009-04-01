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

	/* enable hotplug detect */
	MMIO(0x61110) = 0x00000000;
	MMIO(0x61114) = ~0;
	MMIO(0x61110) = 0x00000220;
	usleep(1000000/3);

	printf("CRT/VGA hotplug detection enabled. Unplug the VGA cable if connected, or connect the VGA cable if not connected\n");
	printf("0x%08X\n",MMIO(0x61114));

	unsigned int xx;
	while ((MMIO(0x61114) & (1 << 11)) == 0);
	printf("Saw it!\n");
	printf("0x%08X\n",xx = MMIO(0x61114));
	MMIO(0x61114) = ~0;

	unmap_intel_resources();
	return 0;
}

