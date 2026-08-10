
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/stringify.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"

#include "hal/sio/sio.h"
#include "hal/sio/nct61x6d.h"

#include "log.h"

/*
 * This HAL only ever drives an NCT6106D or an NCT6126D, recognised by the
 * high byte of the device id (see __nct61x6d_get_device_id() below). The
 * board's declared CONFIG_CHIPID must fall in one of those two families; a
 * value outside both is a typo or a copy-paste from the wrong board's .conf,
 * caught here before the module links rather than at probe time.
 */
_Static_assert(((CONFIG_CHIPID >> 8) & 0xFF) == NCT6106D_ID_HIGH ||
			((CONFIG_CHIPID >> 8) & 0xFF) == NCT6126D_ID_HIGH,
	      "CONFIG_CHIPID " __stringify(CONFIG_CHIPID) " is not an NCT6106D (0xC4xx) or NCT6126D (0xD2xx) id");

/** NCT61x6D Device Private data */
static struct sio_config __nct61x6d_sio_cfg = {
	.index_port = 0,
	.data_port = 0,
	.enter_key = SIO_DEVICE_ENTER_KEY,
	.exit_key = SIO_DEVICE_EXIT_KEY,
	.ldn_reg = NCT61X6D_LDN_REG,
};

/* Chip operations structure */

static s32 __nct61x6d_probe(void);
static void __nct61x6d_enter(void);
static void __nct61x6d_exit(void);
static void __nct61x6d_select(u8 ldn);
static u8 __nct61x6d_read8(u8 reg);
static void __nct61x6d_write8(u8 reg, u8 value);
static u16 __nct61x6d_read16(u8 reg);
static void __nct61x6d_write16(u8 reg, u16 value);

static struct sio_operation __nct61x6d_sio_ops = {
	.probe = __nct61x6d_probe,
	.enter = __nct61x6d_enter,
	.exit = __nct61x6d_exit,
	.select = __nct61x6d_select,
	.read8 = __nct61x6d_read8,
	.write8 = __nct61x6d_write8,
	.read16 = __nct61x6d_read16,
	.write16 = __nct61x6d_write16,
};

struct sio_device nct61x6d_dev = {
	.cfg = &__nct61x6d_sio_cfg,
	.ops = &__nct61x6d_sio_ops,
};

struct bctrl_desc nct61x6d_desc = {
	.type = BCTRL_TYPE_SIO,
	.mask = (BCTRL_DRIVER_WATCHDOG | BCTRL_DRIVER_HWMON),
	.chip_id = 0x0000, // detected at probe (NCT6106D or NCT6126D)
	.priv = (void *)&nct61x6d_dev,
};

/**
 * __nct61x6d_get_device_id - read the 16-bit device ID at the current ports
 *
 * The family is recognised by the high byte (0xC4 = NCT6106D,
 * 0xD2 = NCT6126D); the low byte is the revision.
 */
static u16 __nct61x6d_get_device_id(void)
{
	u8 high, low;

	high = sio_read8(&nct61x6d_dev, SIO_REG_DEVICE_ID_HIGH);
	low = sio_read8(&nct61x6d_dev, SIO_REG_DEVICE_ID_LOW);

	return UINT16_FROM_BYTES(high, low);
}

/**
 * __nct61x6d_probe - Probe and detect an NCT6106D / NCT6126D chip
 * Return: 0 if detected, -ENODEV if not found
 */
static s32 __nct61x6d_probe(void)
{
	u8 found = 0;
	u16 device_id = 0;
	u8 high = 0;
	u8 i = 0;

	for (i = 0; i < 2; i++) {
		__nct61x6d_sio_cfg.index_port = SIO_INDEX_PORT(i);
		__nct61x6d_sio_cfg.data_port = SIO_DATA_PORT(i);

		sio_enter(&nct61x6d_dev);
		device_id = __nct61x6d_get_device_id();
		high = UINT16_HIBYTE(device_id);

		log_debug("Probing NCT61x6D at 0x%02X: Device ID = 0x%04X\n",
			  __nct61x6d_sio_cfg.index_port, device_id);

		if (high == NCT6106D_ID_HIGH || high == NCT6126D_ID_HIGH) {
			found = 1;
			nct61x6d_desc.chip_id = device_id;
			break;
		}
		sio_exit(&nct61x6d_dev);
	}

	if (!found) {
		log_err("NCT61x6D not detected\n");
		return -ENODEV;
	}

	log_debug("Detected (Device ID: 0x%04X)\n", device_id);

	if (device_id != CONFIG_CHIPID) {
		log_warn("NCT61x6D chip id mismatch: board declares 0x%04X, found 0x%04X -- "
			 "continuing, CONFIG_CHIPID for this board is not yet hardware-verified\n",
			 CONFIG_CHIPID, device_id);
	}

	return 0;
}

/**
 * __nct61x6d_enter - Enter configuration mode
 */
static void __nct61x6d_enter(void)
{
	sio_enter(&nct61x6d_dev);
}

/**
 * __nct61x6d_exit - Exit configuration mode
 */
static void __nct61x6d_exit(void)
{
	sio_exit(&nct61x6d_dev);
}

/**
 * __nct61x6d_select - Select Logical Device Number
 * @ldn: Logical Device Number to select
 */
static void __nct61x6d_select(u8 ldn)
{
	sio_write8(&nct61x6d_dev, NCT61X6D_LDN_REG, ldn);
	log_debug("NCT61x6D selected LDN: 0x%02X\n", ldn);
}

/**
 * __nct61x6d_read8 - Read an 8-bit register
 * @reg: Register address
 */
static u8 __nct61x6d_read8(u8 reg)
{
	u8 val = 0;

	sio_enter(&nct61x6d_dev);
	val = sio_read8(&nct61x6d_dev, reg);
	sio_exit(&nct61x6d_dev);
	log_debug("NCT61x6D read8[0x%02X]: 0x%02X\n", reg, val);
	return val;
}

/**
 * __nct61x6d_write8 - Write an 8-bit register
 * @reg: Register address
 * @value: Value to write
 */
static void __nct61x6d_write8(u8 reg, u8 value)
{
	sio_enter(&nct61x6d_dev);
	sio_write8(&nct61x6d_dev, reg, value);
	sio_exit(&nct61x6d_dev);
	log_debug("NCT61x6D write8[0x%02X]: 0x%02X\n", reg, value);
}

/**
 * __nct61x6d_read16 - Read a 16-bit register (big-endian: reg=high, reg+1=low)
 * @reg: Register address of the high byte
 */
static u16 __nct61x6d_read16(u8 reg)
{
	u8 low, high;

	sio_enter(&nct61x6d_dev);
	high = sio_read8(&nct61x6d_dev, reg);
	low = sio_read8(&nct61x6d_dev, reg + 1);
	sio_exit(&nct61x6d_dev);

	log_debug("NCT61x6D read16[0x%02X]: 0x%02X 0x%02X\n", reg, high, low);
	return UINT16_FROM_BYTES(high, low);
}

/**
 * __nct61x6d_write16 - Write a 16-bit register (big-endian)
 * @reg: Register address of the high byte
 * @value: Value to write
 */
static void __nct61x6d_write16(u8 reg, u16 value)
{
	u8 low = UINT16_LOBYTE(value);
	u8 high = UINT16_HIBYTE(value);

	sio_enter(&nct61x6d_dev);
	sio_write8(&nct61x6d_dev, reg, high);
	sio_write8(&nct61x6d_dev, reg + 1, low);
	sio_exit(&nct61x6d_dev);

	log_debug("NCT61x6D write16[0x%02X]: 0x%02X 0x%02X\n", reg, high, low);
}
