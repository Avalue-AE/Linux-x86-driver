
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>

#include <linux/gpio/driver.h>
#include <linux/version.h>

#include "configs/config.h"

#include "hal/bctrl.h"
#include "hal/gpio.h"
#include "log.h"

struct priv_data {
	struct platform_device *pdev;
	struct bctrl_desc *desc;
	struct gpio_chip *gc;
};

/**
 * @direction_input: configures signal "offset" as input, returns 0 on success
 *	or a negative error number. This can be omitted on input-only or
 *	output-only gpio chips.
 */
static int __gpio_chip_direction_input(struct gpio_chip *gc,
				       unsigned int offset)
{
	struct priv_data *priv = gpiochip_get_data(gc);
	struct bctrl_desc *desc = priv->desc;
	s32 ret;

	log_debug("Setting GPIO direction to input at offset %u\n", offset);

	ret = hal_gpio_write_direction(desc, (u16)offset, 1);

	if (ret < 0) {
		log_err("Failed to set GPIO direction to input at offset %u: %d\n",
			offset, ret);
		return ret;
	}

	return 0;
}

/**
 * @direction_output: configures signal "offset" as output, returns 0 on
 *	success or a negative error number. This can be omitted on input-only
 *	or output-only gpio chips.
 */
static int __gpio_chip_direction_output(struct gpio_chip *gc,
					unsigned int offset, int value)
{
	struct priv_data *priv = gpiochip_get_data(gc);
	struct bctrl_desc *desc = priv->desc;
	s32 ret;

	log_debug("Setting GPIO direction to output at offset %u: %d\n", offset, value);

	ret = hal_gpio_write_direction(desc, (u16)offset, 0);
	if (ret < 0) {
		log_err("Failed to set GPIO direction to output at offset %u: %d\n",
			offset, ret);
		return ret;
	}

	ret = hal_gpio_write_value(desc, (u16)offset, value > 0 ? 1 : 0);
	if (ret < 0) {
		log_err("Failed to set GPIO value at offset %u: %d\n", offset, ret);
		return ret;
	}

	return 0;
}

/**
 * @get: returns value for signal "offset", 0=low, 1=high, or negative error
 */
static int __gpio_chip_get(struct gpio_chip *gc, unsigned int offset)
{
	struct priv_data *priv = gpiochip_get_data(gc);
	struct bctrl_desc *desc = priv->desc;
	u16 val = 0;
	s32 ret;

	log_debug("Getting GPIO value at offset %u\n", offset);

	ret = hal_gpio_read_value(desc, (u16)offset, &val);

	if (ret < 0) {
		log_err("Failed to read GPIO value at offset %u: %d\n", offset,
			ret);
		return ret;
	}

	log_debug("Got GPIO value at offset %u: %u\n", offset, val);
	return (int)val;
}

/**
 * @set: assigns output value for signal "offset", 0=low, 1=high
 */
static int __gpio_chip_set(struct gpio_chip *gc, unsigned int offset,
			    int value)
{
	struct priv_data *priv = gpiochip_get_data(gc);
	struct bctrl_desc *desc = priv->desc;
	s32 ret;

	log_debug("Setting GPIO value at offset %u to %d\n", offset, value);

	ret = hal_gpio_write_value(desc, (u16)offset, value > 0 ? 1 : 0);
	if (ret < 0) {
		log_err("Failed to write GPIO value at offset %u: %d\n", offset,
			ret);
		return ret;
	}

	log_debug("Set GPIO value at offset %u to %d successfully\n", offset, value);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 15, 0)
/*
 * gpio_chip.set changed its return type from void to int in kernel 6.15.
 * On older kernels the callback prototype is void-returning, so wrap the int
 * worker above (whose write logic is unchanged); the old API has no channel to
 * report failure, so the return value is intentionally discarded here.
 */
static void __gpio_chip_set_void(struct gpio_chip *gc, unsigned int offset,
				 int value)
{
	__gpio_chip_set(gc, offset, value);
}
#endif

struct gpio_chip __gpio_chip = {
	.label = "gpio",
	.owner = THIS_MODULE,
	.base = -1, // Dynamic allocation
	.ngpio = CONFIG_GPIO_PIN_NUM,
	.direction_input = __gpio_chip_direction_input,
	.direction_output = __gpio_chip_direction_output,
	.get = __gpio_chip_get,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
	.set = __gpio_chip_set,
#else
	.set = __gpio_chip_set_void,
#endif
	.can_sleep = true,
};

struct priv_data __priv = {
	.pdev = NULL,
	.desc = NULL,
	.gc = &__gpio_chip,
};

static int __init gpio_init(void)
{
	int ret = 0;

	log_info("Initializing GPIO driver\n");

	ret = hal_gpio_init();
	if (ret < 0) {
		log_err("Failed to initialize HAL GPIO: %d\n", ret);
		return ret;
	}

	__priv.pdev = platform_device_register_simple("gpio", -1, NULL, 0);
	if (IS_ERR(__priv.pdev)) {
		ret = PTR_ERR(__priv.pdev);
		log_err("Failed to register platform device: %d\n", ret);
		return ret;
	}

	__priv.desc = hal_gpio_desc();
	if (IS_ERR(__priv.desc)) {
		ret = PTR_ERR(__priv.desc);
		log_err("Failed to get HAL GPIO descriptor\n");
		return ret;
	}

	ret = devm_gpiochip_add_data(&__priv.pdev->dev, __priv.gc, &__priv);
	if (ret < 0) {
		log_err("Failed to register GPIO chip: %d\n", ret);
		return ret;
	}

	log_info("GPIO driver initialized successfully, gpiochip%d\n",
		 __priv.gc->base);
	return 0;
}

static void __exit gpio_exit(void)
{
	log_info("Exiting GPIO driver\n");

	if (__priv.pdev && !IS_ERR(__priv.pdev))
		platform_device_unregister(__priv.pdev);

	hal_gpio_exit();
}

module_init(gpio_init);
module_exit(gpio_exit);

MODULE_AUTHOR("Avalue Technology Inc.");
MODULE_AUTHOR("Arthur Huang <arthur_huang@avalue.com>");
MODULE_DESCRIPTION("Hardware Monitor driver for Avalue boards");
MODULE_LICENSE("GPL");
MODULE_VERSION(CONFIG_DRIVER_VERSION);