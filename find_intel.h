#ifndef FIND_INTEL_H
#define FIND_INTEL_H

enum {
	INTEL_UNKNOWN=0,
	INTEL_855,
	INTEL_965,
	INTEL_CLARKDALE
};

extern const char *intel_device_chip_str[];

extern unsigned int intel_pci_device;
extern unsigned int intel_device_chip;
extern char sysfs_intel_graphics_dev[32];
extern unsigned long long fb_base_vis,fb_base_mmio;
extern unsigned long long fb_size_vis,fb_size_mmio;

int readz(int fd,char *line,int sz);
int file_one_line(const char *path,char *line,int sz);
int get_intel_resources();

#endif

