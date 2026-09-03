
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
	u16 timeout; // depends on unit (seconds or minutes)
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
		    /* Required before nowayout=0 is offered: the
		     * core's watchdog_release() (watchdog_dev.c) stops the
		     * watchdog on close when EITHER the caller wrote the
		     * magic 'V' character OR this flag is absent. Without
		     * it, the day nowayout=0 ships, every close of
		     * /dev/watchdog0 -- including the close the kernel
		     * performs when a customer's daemon crashes -- would
		     * stop the watchdog, leaving the board unprotected with
		     * no reset and no message. With it, only a close
		     * preceded by a deliberate 'V' write stops it.
		     */
		    | WDIOF_MAGICCLOSE // /**< Supports magic close char */
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
	/* Advertised only. The kernel core enforces this floor in its own
	 * WDIOC_SETTIMEOUT handler (watchdog_set_timeout() ->
	 * watchdog_timeout_invalid(), drivers/watchdog/watchdog_dev.c), a
	 * path this driver never reaches: __wdt_ioctl() answers
	 * WDIOC_SETTIMEOUT itself and deliberately lets 0 through
	 *.
	 */
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

	if (timeout > wdd->max_timeout)
		timeout = wdd->max_timeout;

	/* timeout == 0 is deliberate and reaches the chip unchanged: it
	 * disables the watchdog timer (Arthur's ruling, an earlier finding). Do not
	 * add a floor here.
	 */
	data->timeout = (u16)timeout;
	wdd->timeout = timeout;
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
		return copy_to_user(argp, &wdt_info, sizeof(wdt_info)) ?
			       -EFAULT :
			       0;
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
		int err;

		if (get_user(timeout, p))
			return -EFAULT;

		err = __wdt_set_timeout(wdd, (unsigned int)timeout);
		if (err < 0)
			return err;

		if (data->is_running) {
			err = __wdt_ping(wdd);
			if (err < 0)
				return err;
		}

		return put_user(wdd->timeout, p);
	}

	case WDIOC_GETTIMEOUT: {
		return put_user(wdd->timeout, p);
	}

	case WDIOC_GETTIMELEFT: {
		int timeleft = __wdt_get_timeleft(wdd);
		return put_user(timeleft, p);
	}

	default:
		/* Every command this switch does not name -- WDIOC_GETSTATUS,
		 * WDIOC_SETOPTIONS, WDIOC_SETPRETIMEOUT, WDIOC_GETPRETIMEOUT --
		 * falls through here to the kernel core.
		 * -ENOIOCTLCMD tells the core to take over and answer the
		 * command itself; -ENOTTY told it the command does not
		 * exist and stopped it right there. The core's own
		 * WDIOC_GETSTATUS then answers with real WDIOF_* flags
		 * (watchdog_get_status()). WDIOC_SETOPTIONS's
		 * WDIOS_ENABLECARD reaches this driver's own __wdt_start()
		 * through the core's watchdog_start(); WDIOS_DISABLECARD
		 * reaches __wdt_stop() through the core's watchdog_stop()
		 * only when the nowayout module parameter is loaded as 0.
		 * On the default (nowayout=1), watchdog_stop() still
		 * refuses with -EBUSY before it ever calls this driver.
		 */
		return -ENOIOCTLCMD;
	}
}

/* Default on: a customer who does nothing sees exactly today's behaviour --
 * the watchdog cannot be stopped by software. Setting
 * nowayout=0 unlocks WDIOS_DISABLECARD (via WDIOC_SETOPTIONS) and the
 * magic-close sequence as real ways to stop it -- see WDIOF_MAGICCLOSE above
 * and hal_wdt_stop() for the preconditions that make turning this off safe.
 */
static bool nowayout = true;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		 "Once the watchdog is running, do not allow it to be stopped in software (default=true). Set nowayout=0 to allow WDIOS_DISABLECARD and the magic-close sequence to stop it.");

static int __init wdt_init(void)
{
	int ret;

	ret = hal_wdt_init();
	if (ret) {
		log_err("Chip initialization failed\n");
		return ret;
	}
	wdt_dev.max_timeout = hal_wdt_max_timeout();

	__priv.desc = hal_wdt_desc();
	__priv.is_running = false;
	__priv.timeout = (u16)wdt_dev.timeout; // Default timeout
	__priv.unit = 0; // Default unit: seconds

	watchdog_set_nowayout(&wdt_dev, nowayout);
	log_info("nowayout=%d: watchdog %s be stopped by software once running\n",
		 nowayout, nowayout ? "cannot" : "can");

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
