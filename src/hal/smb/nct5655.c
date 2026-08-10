
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

extern s32 smb_probe(struct smb_device *dev);
extern s32 smb_ready(struct smb_device *dev);
extern s32 smb_read8(struct smb_device *dev, u8 addr, u8 cmd, u8 *out);
extern s32 smb_write8(struct smb_device *dev, u8 addr, u8 cmd, u8 value);

static s32 __nct5655_gpio_read_value(struct smb_device *sdev, u16 *out);
static s32 __nct5655_gpio_write_value(struct smb_device *sdev, u16 val);
static s32 __nct5655_gpio_read_direction(struct smb_device *sdev, u16 *out);
static s32 __nct5655_gpio_write_direction(struct smb_device *sdev, u16 dir);

static struct hal_gpio_device __nct5655_gpio = {
	.direction.full = 0xFFFF, // Default all pins as input
	.value.full = 0x0000, // Default all pins low
};

static struct smb_config __nct5655_smb_cfg = {
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

	.read_value = __nct5655_gpio_read_value,
	.write_value = __nct5655_gpio_write_value,
	.read_direction = __nct5655_gpio_read_direction,
	.write_direction = __nct5655_gpio_write_direction,
};

struct smb_device nct5655_dev = {
	.cfg = &__nct5655_smb_cfg,
	.gpio = &gpio,
	.priv = (void *)&__nct5655_gpio,
};

struct bctrl_desc nct5655_desc = {
	.type = BCTRL_TYPE_SMBUS,
	.mask = (BCTRL_DEVICE_GPIO),
	.chip_id = 0x0000, // unused
	.priv = (void *)&nct5655_dev,
};

static struct hal_gpio_device *__to_gpio_device(struct smb_device *dev)
{
	if (dev == NULL) {
		log_err("Invalid SMB device pointer\n");
		return NULL;
	}

	return (struct hal_gpio_device *)dev->priv;
}

static s32 __nct5655_gpio_read_value(struct smb_device *sdev, u16 *out)
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

	ret = sdev->gpio->read8(sdev, sdev->cfg->slave_addr2,
				SMBUS_INPUT_PORT(1), &gdev->value.byte.high);
	if (ret < 0) {
		log_err("Failed to read SMBus input port 1\n");
		return ret;
	}

	log_debug("value: 0x%04X\n", gdev->value.full);
	
	*out = gdev->value.full;
	return 0;
}

static s32 __nct5655_gpio_write_value(struct smb_device *sdev, u16 val)
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

	ret = sdev->gpio->write8(sdev, sdev->cfg->slave_addr2,
				 SMBUS_OUTPUT_PORT(1), high);
	if (ret < 0) {
		log_err("Failed to write SMBus output port 1\n");
		return ret;
	}

	log_debug("value: 0x%04X -> 0x%04X\n", gdev->value.full, val);

	gdev->value.full = val;
	return 0;
}

static s32 __nct5655_gpio_read_direction(struct smb_device *sdev, u16 *out)
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

	ret = sdev->gpio->read8(sdev, sdev->cfg->slave_addr2,
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

static s32 __nct5655_gpio_write_direction(struct smb_device *sdev, u16 dir)
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

	ret = sdev->gpio->write8(sdev, sdev->cfg->slave_addr2,
				 SMBUS_CONFIGURATION(1), high);
	if (ret < 0) {
		log_err("Failed to write SMBus config port 1\n");
		return ret;
	}

	log_debug("direction: 0x%04X -> 0x%04X\n", gdev->direction.full, dir);

	gdev->direction.full = dir;
	return 0;
}
