
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
	/* Writing 0 to the two-byte counter is the EC's own disable
	 * semantic (every board's EC_BRAM_map.md: "0 - Disabled WDT"), the
	 * same write the legacy 3.x driver's wdt_stop() performs
	 * (lib/wdt.c -> wdt_set_timeout(0)). nowayout blocked this
	 * function from ever running until the nowayout module parameter
	 * was added; it used to be a literal no-op.
	 */
	ite_dev.ops->write16(ITE_REG_WDT_TIMEOUT, 0);
}

u16 hal_wdt_max_timeout(void)
{
	return 0xFFFF;
}

void hal_wdt_write(u16 time)
{
	ite_dev.ops->write16(ITE_REG_WDT_TIMEOUT, time);
}

u16 hal_wdt_read(void)
{
	return ite_dev.ops->read16(ITE_REG_WDT_TIMEOUT);
}

s32 hal_wdt_init(void)
{
	return ite_dev.ops->probe();
}

void hal_wdt_exit(void)
{
	// no need for ite chips
}
