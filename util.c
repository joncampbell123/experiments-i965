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

double frtime() {
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return (((double)tv.tv_sec) + ((double)tv.tv_usec)/1000000);
}

unsigned int mult_rgb(unsigned int x,int a) {
	int r = x & 0xFF;
	int g = (x >> 8) & 0xFF;
	int b = (x >> 16) & 0xFF;
	return	((r * a) >> 8) |
		(((g * a) >> 8) << 8) |
		(((b * a) >> 8) << 16);
}

void make_gradient_test_cursor_argb(unsigned char *cursor) {
	int x;
	unsigned int *p_xor = (unsigned int*)cursor;

	for (x=0;x < 256*256;x++) {
		int a = x >> 8;
		p_xor[x] = mult_rgb(x,a) | (a << 24);
	}
}

