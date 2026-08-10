
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/stringify.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"

#include "hal/ec/ec.h"
#include "hal/ec/ite.h"

#include "log.h"

/*
 * The board file names one of these seven ITE EC parts in CONFIG_CHIPID (see
 * the chip list in the comment block below). A value outside this set cannot
 * be an ITE EC this HAL knows how to drive, so catch a typo or a copy-paste
 * from the wrong board before the module links, not at probe time on real
 * hardware.
 */
_Static_assert(CONFIG_CHIPID == 0x5571 || CONFIG_CHIPID == 0x5782 ||
			CONFIG_CHIPID == 0x5792 || CONFIG_CHIPID == 0x8518 ||
			CONFIG_CHIPID == 0x8519 || CONFIG_CHIPID == 0x8528 ||
			CONFIG_CHIPID == 0x8987,
	      "CONFIG_CHIPID " __stringify(CONFIG_CHIPID) " is not one of the ITE EC chips this HAL supports (0x5571, 0x5782, 0x5792, 0x8518, 0x8519, 0x8528, 0x8987)");

static const u16 __ite_ec_index_ports[] = { ITE_EC_INDEX_PORT(0),
					    ITE_EC_INDEX_PORT(1), 0x914 };
static const u16 __ite_ec_data_ports[] = { ITE_EC_DATA_PORT(0),
					   ITE_EC_DATA_PORT(1), 0x915 };

static struct ec_config __ite_ec_cfg = {
	.index_port = 0,
	.data_port = 0,

	.bram_data_port = 0,
	.bram_index_port = 0,
	.bram_offset = 0,
};

static s32 __ite_probe(void);
static u8 __ite_read8(u8 reg);
static void __ite_write8(u8 reg, u8 val);
static u16 __ite_read16(u8 reg);
static void __ite_write16(u8 reg, u16 val);

static struct ec_operation __ite_ec_ops = {
	.probe = __ite_probe,
	.read8 = __ite_read8,
	.write8 = __ite_write8,
	.read16 = __ite_read16,
	.write16 = __ite_write16,
};

struct ec_device ite_dev = {
	.cfg = &__ite_ec_cfg,
	.ops = &__ite_ec_ops,
};

struct bctrl_desc ite_desc = {
	.type = BCTRL_TYPE_EC,
	.mask = (BCTRL_DRIVER_WATCHDOG | BCTRL_DRIVER_HWMON |
		 BCTRL_DRIVER_GPIO),
	.chip_id = 0,
	.priv = (void *)&ite_dev,
};

/**
 * Embedded Controller ITE Chips operations
 * 
 * The ITE Chips communicates with the host system through standard I/O ports
 * (typically 0x912/0x913 for index/data access) using embedded controller
 * protocol. This HAL module provides low-level register access operations
 * for upper layer drivers (watchdog, hwmon, GPIO) to interact with the
 * embedded controller hardware.
 * 
 * We are maintaining the following ITE EC chip variants:
 *   - IT5571 (0x5571)
 *   - IT5782 (0x5782)
 *   - IT5792 (0x5792)
 *   - IT8518 (0x8518)
 *   - IT8519 (0x8519)
 *   - IT8528 (0x8528)
 *   - IT8987 (0x8987)
 */

static u16 __ite_get_device_id(void)
{
	u8 high, low;

	high = ec_read8(&ite_dev, ITE_REG_DEVICE_ID_HIGH);
	low = ec_read8(&ite_dev, ITE_REG_DEVICE_ID_LOW);

	return UINT16_FROM_BYTES(high, low);
}

/**
  * ite_probe - Probe for ITE EC
  * Return: 0 if detected, -ENODEV if not found
  */
static s32 __ite_probe(void)
{
	u8 i = 0, buf = 0;
	u16 id = 0;
	u16 addr;

	for (i = 0; i < 3; i++) {
		__ite_ec_cfg.index_port = __ite_ec_index_ports[i];
		__ite_ec_cfg.data_port = __ite_ec_data_ports[i];
		id = __ite_get_device_id();
		if (id != 0 && id != 0xFFFF)
			break;
	}

	log_debug("ITE EC index port: 0x%02X, data port: 0x%02X\n",
		  __ite_ec_cfg.index_port, __ite_ec_cfg.data_port);

	log_debug("Probing ITE EC: ID = 0x%04X\n", id);

	if (id == 0 || id == 0xFFFF)
		return -ENODEV;

	if (id != CONFIG_CHIPID) {
		log_warn("ITE EC chip id mismatch: board declares 0x%04X, found 0x%04X -- "
			 "continuing, CONFIG_CHIPID for this board is not yet hardware-verified\n",
			 CONFIG_CHIPID, id);
	}

	ite_desc.chip_id = id; // identify chip id into desc

	ec_write8(&ite_dev, ITE_LDN_REG, ITE_LDN_BRAM);
	buf = ec_read8(&ite_dev, ITE_REG_LDN_EN);
	if ((buf & 0x01) == 0)
		return -ENODEV; // no bram in current chip

	addr = UINT16_FROM_BYTES( //
		ec_read8(&ite_dev, ITE_REG_BRAM_BANK_HIGH(0)), //
		ec_read8(&ite_dev, ITE_REG_BRAM_BANK_LOW(0)) //
	);

	if (addr == 0)
		return -ENODEV; // no bram address found in current chip

	ite_dev.cfg->bram_index_port = addr;
	ite_dev.cfg->bram_data_port = addr + 1;

	log_debug("BRAM index port: 0x%02X, BRAM data port: 0x%02X\n",
		  ite_dev.cfg->bram_index_port, ite_dev.cfg->bram_data_port);

	switch (id) {
	case 0x5571:
	case 0x5782:
	case 0x5792:
	case 0x8528:
		ite_dev.cfg->bram_offset = 0x80;
		break;
	}

	return 0;
}

/*
 * __ite_read8/__ite_write8/__ite_read16/__ite_write16 below are the runtime
 * hot path shared by every ITE-chipset board's GPIO, HWM, MISC, and WDT HAL --
 * but each of those subsystems builds as its own separate kernel module
 * (gpio.ko, hwm.ko, misc.ko, wdt.ko), with this file linked into every one of
 * them independently. A spinlock here would give each .ko its own private
 * copy in its own .bss and serialize nothing across modules. Arbitrate the
 * shared bram_index_port/bram_data_port range instead with
 * request_muxed_region()/release_region(), which key off the kernel's global
 * ioport_resource tree by physical port range rather than by module identity
 * -- the same pattern upstream uses to mux a shared Super-I/O config port
 * across independently loaded drivers (e.g. drivers/watchdog/f71808e_wdt.c
 * and drivers/hwmon/f71882fg.c on 0x2e/0x4e). It blocks instead of spinning
 * when another muxed holder owns the range, which is fine here: every path
 * into these ops is process-context sysfs show/store or the watchdog core's
 * ping/start/stop callback, never irq/atomic context.
 */
static struct resource *__ite_lock(void)
{
	struct resource *res;

	res = request_muxed_region(ite_dev.cfg->bram_index_port, 2, "ite-ec");
	if (!res)
		log_err("failed to acquire ITE EC BRAM port range 0x%04X-0x%04X\n",
			ite_dev.cfg->bram_index_port,
			ite_dev.cfg->bram_index_port + 1);

	return res;
}

static void __ite_unlock(struct resource *res)
{
	if (!res)
		return;

	release_region(ite_dev.cfg->bram_index_port, 2);
}

/*
 * Raw, lock-free port I/O -- callers must already hold the bram port range
 * (a successful __ite_lock()). __ite_read16/__ite_write16 need both halves
 * of their 16-bit access to run under a single lock/unlock pair, so they
 * call these directly instead of going back through the locking
 * __ite_read8/__ite_write8.
 */
static u8 __raw_ite_read8(u8 reg)
{
	hal_port_write8(ite_dev.cfg->bram_index_port,
			reg | ite_dev.cfg->bram_offset);

	return hal_port_read8(ite_dev.cfg->bram_data_port);
}

static void __raw_ite_write8(u8 reg, u8 val)
{
	hal_port_write8(ite_dev.cfg->bram_index_port,
			reg | ite_dev.cfg->bram_offset);
	hal_port_write8(ite_dev.cfg->bram_data_port, val);
}

static u8 __ite_read8(u8 reg)
{
	struct resource *res;
	u8 val;

	res = __ite_lock();
	if (!res)
		return 0;

	val = __raw_ite_read8(reg);
	__ite_unlock(res);

	return val;
}

static void __ite_write8(u8 reg, u8 val)
{
	struct resource *res;

	res = __ite_lock();
	if (!res)
		return;

	__raw_ite_write8(reg, val);
	__ite_unlock(res);
}

static u16 __ite_read16(u8 reg)
{
	struct resource *res;
	u8 low, high;

	res = __ite_lock();
	if (!res)
		return 0;

	high = __raw_ite_read8(reg);
	low = __raw_ite_read8(reg + 1);
	__ite_unlock(res);

	return UINT16_FROM_BYTES(high, low);
}

static void __ite_write16(u8 reg, u16 val)
{
	struct resource *res;

	res = __ite_lock();
	if (!res)
		return;

	__raw_ite_write8(reg, UINT16_HIBYTE(val));
	__raw_ite_write8(reg + 1, UINT16_LOBYTE(val));
	__ite_unlock(res);
}