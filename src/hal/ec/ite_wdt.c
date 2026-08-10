
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/ec/ite.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/wdt.h"

#include "log.h"

extern struct ec_device ite_dev;
extern struct bctrl_desc ite_desc;

struct bctrl_desc *hal_wdt_desc(void)
{
	return &ite_desc;
}

void hal_wdt_start(void)
{
	// no need for ite chips
}

void hal_wdt_stop(void)
{
	// no need for ite chips
}

void hal_wdt_write(u8 time)
{
	ite_dev.ops->write16(ITE_REG_WDT_TIMEOUT, time);
}

u8 hal_wdt_read(void)
{
	return (u8)(ite_dev.ops->read16(ITE_REG_WDT_TIMEOUT) & 0xFF);
}

s32 hal_wdt_init(void)
{
	return ite_dev.ops->probe();
}

void hal_wdt_exit(void)
{
	// no need for ite chips
}