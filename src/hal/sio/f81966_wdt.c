

#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/sio/f81966.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/wdt.h"

#include "log.h"

/** F81966 Watchdog descriptor */
struct bctrl_desc *hal_wdt_desc(void);

/** F81966 Watchdog initialization */
s32 hal_wdt_init(void);

/** F81966 Watchdog deinitialization */
void hal_wdt_exit(void);

/** F81966 Watchdog start */
void hal_wdt_start(void);

/** F81966 Watchdog stop */
void hal_wdt_stop(void);

/** F81966 Watchdog write timeout value */
void hal_wdt_write(u8 time);

/** F81966 Watchdog read timeout value */
u8 hal_wdt_read(void);

extern struct sio_device f81966_dev;
extern struct bctrl_desc f81966_desc;

struct bctrl_desc *hal_wdt_desc(void)
{
	return &f81966_desc;
}

s32 hal_wdt_init(void)
{
	return f81966_dev.ops->probe();
}

void hal_wdt_exit(void)
{
	// No specific deinitialization needed for F81966 WDT
}

void hal_wdt_start(void)
{
	u8 control;

	f81966_dev.ops->enter();
	f81966_dev.ops->select(F81966_LD7_WDT);
	log_debug("Configuring F81966 WDT registers\n");

	// Enable WDT PME
	control = f81966_dev.ops->read8(F81966_WDT_REG_PME);
	control |= F81966_WDT_REG_PME_BIT_WDTEN;
	f81966_dev.ops->write8(F81966_WDT_REG_PME, control);
	log_debug("WDT PME enabled 0x%02x\n", control);

	// Enable WDT Register
	control = f81966_dev.ops->read8(F81966_WDT_REG_CONTROL);
	control |= F81966_WDT_REG_CONTROL_BIT_EN;
	// control |= F81966_WDT_REG_CONTROL_BIT_UNIT; // 0: default to seconds
	f81966_dev.ops->write8(F81966_WDT_REG_CONTROL, control);
	log_debug("WDT Control enabled 0x%02x\n", control);

	f81966_dev.ops->exit();
}

void hal_wdt_stop(void)
{
	u8 control;

	f81966_dev.ops->enter();
	f81966_dev.ops->select(F81966_LD7_WDT);
	log_debug("Disabling F81966 WDT registers\n");

	// Disable WDT PME
	control = f81966_dev.ops->read8(F81966_WDT_REG_CONTROL);
	control &= ~F81966_WDT_REG_CONTROL_BIT_EN;
	f81966_dev.ops->write8(F81966_WDT_REG_CONTROL, control);
	log_debug("WDT Control disabled 0x%02x\n", control);

	// Disable WDT Register
	control = f81966_dev.ops->read8(F81966_WDT_REG_PME);
	control &= ~F81966_WDT_REG_PME_BIT_WDTEN;
	f81966_dev.ops->write8(F81966_WDT_REG_PME, control);
	log_debug("WDT PME disabled 0x%02x\n", control);

	f81966_dev.ops->exit();
}

void hal_wdt_write(u8 time)
{
	f81966_dev.ops->enter();
	f81966_dev.ops->select(F81966_LD7_WDT);

	f81966_dev.ops->write8(F81966_WDT_REG_TIMER, time);
	log_debug("Writing WDT timeout value: %u\n", time);

	f81966_dev.ops->exit();
}

u8 hal_wdt_read(void)
{
	u8 time;

	f81966_dev.ops->enter();
	f81966_dev.ops->select(F81966_LD7_WDT);

	time = f81966_dev.ops->read8(F81966_WDT_REG_TIMER);
	log_debug("Reading WDT timeout value: %u\n", time);

	f81966_dev.ops->exit();
	return time;
}