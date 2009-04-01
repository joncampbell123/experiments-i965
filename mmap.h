
#include <stdint.h>

extern int mem_fd;
extern unsigned char *fb_base;
extern volatile uint32_t *fb_mmio;

#define MMIO(x) *( (volatile uint32_t*) (((unsigned char*)fb_mmio) + (x)) )

int unmap_intel_resources();
int map_intel_resources();

