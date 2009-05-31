
/* COPIED FROM k_tvbox_i8xx */

#define DEBUG_ME

enum {
	/* sorry these are all the test subjects I have */
	CHIP_855,	/* 855GM chipsets */
	CHIP_965	/* 965 chipset */
};

/* NOTE: this is not cross x86 and x86-64 usable.
 *       if your kernel is x86 you need to use this like an x86.
 *       if you're x86-64 then you can't use this as an x86 app.
 *
 *       NOTE: be aware of integer type sizes
 *
 *                   x86         x86-64
 *               -------------------------
 * unsigned int  |    4            4
 * unsigned long |    4            8
 * long long     |    8            8
 *
 */
struct tvbox_i8xx_info {
/* chipset info */
	unsigned long		total_memory;

	unsigned long		stolen_base;
	unsigned long		stolen_size;

	unsigned long		aperature_base;
	unsigned long		aperature_size;

	unsigned long		mmio_base;
	unsigned long		mmio_size;

	unsigned int		chipset;

/* physical buffer info */
	unsigned long		pgtable_base;
	unsigned long		pgtable_size;
} tvbox_i8xx_info;

/* driver ioctls */
/* --- get driver info */
#define TVBOX_I8XX_GINFO			_IOR('I', 0x01, struct tvbox_i8xx_info)
/* --- instruct driver to overwrite pgtable with default mapping and set page table control to point at that */
#define TVBOX_I8XX_SET_DEFAULT_PGTABLE		_IO ('I', 0x02)
/* --- instruct driver to restore Intel VGA BIOS pgtable in stolen RAM. destroys our pgtable in the process */
#define TVBOX_I8XX_SET_VGA_BIOS_PGTABLE		_IO ('I', 0x03)
/* --- instruct driver to make allocated pgtable the active buffer */
#define TVBOX_I8XX_PGTABLE_ACTIVATE		_IO ('I', 0x04)

#define TVBOX_I8XX_MINOR	248

