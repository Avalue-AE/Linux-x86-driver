
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/smb/smb.h"
#include "hal/gpio.h"

#include "log.h"

/*
 * PCA9555 — 16-bit SMBus I/O expander.
 *
 * The register layout (INPUT/OUTPUT/POLARITY/CONFIGURATION port 0/1 at
 * offsets 0x00/0x02/0x04/0x06) is the standard expander map already
 * described by smb.h, so this HAL reuses the shared i801 protocol layer
 * exactly like nct5655 does.
 *
 * The one meaningful difference from the nct5655 HAL is topology: a
 * single PCA9555 is ONE SMBus slave that exposes both 8-bit banks (P0.x
 * = port 0 = bits 0-7, P1.x = port 1 = bits 8-15). Both bytes are read
 * from / written to slave_addr1. This matches the legacy driver, whose
 * i2c_smbus_{read,write}_dio only ever addressed SDIO_DEVICE_ADDR1 for
 * both ports. slave_addr2 is intentionally unused here.
 *
 * The raw 16-bit register value is returned as-is (port 0 -> low byte,
 * port 1 -> high byte); the per-board CONFIG_GPIO_PIN_MAP offsets select
 * which physical bits map to logical GPIO lines.
 */

extern s32 smb_probe(struct smb_device *dev);
extern s32 smb_ready(struct smb_device *dev);
extern s32 smb_read8(struct smb_device *dev, u8 addr, u8 cmd, u8 *out);
extern s32 smb_write8(struct smb_device *dev, u8 addr, u8 cmd, u8 value);

static s32 __pca9555_gpio_read_value(struct smb_device *sdev, u16 *out);
static s32 __pca9555_gpio_write_value(struct smb_device *sdev, u16 val);
static s32 __pca9555_gpio_read_direction(struct smb_device *sdev, u16 *out);
static s32 __pca9555_gpio_write_direction(struct smb_device *sdev, u16 dir);

static struct hal_gpio_device __pca9555_gpio = {
	.direction.full = 0xFFFF, // Default all pins as input
	.value.full = 0x0000, // Default all pins low
};

static struct smb_config __pca9555_smb_cfg = {
	.base_port = SMBUS_BASE_PORT,
	.slave_addr1 = SMBUS_SLAVE_ADDR1,
	.slave_addr2 = SMBUS_SLAVE_ADDR2,
};

static struct smb_gpio gpio = {
	.ngpio = 16, // Number of GPIO pins supported

	.probe = smb_probe,
	.ready = smb_ready,
	.read8 = smb_read8,
	.write8 = smb_write8,

	.read_value = __pca9555_gpio_read_value,
	.write_value = __pca9555_gpio_write_value,
	.read_direction = __pca9555_gpio_read_direction,
	.write_direction = __pca9555_gpio_write_direction,
};

struct smb_device pca9555_dev = {
	.cfg = &__pca9555_smb_cfg,
	.gpio = &gpio,
	.priv = (void *)&__pca9555_gpio,
};

struct bctrl_desc pca9555_desc = {
	.type = BCTRL_TYPE_SMBUS,
	.mask = (BCTRL_DEVICE_GPIO),
	.chip_id = 0x0000, // unused
	.priv = (void *)&pca9555_dev,
};

static struct hal_gpio_device *__to_gpio_device(struct smb_device *dev)
{
	if (dev == NULL) {
		log_err("Invalid SMB device pointer\n");
		return NULL;
	}

	return (struct hal_gpio_device *)dev->priv;
}

static s32 __pca9555_gpio_read_value(struct smb_device *sdev, u16 *out)
{
	struct hal_gpio_device *gdev = __to_gpio_device(sdev);
	s32 ret = 0;

	log_debug("value: 0x%04X\n", gdev->value.full);

	ret = sdev->gpio->read8(sdev, sdev->cfg->slave_addr1,
				SMBUS_INPUT_PORT(0), &gdev->value.byte.low);
	if (ret < 0) {
		log_err("Failed to read SMBus input port 0\n");
		return ret;
	}

	ret = sdev->gpio->read8(sdev, sdev->cfg->slave_addr1,
				SMBUS_INPUT_PORT(1), &gdev->value.byte.high);
	if (ret < 0) {
		log_err("Failed to read SMBus input port 1\n");
		return ret;
	}

	log_debug("value: 0x%04X\n", gdev->value.full);

	*out = gdev->value.full;
	return 0;
}

static s32 __pca9555_gpio_write_value(struct smb_device *sdev, u16 val)
{
	struct hal_gpio_device *gdev = __to_gpio_device(sdev);
	s32 ret = 0;
	u8 low = UINT16_LOBYTE(val);
	u8 high = UINT16_HIBYTE(val);

	log_debug("value: 0x%04X -> 0x%04X\n", gdev->value.full, val);

	ret = sdev->gpio->write8(sdev, sdev->cfg->slave_addr1,
				 SMBUS_OUTPUT_PORT(0), low);
	if (ret < 0) {
		log_err("Failed to write SMBus output port 0\n");
		return ret;
	}

	ret = sdev->gpio->write8(sdev, sdev->cfg->slave_addr1,
				 SMBUS_OUTPUT_PORT(1), high);
	if (ret < 0) {
		log_err("Failed to write SMBus output port 1\n");
		return ret;
	}

	log_debug("value: 0x%04X -> 0x%04X\n", gdev->value.full, val);

	gdev->value.full = val;
	return 0;
}

static s32 __pca9555_gpio_read_direction(struct smb_device *sdev, u16 *out)
{
	struct hal_gpio_device *gdev = __to_gpio_device(sdev);
	s32 ret = 0;

	log_debug("direction: 0x%04X\n", gdev->direction.full);

	ret = sdev->gpio->read8(sdev, sdev->cfg->slave_addr1,
				SMBUS_CONFIGURATION(0),
				&gdev->direction.byte.low);
	if (ret < 0) {
		log_err("Failed to read SMBus config port 0\n");
		return ret;
	}

	ret = sdev->gpio->read8(sdev, sdev->cfg->slave_addr1,
				SMBUS_CONFIGURATION(1),
				&gdev->direction.byte.high);
	if (ret < 0) {
		log_err("Failed to read SMBus config port 1\n");
		return ret;
	}

	log_debug("direction: 0x%04X\n", gdev->direction.full);

	*out = gdev->direction.full;
	return 0;
}

static s32 __pca9555_gpio_write_direction(struct smb_device *sdev, u16 dir)
{
	struct hal_gpio_device *gdev = __to_gpio_device(sdev);
	s32 ret = 0;
	u8 low = UINT16_LOBYTE(dir);
	u8 high = UINT16_HIBYTE(dir);

	log_debug("direction: 0x%04X -> 0x%04X\n", gdev->direction.full, dir);

	ret = sdev->gpio->write8(sdev, sdev->cfg->slave_addr1,
				 SMBUS_CONFIGURATION(0), low);
	if (ret < 0) {
		log_err("Failed to write SMBus config port 0\n");
		return ret;
	}

	ret = sdev->gpio->write8(sdev, sdev->cfg->slave_addr1,
				 SMBUS_CONFIGURATION(1), high);
	if (ret < 0) {
		log_err("Failed to write SMBus config port 1\n");
		return ret;
	}

	log_debug("direction: 0x%04X -> 0x%04X\n", gdev->direction.full, dir);

	gdev->direction.full = dir;
	return 0;
}
