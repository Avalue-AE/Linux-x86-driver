
#ifndef __HAL_EC_COMMON_H__
#define __HAL_EC_COMMON_H__

#include <linux/types.h>

#include "log.h"
#include "hal/ioport.h"

/**
 * struct ec_config - EC configuration
 * 
 * @index_port: Index port address
 * @data_port: Data port address
 */
struct ec_config {
	u16 index_port; // lpc index port
	u16 data_port; // lpc data port

	u16 bram_index_port;
	u16 bram_data_port;
	u8 bram_offset;
};

/**
 * struct ec_operation - EC operations
 * 
 * @probe: Function pointer to probe the EC device
 * @read8: Function to read 8-bit value from EC register
 * @write8: Function to write 8-bit value to EC register
 * 
 */
struct ec_operation {
	s32 (*probe)(void);
	u8 (*read8)(u8 reg);
	void (*write8)(u8 reg, u8 val);
	u16 (*read16)(u8 reg);
	void (*write16)(u8 reg, u16 val);
};

struct ec_device {
	struct ec_config *cfg;
	struct ec_operation *ops;
};

/**
 * ec_read8 - Read from ec register
 * @dev: Pointer to ec_device struct
 * @reg: Register to read from
 * @return: Value read from the register
 */
static inline u8 ec_read8(struct ec_device *dev, u8 reg)
{
	u8 val;

	if (!dev || !dev->cfg) {
		log_err("Invalid EC device or configuration");
		return 0;
	}
	hal_port_write8(dev->cfg->index_port, reg);
	val = hal_port_read8(dev->cfg->data_port);
	log_debug("EC[0x%04X][0x%04X] read8[0x%02X]: 0x%02X",
		  dev->cfg->index_port, dev->cfg->data_port, reg, val);
	return val;
}

static inline void ec_write8(struct ec_device *dev, u8 reg, u8 val)
{
	if (!dev || !dev->cfg) {
		log_err("Invalid EC device or configuration");
		return;
	}

	hal_port_write8(dev->cfg->index_port, reg);
	hal_port_write8(dev->cfg->data_port, val);

	log_debug("EC[0x%04X][0x%04X] write8[0x%02X]: 0x%02X",
		  dev->cfg->index_port, dev->cfg->data_port, reg, val);
}

#endif /* __HAL_EC_COMMON_H__ */
