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

#include <dirent.h>

#include "find_intel.h"

unsigned long long fb_base_vis=0,fb_base_mmio=0;
unsigned long long fb_size_vis=0,fb_size_mmio=0;

int readz(int fd,char *line,int sz) {
	int rd = read(fd,line,sz-1);
	if (rd < 0) rd = 0;
	line[rd] = 0;
	return rd;
}

int file_one_line(const char *path,char *line,int sz) {
	int fd = open(path,O_RDONLY);
	if (fd < 0) return 0;
	int rd = readz(fd,line,sz);
	if (--rd >= 0 && line[rd] == '\n') line[rd] = 0; /* eat newline */
	close(fd);
	return rd;
}

/* WARNING: copied from linux/ioport.h
 *          perhaps we should include these directly from linux kernel source */
#define IORESOURCE_IO		0x00000100
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_PREFETCH	0x00001000
#define IORESOURCE_CACHEABLE	0x00004000

int get_intel_resources() {
	char line[16384];
	char path[512];
	int fd,rd;

	fb_base_vis = 0;
	fb_size_vis = 0;
	fb_base_mmio = 0;
	fb_size_mmio = 0;

/* scan the PCI bus for any Intel device.
 * Usually the integrated graphics chip is device 0:2:0 on the PCI bus */
	{
		DIR *dir = opendir("/sys/bus/pci/devices");
		if (!dir) {
			fprintf(stderr,"Cannot open sysfs pci list\n");
			return 0;
		}

		struct dirent *d;
		while ((d = readdir(dir)) != NULL) {
			if (d->d_name[0] == '.') continue;
			if (!isdigit(d->d_name[0])) continue;

			sprintf(path,"/sys/bus/pci/devices/%s/vendor",d->d_name);
			if (!file_one_line(path,line,sizeof(line))) continue;
			if (strtoul(line,NULL,0) != 0x8086) continue;

			sprintf(path,"/sys/bus/pci/devices/%s/class",d->d_name);
			if (!file_one_line(path,line,sizeof(line))) continue;
			if ((strtoul(line,NULL,0)&0xFFFF00) != 0x030000) continue;

			sprintf(path,"/sys/bus/pci/devices/%s/resource",d->d_name);
			if ((fd = open(path,O_RDONLY)) < 0) continue;
			rd = read(fd,line,sizeof(line)-1); if (rd < 0) rd = 0;
			line[rd] = 0;
			{
				char *p = line,*fence = line+rd;
				while (p < fence) {
					char *n = strchr(p,'\n');
					if (!n) n = fence;
					else *n++ = 0;

					unsigned long start = strtoul(p,&p,0);
					while (*p == ' ') p++;

					unsigned long end = strtoul(p,&p,0);
					while (*p == ' ') p++;

					unsigned long flags = strtoul(p,&p,0);

					if (flags & IORESOURCE_MEM) {
						if (flags & IORESOURCE_PREFETCH) {
							if (fb_base_vis == 0) {
								fb_base_vis = start;
								fb_size_vis = (end-start)+1;
							}
						}
						else {
							if (fb_base_mmio == 0) {
								fb_base_mmio = start;
								fb_size_mmio = (end-start)+1;
							}
						}
					}

					p = n;
				}
			}
			close(fd);

			break;
		}

		closedir(dir);
	};

#if 1
	printf("Framebuffer: Base 0x%08X Size 0x%08X\n",fb_base_vis,fb_size_vis);
	printf("MMIO:        Base 0x%08X Size 0x%08X\n",fb_base_mmio,fb_size_mmio);
#endif

	return 1;
}

