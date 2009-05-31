#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <linux/fb.h>

#include <xmmintrin.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include <sys/io.h>

#include "intelfbhw.h"
#include "find_intel.h"
#include "ringbuffer.h"
#include "util.h"
#include "mmap.h"	/* we re-use mem_fd descriptor to /dev/mem */

#include "tvbox_i8xx.h"
#include "pgtable.h"

/* device driver to control page table mapping */
int i8xx_pgtable_fd = -1;
struct tvbox_i8xx_info i8xx_info;
volatile uint32_t *i8xx_pgtable = NULL;
volatile uint32_t *i8xx_hwst = NULL;
int i8xx_pgtable_size = 0;

void restore_i8xx_pgtable() {
	if (ioctl(i8xx_pgtable_fd,TVBOX_I8XX_SET_DEFAULT_PGTABLE) < 0)
		fprintf(stderr,"Cannot TVBOX_I8XX_SET_DEFAULT_PGTABLE, %s\n",strerror(errno));
}

void close_i8xx_pgtable() {
	if (i8xx_hwst != NULL) {
		munmap((void*)i8xx_hwst,4096);
		i8xx_hwst = NULL;
	}

	if (i8xx_pgtable != NULL) {
		munmap((void*)i8xx_pgtable,i8xx_pgtable_size);
		i8xx_pgtable = NULL;
	}

	if (i8xx_pgtable_fd >= 0) {
		close(i8xx_pgtable_fd);
		i8xx_pgtable_fd = -1;
	}
}

int open_i8xx_pgtable() {
	int x;

	if (i8xx_pgtable_fd >= 0)
		return 1;

	if ((i8xx_pgtable_fd = open("/dev/tvbox_i8xx",O_RDWR)) < 0) {
		fprintf(stderr,"Cannot open /dev/tvbox_i8xx, %s\n",strerror(errno));
		close_i8xx_pgtable();
		return 0;
	}

	if (ioctl(i8xx_pgtable_fd,TVBOX_I8XX_GINFO,&i8xx_info) < 0) {
		fprintf(stderr,"Cannopt get tvbox_i8xx info, %s\n",strerror(errno));
		close_i8xx_pgtable();
		return 0;
	}
	i8xx_pgtable_size = i8xx_info.pgtable_size;

	i8xx_pgtable = (volatile uint32_t*)mmap(NULL,i8xx_pgtable_size,PROT_READ|PROT_WRITE,MAP_SHARED,i8xx_pgtable_fd,i8xx_info.pgtable_base);
	if (i8xx_pgtable == (volatile uint32_t*)(-1)) {
		fprintf(stderr,"Cannot mmap tvbox pgtable, %s\n",strerror(errno));
		close_i8xx_pgtable();
		return 0;
	}

	/* whatever's there, force it to flush, so weird things don't happen */
	for (x=0;x < i8xx_pgtable_size;x += 16)
		_mm_clflush((void const*)((char*)i8xx_pgtable+x));

	i8xx_hwst = NULL;
	if (i8xx_info.hwst_base != 0) {
		/* note newer kernels have code to explicitly deny mmap()ing system memory, even for root. we can't use /dev/mem */
		printf("H/W status page present. mapping that too\n");
		i8xx_hwst = (volatile uint32_t*)mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,i8xx_pgtable_fd,i8xx_info.hwst_base);
		if (i8xx_hwst == (volatile uint32_t*)(-1)) {
			printf("Can't mmap it\n");
			i8xx_hwst = NULL;
		}
	}

	printf("Pgtable driver, %luKB @ 0x%08lX\n",(unsigned long)(i8xx_info.pgtable_size >> 10UL),(unsigned long)i8xx_info.pgtable_base);
	return 1;
}

