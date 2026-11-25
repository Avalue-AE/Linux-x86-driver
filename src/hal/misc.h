
#ifndef __HAL_MISC_H__
#define __HAL_MISC_H__

#include <linux/types.h>

struct hal_misc_config {
	const char *label;
	u32 ior;
	u32 iow;
	u8 reg;
	u8 mask;
	u32 max;
};

#define MISC_CONFIG_NONE       \
	{                      \
		.label = NULL, \
		.ior = 0,      \
		.iow = 0,      \
		.reg = 0,      \
		.mask = 0,     \
		.max = 0,      \
	}

#define MISC_CONFIG_ATTR(_lab, _ior, _iow, _reg, _mask, _max) \
	{                                               \
		.label = _lab,                          \
		.ior = _ior,                            \
		.iow = _iow,                            \
		.reg = _reg,                            \
		.mask = _mask,                          \
		.max = _max,                            \
	}

extern s32 hal_misc_read(struct hal_misc_config *cfg, u32 *kdata);

extern s32 hal_misc_write(struct hal_misc_config *cfg, u32 *udata);

extern s32 hal_misc_ioctl(u32 cmd, u32 *udata, u32 *kdata);

extern s32 hal_misc_init(void);

extern void hal_misc_exit(void);

#endif /** __HAL_MISC_H__ */
