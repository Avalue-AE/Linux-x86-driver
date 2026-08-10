
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/smb/smb.h"
#include "hal/gpio.h"

#include "log.h"

extern struct smb_device nct5655_dev;
extern struct bctrl_desc nct5655_desc;

static const u16 hal_gpio_map[CONFIG_GPIO_PIN_NUM] = CONFIG_GPIO_PIN_MAP;

static struct smb_device *__to_smb_device(struct bctrl_desc *desc)
{
	if (desc == NULL) {
		log_err("Invalid bctrl descriptor pointer\n");
		return NULL;
	}

	return (struct smb_device *)desc->priv;
}

struct bctrl_desc *hal_gpio_desc(void)
{
	return &nct5655_desc;
}

s32 hal_gpio_read_value(struct bctrl_desc *dev, u16 pin, u16 *out)
{
	struct smb_device *sdev = __to_smb_device(dev);
	s32 ret = 0;
	u16 mask = 0, buf = 0;

	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

	if (sdev == NULL) {
		log_err("Invalid GPIO device pointer\n");
		return -ENODEV;
	}

	mask = BIT(hal_gpio_map[pin]);
	ret = sdev->gpio->read_value(sdev, &buf);
	if (ret < 0) {
		log_err("Failed to read GPIO value\n");
		return ret;
	}

	*out = (u16)((buf & mask) > 0) ? 1 : 0;
	log_debug("Read GPIO value at pin %u: %u\n", pin, *out);
	return 0;
}

s32 hal_gpio_write_value(struct bctrl_desc *dev, u16 pin, u16 val)
{
	struct smb_device *sdev = __to_smb_device(dev);
	s32 ret = 0;
	u16 mask = 0, buf = 0;

	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

	if (sdev == NULL) {
		log_err("Invalid GPIO device pointer\n");
		return -ENODEV;
	}

	mask = BIT(hal_gpio_map[pin]);
	ret = sdev->gpio->read_value(sdev, &buf);
	if (ret < 0) {
		log_err("Failed to read GPIO value\n");
		return ret;
	}

	ret = sdev->gpio->write_value(sdev, val > 0 ? buf | mask : buf & ~mask);
	if (ret < 0) {
		log_err("Failed to write GPIO value\n");
		return ret;
	}

	log_debug("Wrote GPIO value at pin %u: %u\n", pin, val);
	return 0;
}

s32 hal_gpio_read_direction(struct bctrl_desc *dev, u16 pin, u16 *out)
{
	struct smb_device *sdev = __to_smb_device(dev);
	s32 ret = 0;
	u16 mask = 0, buf = 0;

	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

	if (sdev == NULL) {
		log_err("Invalid GPIO device pointer\n");
		return -ENODEV;
	}

	mask = BIT(hal_gpio_map[pin]);
	ret = sdev->gpio->read_direction(sdev, &buf);
	if (ret < 0) {
		log_err("Failed to read GPIO direction\n");
		return ret;
	}

	*out = (u16)((buf & mask) > 0) ? 1 : 0;
	log_debug("Read GPIO direction at pin %u: %s\n", pin,
		  (*out > 0) ? "input" : "output");
	return 0;
}

s32 hal_gpio_write_direction(struct bctrl_desc *dev, u16 pin, u16 dir)
{
	struct smb_device *sdev = __to_smb_device(dev);
	s32 ret = 0;
	u16 mask = 0, buf = 0;

	if (sdev == NULL) {
		log_err("Invalid GPIO device pointer\n");
		return -ENODEV;
	}

	mask = BIT(hal_gpio_map[pin]);
	ret = sdev->gpio->read_direction(sdev, &buf);
	if (ret < 0) {
		log_err("Failed to read GPIO direction\n");
		return ret;
	}

	ret = sdev->gpio->write_direction(sdev,
					  dir > 0 ? buf | mask : buf & ~mask);
	if (ret < 0) {
		log_err("Failed to write GPIO direction\n");
		return ret;
	}
	log_debug("Wrote GPIO direction at pin %u: %s\n", pin,
		  (dir > 0) ? "input" : "output");
	return 0;
}

s32 hal_gpio_init(void)
{
	s32 ret = 0;

	ret = nct5655_dev.gpio->probe(&nct5655_dev);
	if (ret < 0) {
		log_err("NCT5655 SMBus probe failed\n");
		return ret;
	}

	log_debug("NCT5655 GPIO initialized successfully\n");
	return 0;
}

void hal_gpio_exit(void)
{
	// No specific deinitialization needed for NCT5655 GPIO
}