
#ifndef __HAL_BCTRL_H__
#define __HAL_BCTRL_H__

#include <linux/types.h>

enum bctrl_type {
	BCTRL_TYPE_NONE = 0,
	BCTRL_TYPE_SIO,
	BCTRL_TYPE_EC,
	BCTRL_TYPE_SMBUS,
	BCTRL_TYPE_MAX,
};

enum bctrl_driver {
	BCTRL_DRIVER_NONE = 0,
	BCTRL_DRIVER_WATCHDOG = 1 << 0,
	BCTRL_DRIVER_HWMON = 1 << 1,
	BCTRL_DRIVER_GPIO = 1 << 2,

};

enum bctrl_device {
	BCTRL_DEVICE_NONE = 0,
	BCTRL_DEVICE_WDT,
	BCTRL_DEVICE_HWM,
	BCTRL_DEVICE_GPIO,
	BCTRL_DEVICE_MAX,
};

/** 
 * struct bctrl_desc - Board control identification information
 * @name: Chip name
 * @vendor: Vendor name
 * @chip_id: Device ID
 */
struct bctrl_desc {
	enum bctrl_type type; // Board control type @see bctrl_type (BCTRL_TYPE_*)
	u8 mask; // Supported drivers @see bctrl_driver (BCTRL_DRIVER_*)
	u16 chip_id; // Chipset/device ID
	void *priv; // Driver specific private data
};

extern struct bctrl_desc hal_desc;

#endif /* __HAL_BCTRL_H__ */
