
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/sio/nct61x6d.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/wdt.h"

#include "log.h"

extern struct sio_device nct61x6d_dev;
extern struct bctrl_desc nct61x6d_desc;

struct bctrl_desc *hal_wdt_desc(void)
{
	return &nct61x6d_desc;
}

s32 hal_wdt_init(void)
{
	return nct61x6d_dev.ops->probe();
}

void hal_wdt_exit(void)
{
	// No specific deinitialization needed for NCT61x6D WDT
}

/*
 * Enable the watchdog. Mirrors the legacy sio_wdt_init() bring-up:
 *   1. route GPIO7 as the WDT reset output (GPIO LDN, reg 0x30 bit7),
 *   2. steer the GPIO multi-function to WDT (WDT LDN, reg 0xE7 bit0),
 *   3. activate the WDT logical device and select second-granularity mode.
 * The counter itself is loaded separately via hal_wdt_write().
 */
void hal_wdt_start(void)
{
	u8 val;

	log_debug("Enabling NCT61x6D WDT\n");

	// 1) route GPIO7 as WDT reset output
	nct61x6d_dev.ops->enter();
	nct61x6d_dev.ops->select(NCT61X6D_LD_GPIO);
	val = nct61x6d_dev.ops->read8(NCT61X6D_GPIO_REG_STATUS);
	val |= NCT61X6D_GPIO_ONLY_7_ACTIVE;
	nct61x6d_dev.ops->write8(NCT61X6D_GPIO_REG_STATUS, val);

	// 2) steer GPIO multi-function to WDT, then 3) activate + second mode
	nct61x6d_dev.ops->enter();
	nct61x6d_dev.ops->select(NCT61X6D_LD_WDT);
	val = nct61x6d_dev.ops->read8(NCT61X6D_WDT_REG_GPIO_MULTI);
	val |= 0x01;
	nct61x6d_dev.ops->write8(NCT61X6D_WDT_REG_GPIO_MULTI, val);

	nct61x6d_dev.ops->write8(NCT61X6D_WDT_REG_STATUS,
				 NCT61X6D_WDT_STATUS_ACTIVE);
	nct61x6d_dev.ops->write8(NCT61X6D_WDT_REG_CONTROL,
				 NCT61X6D_WDT_MODE_SECOND);
	nct61x6d_dev.ops->exit();

	log_debug("NCT61x6D WDT enabled (second mode)\n");
}

/*
 * Disable the watchdog: zero the counter and deactivate the logical device.
 */
void hal_wdt_stop(void)
{
	log_debug("Disabling NCT61x6D WDT\n");

	nct61x6d_dev.ops->enter();
	nct61x6d_dev.ops->select(NCT61X6D_LD_WDT);
	nct61x6d_dev.ops->write8(NCT61X6D_WDT_REG_TIMEOUT, 0);
	nct61x6d_dev.ops->write8(NCT61X6D_WDT_REG_STATUS,
				 NCT61X6D_WDT_STATUS_INACTIVE);
	nct61x6d_dev.ops->exit();
}

void hal_wdt_write(u8 time)
{
	nct61x6d_dev.ops->enter();
	nct61x6d_dev.ops->select(NCT61X6D_LD_WDT);
	nct61x6d_dev.ops->write8(NCT61X6D_WDT_REG_TIMEOUT, time);
	nct61x6d_dev.ops->exit();

	log_debug("Writing NCT61x6D WDT timeout value: %u\n", time);
}

u8 hal_wdt_read(void)
{
	u8 time;

	nct61x6d_dev.ops->enter();
	nct61x6d_dev.ops->select(NCT61X6D_LD_WDT);
	time = nct61x6d_dev.ops->read8(NCT61X6D_WDT_REG_TIMEOUT);
	nct61x6d_dev.ops->exit();

	log_debug("Reading NCT61x6D WDT timeout value: %u\n", time);
	return time;
}
