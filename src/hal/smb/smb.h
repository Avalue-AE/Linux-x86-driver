
#ifndef __HAL_SMB_COMMON_H__
#define __HAL_SMB_COMMON_H__

#include <linux/types.h>

#include "log.h"
#include "hal/ioport.h"

#define SMBUS_BASE_PORT CONFIG_SMBUS_BASE_PORT
#define SMBUS_SLAVE_ADDR1 CONFIG_SMBUS_SLAVE_ADDR1
#define SMBUS_SLAVE_ADDR2 CONFIG_SMBUS_SLAVE_ADDR2

#define SMBUS_HOST_STATUS(_port) ((_port) + 0x00)
#define SMBUS_HOST_PROTOCOL(_port) ((_port) + 0x01)
#define SMBUS_HOST_CONTROL(_port) ((_port) + 0x02)
#define SMBUS_HOST_COMMAND(_port) ((_port) + 0x03)
#define SMBUS_HOST_ADDRESS(_port) ((_port) + 0x04)
#define SMBUS_HOST_DATA0(_port) ((_port) + 0x05)
#define SMBUS_HOST_DATA1(_port) ((_port) + 0x06)
#define SMBUS_HOST_BLOCK(_port) ((_port) + 0x07)

#define SMBUS_INPUT_PORT(n) ((0x00 + (n)))
#define SMBUS_OUTPUT_PORT(n) ((0x02 + (n)))
#define SMBUS_POLAR_INVERT(n) ((0x04 + (n)))
#define SMBUS_CONFIGURATION(n) ((0x06 + (n)))

#define SMBUS_ERROR_BUSY BIT(1)
#define SMBUS_ERROR_FAILED BIT(2)
#define SMBUS_ERROR_DEVERR BIT(3)
#define SMBUS_ERROR_TIMEOUT BIT(4)
#define SMBUS_ERROR_COLLISION BIT(5)
#define SMBUS_ERROR_ALERT BIT(6)
#define SMBUS_ERROR_PEC BIT(7)

#define SMBUS_GPIO_PIN_LABEL(_id) "GPIO" #_id
#define SMBUS_GPIO_PIN_MASK(_id) (1 << _id)

/* Forward declaration */
struct smb_device;

struct smb_gpio {
	u16 ngpio; // Number of GPIO pins

	s32 (*probe)(struct smb_device *sdev);
	s32 (*ready)(struct smb_device *sdev);
	s32 (*read8)(struct smb_device *sdev, u8 addr, u8 cmd, u8 *out);
	s32 (*write8)(struct smb_device *sdev, u8 addr, u8 cmd, u8 value);

	s32 (*read_value)(struct smb_device *sdev, u16 *out);
	s32 (*write_value)(struct smb_device *sdev, u16 val);
	s32 (*read_direction)(struct smb_device *sdev, u16 *out);
	s32 (*write_direction)(struct smb_device *sdev, u16 dir);
};

struct smb_config {
	u16 base_port;
	u8 slave_addr1;
	u8 slave_addr2;
};

struct smb_device {
	struct smb_config *cfg;
	struct smb_gpio *gpio;

	// Specific private data for the SMB device
	void *priv;
};

/*
 * Pluggable SMBus host-controller protocol. Exactly one implementation is
 * linked per board, chosen by MAKE_GPIO_PROTOCOL in the board .conf:
 *   i801     -> src/hal/smb/i801.c    (Intel i801-style, base from PCI BAR4)
 *   zhaoxin  -> src/hal/smb/zhaoxin.c (Zhaoxin, base from PCI 00:11.0 cfg reg)
 * Chip HALs (nct5655, pca9555) call these names and are protocol-agnostic.
 */
s32 smb_probe(struct smb_device *dev);
s32 smb_ready(struct smb_device *dev);
s32 smb_read8(struct smb_device *dev, u8 addr, u8 cmd, u8 *out);
s32 smb_write8(struct smb_device *dev, u8 addr, u8 cmd, u8 value);

#endif /** __HAL_SMB_COMMON_H__ */