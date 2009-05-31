#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

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
#include "mmap.h"

#include "tvbox_i8xx.h"

static int DIE = 0;

void sigma(int x) {
	if (++DIE >= 5)
		_exit(0);
}

const int seizure_mode = 0;

/* device driver to control page table mapping */
int i8xx_pgtable_fd = -1;
struct tvbox_i8xx_info i8xx_info;
volatile uint32_t *i8xx_pgtable = NULL;
int i8xx_pgtable_size = 0;

void restore_i8xx_pgtable() {
	if (ioctl(i8xx_pgtable_fd,TVBOX_I8XX_SET_DEFAULT_PGTABLE) < 0)
		fprintf(stderr,"Cannot TVBOX_I8XX_SET_DEFAULT_PGTABLE, %s\n",strerror(errno));
}

void close_i8xx_pgtable() {
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

	i8xx_pgtable = (volatile uint32_t*)mmap(NULL,i8xx_pgtable_size,PROT_READ|PROT_WRITE,MAP_SHARED,i8xx_pgtable_fd,0);
	if (i8xx_pgtable == (volatile uint32_t*)(-1)) {
		fprintf(stderr,"Cannot mmap tvbox pgtable, %s\n",strerror(errno));
		close_i8xx_pgtable();
		return 0;
	}

	printf("Pgtable driver, %luKB @ 0x%08lX\n",(unsigned long)(i8xx_info.pgtable_size >> 10UL),(unsigned long)i8xx_info.pgtable_base);
	return 1;
}

int uvirt_to_phys_fd = -1;

int open_uvirt_to_phys() {
	if (uvirt_to_phys_fd >= 0)
		return 1;

	if ((uvirt_to_phys_fd = open("/proc/self/pagemap",O_RDONLY)) < 0) {
		fprintf(stderr,"Cannot open pagemap\n");
		return 0;
	}

	return 1;
}

void close_uvirt_to_phys() {
	if (uvirt_to_phys_fd >= 0) {
		close(uvirt_to_phys_fd);
		uvirt_to_phys_fd = -1;
	}
}

unsigned long long uvirt_to_phys(unsigned long vma) {
	uint64_t d;

	if (vma & 4095)
		return -1LL;

	vma >>= 12UL - 3UL;
	if (lseek(uvirt_to_phys_fd,vma,SEEK_SET) != vma)
		return -1LL;
	if (read(uvirt_to_phys_fd,&d,8) != 8)
		return -1LL;

	if (!(d & (1ULL << 63ULL)))
		return -1LL;	/* not present */
	if (d & (1ULL << 62ULL))
		return -1LL;	/* swapped out */

	return (d & ((1ULL << 56ULL) - 1ULL)) << 12ULL;
}

int main() {
	signal(SIGINT,sigma);
	signal(SIGQUIT,sigma);
	signal(SIGTERM,sigma);

	iopl(3);

	if (!get_intel_resources())
		return 1;
	if (!map_intel_resources())
		return 1;
	if (!open_i8xx_pgtable())
		return 1;
	if (!open_uvirt_to_phys())
		return 1;

	/* I pick the 2MB mark for the ring buffer. Use larger space for speed tests, about slightly less than 2MB */
	set_ring_area(0x200000,2040*1024);
	start_ring();
	make_gradient_test_cursor_argb(fb_base + 0x400000);

	/* at the 16MB mark, map in a buffer from our userspace */
	int pbuf_sz = 512*512*2;
	uint16_t *pbuf = (uint16_t*)mmap(NULL,pbuf_sz,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
	if (pbuf == (uint16_t*)(-1)) {
		fprintf(stderr,"Cannot mmap private buffer\n");
		return 1;
	}
	if (mlock(pbuf,pbuf_sz) < 0) {
		fprintf(stderr,"Cannot lock private buffer\n");
		return 1;
	}
	/* write a visible pattern. this will probably be heavily cached by the processor.
	 * if the below code fails, we'll see a half-assed writeback hack copy of this pattern :) */
	{
		int x,y;
		for (y=0;y < 512;y++) {
			for (x=0;x < 512;x++) {
				pbuf[(y*512)+x] = x^y;
			}
		}
		for (x=0;x < 512*512;x += 16)
			_mm_clflush((void const*)(pbuf+x));
	}
	/* convert each page to physical and write it into mapping */
	{
		int page;
		unsigned int pgoff = (8UL << 20UL) >> 12UL;
		for (page=0;page < pbuf_sz;page += 4096) {
			unsigned long long phys_addr = uvirt_to_phys((unsigned long)((unsigned char*)pbuf + page));
			if (phys_addr == (unsigned long long)(-1LL)) {
				fprintf(stderr,"Cannot map %uth page\n",page>>12);
				return 1;
			}
			printf("page %d: 0x%08llX\n",page>>12,(unsigned long long)phys_addr);
			i8xx_pgtable[pgoff++] = (uint32_t)phys_addr | 1;
		}
	}

	fprintf(stderr,"Booting 2D ringbuffer.\n");

	/* insert no-ops that change NOPID. if the NOPID becomes the value we wrote, consider the test successful. */
	uint32_t v = ((uint32_t)((rand()*rand()*rand()))) & 0x1FFFFF;
	mi_noop_id(v);
	fill_no_ops(1);
	ring_emit_finish();

	int counter = 0;
	while (read_nopid() != v) {
		if (++counter == 1000000) {
			fprintf(stderr,"Test failed. Ring buffer failed to start.\n");
			fprintf(stderr,"Wrote 0x%08X, read back 0x%08X\n",v,read_nopid());
			return 1;
		}
	}
	printf("Ringbuffer hit NOPID no-op in %u counts. Success.\n",counter);

	int screen_width = 1280;
	int screen_height = 768;

	/* now fill with no-ops and see how fast it really goes.
	 * this time we tell it to wait for vblank, so the pipeline will be waiting around a lot.
	 * and hey, maybe this might be a good way to measure the vertical sync rate.
	 * to avoid a race with the 2D ring we stop the ring, fill with commands, then start it again. */
	stop_ring();
	{
		unsigned int c,cmax=1000000;
		/* prep the cursors */
		mi_load_imm(0x70080,1 << 28);
		mi_load_imm(0x700C0,1 << 28);
		mi_load_imm(0x70084,0x400000);
		mi_load_imm(0x700C4,0x400000);
		mi_load_imm(0x71184,0);
		mi_load_imm(0x7119C,0);
		start_ring();
		/* animate */
		for (c=0;!DIE && c <= cmax;c++) {	/* how many we can fill up before hitting the end of the ring */
			int x1,x2,y1,y2;
			int cx = 640,cy = 360;
			double a = (((double)c) * 6.28) / 1000;
			x1 = cx + (sin(a) * 480);
			y1 = cy + (cos(a) * 240);
			x2 = cx + (sin(a*5) * 360);
			y2 = cy + (cos(a*5) * 180);
			if (seizure_mode) { }
			else if (intel_device_chip == INTEL_965)
				ring_emit((3 << 23) | (1 << 18)); /* pipe B: wait for HBLANK */
			else
				ring_emit((3 << 23) | (1 << 3)); /* pipe B: wait for HBLANK */

#if 0
			mi_load_imm(0x700C0,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x700C8,(y1 << 16) | x1);
			mi_load_imm(0x70080,(1 << 28) | 0x20 | 3);
			mi_load_imm(0x70088,(y2 << 16) | x2);
#endif
			/* fun with the COLOR_BLIT */
			src_copy_blit(
				(screen_width*2*1)+(1*2),	/* dest */
				screen_width,600,	/* 320x240 block */
				screen_width*2,		/* dest pitch */
				(screen_width*2*2)+(0*2),	/* src */
				screen_width*2);
			color_blit_fill((screen_width*2*598)+(0*2), /* start at 2nd scan line */
				1000,2,	/* 640x480 block */
				screen_width*2,		/* pitch */
				c);		/* what to fill with */
			/* and from our buffer */
			int sypan = (c % ((512 - 100) * 2));
			if (sypan >= (512 - 100))
				sypan = ((512 - 100) * 2) - sypan;

			src_copy_blit(
				(screen_width*2*600),
				512,100,
				screen_width*2,
				(8UL << 20ULL)+(512*2*sypan),		/* src @ 16MB mark */
				512*2);
			ring_emit_finish();
			wait_ring_space(64);

			{
				int x,y;
				for (y=0;y < 512;y++) {
					for (x=0;x < 512;x++) {
						pbuf[(y*512)+x] = (x^y)+c;
					}
				}
				for (x=0;x < 512*512;x += 16)
					_mm_clflush((void const*)(pbuf+x));
			}
		}
/*		wait_ring_space((ring_size>>2)-16); */
	}

	while (!DIE);

	stop_ring();
	restore_i8xx_pgtable();
	close_i8xx_pgtable();
	close_uvirt_to_phys();
	unmap_intel_resources();
	return 0;
}

