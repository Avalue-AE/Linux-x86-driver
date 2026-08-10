
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

#include <linux/watchdog.h>

#include "configs/config.h"

#include "hal/bctrl.h"
#include "hal/wdt.h"
#include "log.h"

// Private data structure for watchdog
struct priv_data {
	struct bctrl_desc *desc;
	bool is_running;
	u8 timeout; // depends on unit (seconds or minutes)
	u8 unit; // 0: seconds, 1: minutes
};

static struct priv_data __priv;

/** Forward declaration for watchdog operations */

// starting the watchdog device @see watchdog.h
static int __wdt_start(struct watchdog_device *wdd);
// stopping the watchdog device @see watchdog.h
static int __wdt_stop(struct watchdog_device *wdd);
// ping/pet/kick the watchdog to prevent timeout @see watchdog.h
static int __wdt_ping(struct watchdog_device *wdd);
// set timeout value for the watchdog @see watchdog.h
static int __wdt_set_timeout(struct watchdog_device *wdd, unsigned int timeout);
// get remaining time before watchdog timeout @see watchdog.h
static unsigned int __wdt_get_timeleft(struct watchdog_device *wdd);
// ioctl operations for the watchdog @see watchdog.h
static long __wdt_ioctl(struct watchdog_device *wdd, unsigned int cmd,
			unsigned long arg);

static struct watchdog_info wdt_info = {
	.identity = CONFIG_BOARD_NAME " " CONFIG_WDT_CHIPSET " Watchdog",
	.options = (WDIOF_SETTIMEOUT // /**< Timeout can be set */
		    | WDIOF_KEEPALIVEPING // /**< Keep alive ping supported */
		    | WDIOS_ENABLECARD // /**< Can enable the watchdog */
		    | WDIOS_DISABLECARD // /**< Can disable the watchdog */
		    ),
};

static struct watchdog_ops wdt_ops = {
	.owner = THIS_MODULE,
	.start = __wdt_start,
	.stop = __wdt_stop,
	.ping = __wdt_ping,
	.set_timeout = __wdt_set_timeout,
	.get_timeleft = __wdt_get_timeleft,
	.ioctl = __wdt_ioctl,
};

static struct watchdog_device wdt_dev = {
	.info = &wdt_info,
	.ops = &wdt_ops,
	.driver_data = (void *)&__priv,
	.timeout = 60, /* Default timeout in seconds */
	.min_timeout = 1, /* Minimum timeout in seconds */
	.max_timeout = 255, /* Maximum timeout in seconds */
};

static int __wdt_start(struct watchdog_device *wdd)
{
	struct priv_data *data = (struct priv_data *)wdd->driver_data;

	if (data == NULL) {
		log_err("Invalid private data\n");
		return -EINVAL;
	}

	hal_wdt_write(data->timeout);
	hal_wdt_start();
	data->is_running = true;
	return 0;
}

static int __wdt_stop(struct watchdog_device *wdd)
{
	struct priv_data *data = (struct priv_data *)wdd->driver_data;

	if (data == NULL) {
		log_err("Invalid private data\n");
		return -EINVAL;
	}

	hal_wdt_stop();
	data->is_running = false;
	return 0;
}

static int __wdt_ping(struct watchdog_device *wdd)
{
	struct priv_data *data = (struct priv_data *)wdd->driver_data;

	if (data == NULL) {
		log_err("Invalid private data\n");
		return -EINVAL;
	}

	hal_wdt_write(data->timeout);
	return 0;
}

static int __wdt_set_timeout(struct watchdog_device *wdd, unsigned int timeout)
{
	struct priv_data *data = (struct priv_data *)wdd->driver_data;
	if (data == NULL) {
		log_err("Invalid private data\n");
		return -EINVAL;
	}

	if (timeout > 255)
		timeout = 255;

	data->timeout = (u8)timeout;
	return 0;
}

static unsigned int __wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct priv_data *data = (struct priv_data *)wdd->driver_data;

	if (data == NULL) {
		log_err("Invalid private data\n");
		return -EINVAL;
	}

	return hal_wdt_read();
}

static long __wdt_ioctl(struct watchdog_device *wdd, unsigned int cmd,
			unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;

	struct priv_data *data = (struct priv_data *)wdd->driver_data;

	switch (cmd) {
	case WDIOC_GETSUPPORT: {
		return copy_to_user(argp, data->desc,
				    sizeof(struct bctrl_desc)) ?
			       -EFAULT :
			       0;
	}

	case WDIOC_GETSTATUS: {
		return put_user(data->is_running ? 1 : 0, p);
	}

	case WDIOC_GETBOOTSTATUS: {
		// Not implemented, return 0
		return put_user(0, p);
	}

	case WDIOC_KEEPALIVE: {
		return __wdt_ping(wdd);
	}

	case WDIOC_SETTIMEOUT: {
		int timeout;

		if (get_user(timeout, p))
			return -EFAULT;

		return __wdt_set_timeout(wdd, (unsigned int)timeout);
	}

	case WDIOC_GETTIMEOUT: {
		return put_user(wdd->timeout, p);
	}

	case WDIOC_GETTIMELEFT: {
		int timeleft = __wdt_get_timeleft(wdd);
		return put_user(timeleft, p);
	}

	default:
		return -ENOTTY;
	}
}

static int __init wdt_init(void)
{
	int ret;

	ret = hal_wdt_init();
	if (ret) {
		log_err("Chip initialization failed\n");
		return ret;
	}

	__priv.desc = hal_wdt_desc();
	__priv.is_running = false;
	__priv.timeout = 60; // Default timeout
	__priv.unit = 0; // Default unit: seconds

	watchdog_set_nowayout(&wdt_dev, true);

	ret = watchdog_register_device(&wdt_dev);
	if (ret) {
		log_err("Watchdog device registration failed\n");
		hal_wdt_exit();
		return ret;
	}

	log_info("Watchdog driver initialized\n");
	return 0;
}

static void __exit wdt_exit(void)
{
	hal_wdt_exit();
	watchdog_unregister_device(&wdt_dev);
	log_info("Watchdog driver exited\n");
}

module_init(wdt_init);
module_exit(wdt_exit);

MODULE_AUTHOR("Avalue Technology Inc.");
MODULE_AUTHOR("Arthur Huang <arthur_huang@avalue.com>");
MODULE_DESCRIPTION("Watchdog Driver for Avalue boards");
MODULE_LICENSE("GPL");
MODULE_VERSION(CONFIG_DRIVER_VERSION);