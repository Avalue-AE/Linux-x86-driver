
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/ec/ite.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/misc.h"

#include "log.h"

extern struct ec_device ite_dev;
extern struct bctrl_desc ite_desc;

#define MISC_CONFIG(nr)                                                    \
	MISC_CONFIG_ATTR(CONFIG_MISC_##nr##_LABEL, CONFIG_MISC_##nr##_IOR, \
			 CONFIG_MISC_##nr##_IOW, CONFIG_MISC_##nr##_REG,   \
			 CONFIG_MISC_##nr##_MASK)

/** ITE MISC map */
static const u8 hal_misc_map[CONFIG_MISC_NUM] = CONFIG_MISC_MAP;
struct hal_misc_config hal_misc_configs[CONFIG_MISC_NUM] = {
#if CONFIG_MISC_0_ENABLE
	MISC_CONFIG(0),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_1_ENABLE
	MISC_CONFIG(1),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_2_ENABLE
	MISC_CONFIG(2),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_3_ENABLE
	MISC_CONFIG(3),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_4_ENABLE
	MISC_CONFIG(4),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_5_ENABLE
	MISC_CONFIG(5),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_6_ENABLE
	MISC_CONFIG(6),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_7_ENABLE
	MISC_CONFIG(7),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_8_ENABLE
	MISC_CONFIG(8),
#else
	MISC_CONFIG_NONE,
#endif

#if CONFIG_MISC_9_ENABLE
	MISC_CONFIG(9),
#else
	MISC_CONFIG_NONE,
#endif
};

s32 hal_misc_read(struct hal_misc_config *cfg, u32 *kdata)
{
	u8 data = 0;

	data = ite_dev.ops->read8(cfg->reg) & cfg->mask;
	*kdata = (u32)data;
	return 0;
}

s32 hal_misc_write(struct hal_misc_config *cfg, u32 *udata)
{
	u8 data = 0;

	data = ite_dev.ops->read8(cfg->reg);
	data = (data & ~cfg->mask) | ((u8)(*udata) & cfg->mask);
	ite_dev.ops->write8(cfg->reg, data);
	return 0;
}

s32 hal_misc_ioctl(u32 cmd, u32 *udata, u32 *kdata)
{
	u8 i = 0, nr = 0, rw = 0;
	struct hal_misc_config *cfg = NULL;

	for (i = 0; i < CONFIG_MISC_NUM; i++) {
		cfg = &hal_misc_configs[i];

		if (cfg->ior != 0 && cmd == cfg->ior)
			return hal_misc_read(cfg, kdata);

		if (cfg->iow != 0 && cmd == cfg->iow)
			return hal_misc_write(cfg, udata);
	}

	// not found
	return -ENOTTY;
}

s32 hal_misc_init(void)
{
	return ite_dev.ops->probe();
}

void hal_misc_exit(void)
{
	// no need for ITE Chips
}