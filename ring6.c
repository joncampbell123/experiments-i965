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
#include "mmap.h"

#include "tvbox_i8xx.h"
#include "pgtable.h"
#include "uvma.h"

static int DIE = 0;

void sigma(int x) {
	if (++DIE >= 5)
		_exit(0);
}

const int seizure_mode = 0;

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

	{
		int fd = open("/dev/fb0",O_RDWR);
		if (fd < 0) return 1;

		struct fb_var_screeninfo vi;
		if (ioctl(fd,FBIOGET_VSCREENINFO,&vi) < 0)
			return 1;

		screen_width = vi.xres;
		screen_height = vi.yres;
		close(fd);
	}

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
			if (seizure_mode) { }
			else if (intel_device_chip == INTEL_965)
				ring_emit((3 << 23) | (1 << 18)); /* pipe B: wait for HBLANK */
			else
				ring_emit((3 << 23) | (1 << 3)); /* pipe B: wait for HBLANK */

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

			wait_ring_space(256);
		}
/*		wait_ring_space((ring_size>>2)-16); */
	}

	while (!DIE) {
		/* Intel 855GM we have to do this or else the chipset runs out and hangs */
		fill_no_ops(128);
		wait_ring_space(256);
	}

	stop_ring();
	restore_i8xx_pgtable();
	close_i8xx_pgtable();
	close_uvirt_to_phys();
	unmap_intel_resources();
	return 0;
}

