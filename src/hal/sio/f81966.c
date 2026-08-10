
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/stringify.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"

#include "hal/sio/sio.h"
#include "hal/sio/f81966.h"

#include "log.h"

/*
 * This HAL only ever drives an F81966, so the board's declared CONFIG_CHIPID
 * must be that one id. A different value is a typo or a copy-paste from the
 * wrong board's .conf; catch it before the module links, not at probe time.
 */
_Static_assert(CONFIG_CHIPID == SIO_DEVICE_ID_F81966,
	      "CONFIG_CHIPID " __stringify(CONFIG_CHIPID) " does not match the F81966 device id (" __stringify(SIO_DEVICE_ID_F81966) ")");

/** F81966 Device Private data */
static struct sio_config __f81966_sio_cfg = {
	.index_port = 0,
	.data_port = 0,
	.enter_key = SIO_DEVICE_ENTER_KEY,
	.exit_key = SIO_DEVICE_EXIT_KEY,
	.ldn_reg = F81966_LDN_REG,
};

/* Chip operations structure */

static s32 __f81966_probe(void);
static void __f81966_enter(void);
static void __f81966_exit(void);
static void __f81966_select(u8 ldn);
static u8 __f81966_read8(u8 reg);
static void __f81966_write8(u8 reg, u8 value);
static u16 __f81966_read16(u8 reg);
static void __f81966_write16(u8 reg, u16 value);

static struct sio_operation __f81966_sio_ops = {
	.probe = __f81966_probe,
	.enter = __f81966_enter,
	.exit = __f81966_exit,
	.select = __f81966_select,
	.read8 = __f81966_read8,
	.write8 = __f81966_write8,
	.read16 = __f81966_read16,
	.write16 = __f81966_write16,
};

struct sio_device f81966_dev = {
	.cfg = &__f81966_sio_cfg,
	.ops = &__f81966_sio_ops,
};

struct bctrl_desc f81966_desc = {
	.type = BCTRL_TYPE_SIO,
	.mask = (BCTRL_DRIVER_WATCHDOG),
	.chip_id = SIO_DEVICE_ID_F81966,
	.priv = (void *)&f81966_dev,
};

static u16 __f81966_get_device_id(void)
{
	u8 high, low;

	high = sio_read8(&f81966_dev, SIO_REG_DEVICE_ID_HIGH);
	low = sio_read8(&f81966_dev, SIO_REG_DEVICE_ID_LOW);

	return UINT16_FROM_BYTES(high, low);
}

/**
 * __f81966_probe - Probe and detect F81966 chip
 * Return: 0 if F81966 detected, -ENODEV if not found
 */
static s32 __f81966_probe(void)
{
	u8 found = 0;
	u16 device_id;
	u8 i = 0;

	for (i = 0; i < 2; i++) {
		__f81966_sio_cfg.index_port = SIO_INDEX_PORT(i);
		__f81966_sio_cfg.data_port = SIO_DATA_PORT(i);

		sio_enter(&f81966_dev);
		device_id = __f81966_get_device_id();
		
		log_debug("Probing F81966 at 0x%02X: Device ID = 0x%02X\n",
			  __f81966_sio_cfg.index_port, device_id);

		if (device_id == CONFIG_CHIPID) {
			found = 1;
			break;
		}
		sio_exit(&f81966_dev);
	}

	if (!found) {
		log_err("F81966 not detected\n");
		return -ENODEV;
	}

	log_debug("Detected (Device ID: 0x%04X)\n", device_id);

	return 0;
}

/**
 * __f81966_enter - Enter F81966 configuration mode
 */
static void __f81966_enter(void)
{
	sio_enter(&f81966_dev);
}

/**
 * __f81966_exit - Exit F81966 configuration mode
 */
static void __f81966_exit(void)
{
	sio_exit(&f81966_dev);
}

/**
 * __f81966_select - Select Logical Device Number
 * @ldn: Logical Device Number to select
 */
static void __f81966_select(u8 ldn)
{
	sio_write8(&f81966_dev, F81966_LDN_REG, ldn);
	log_debug("F81966 selected LDN: 0x%02X\n", ldn);
}

/**
 * __f81966_read8 - Read from F81966 register
 * @reg: Register address
 * @val: Pointer to store read value
 *
 * Return: 0 on success, negative error code on failure
 */
static u8 __f81966_read8(u8 reg)
{
	u8 val = 0;
	sio_enter(&f81966_dev);
	val = sio_read8(&f81966_dev, reg);
	sio_exit(&f81966_dev);
	log_debug("F81966 read8[0x%02X]: 0x%02X\n", reg, val);
	return val;
}

/**
 * __f81966_write8 - Write to F81966 register
 * @reg: Register address
 * @value: Value to write
 *
 * Return: 0 on success, negative error code on failure
 */
static void __f81966_write8(u8 reg, u8 value)
{
	sio_enter(&f81966_dev);
	sio_write8(&f81966_dev, reg, value);
	sio_exit(&f81966_dev);
	log_debug("F81966 write8[0x%02X]: 0x%02X\n", reg, value);
}

/**
 * __f81966_read16 - Read 16-bit value from F81966 register
 * 
 * @reg: Register address
 * @val: Pointer to store read value
 * 
 * Return: 0 on success, negative error code on failure	
 */
static u16 __f81966_read16(u8 reg)
{
	u8 low, high;

	sio_enter(&f81966_dev);
	high = sio_read8(&f81966_dev, reg);
	low = sio_read8(&f81966_dev, reg + 1);
	sio_exit(&f81966_dev);

	log_debug("F81966 read16[0x%02X]: 0x%02X 0x%02X\n", reg, high, low);
	return UINT16_FROM_BYTES(high, low);
}

/**
 * __f81966_write16 - Write 16-bit value to F81966 register
 * 
 * @reg: Register address
 * @value: Value to write
 * 
 * Return: 0 on success, negative error code on failure
 */
static void __f81966_write16(u8 reg, u16 value)
{
	u8 low = UINT16_LOBYTE(value);
	u8 high = UINT16_HIBYTE(value);

	sio_enter(&f81966_dev);
	sio_write8(&f81966_dev, reg, high);
	sio_write8(&f81966_dev, reg + 1, low);
	sio_exit(&f81966_dev);

	log_debug("F81966 write16[0x%02X]: 0x%02X 0x%02X\n", reg, high, low);
}
