
#ifndef __HAL_SIO_COMMON_H__
#define __HAL_SIO_COMMON_H__

#include <linux/types.h>

#include "log.h"
#include "hal/ioport.h"

/**
 * Supper IO Common index port calculation
 * e.g.,
 * for SIO chip number 0, index port is 0x2E
 * for SIO chip number 1, index port is 0x4E
 *@n: SIO chip number (0-based)
 */
#define SIO_INDEX_PORT(n) (0x2E + (n) * 2 * 0x10)

/**
 * Supper IO Common data port calculation
 * e.g.,
 * for SIO chip number 0, data port is 0x2F
 * for SIO chip number 1, data port is 0x4F
 *@n: SIO chip number (0-based)
 */
#define SIO_DATA_PORT(n) (SIO_INDEX_PORT(n) + 1)

#ifndef SIO_DEVICE_ENTER_KEY
#define SIO_DEVICE_ENTER_KEY 0x87
#endif

#ifndef SIO_DEVICE_EXIT_KEY
#define SIO_DEVICE_EXIT_KEY 0xAA
#endif

#define SIO_REG_DEVICE_ID_HIGH 0x20
#define SIO_REG_DEVICE_ID_LOW 0x21

/**
 * @brief Supported Super IO Chips
 */

/**
 * struct sio_config - SIO configuration
 * 
 * @index_port: Index port address
 * @data_port: Data port address
 * @enter_key: Enter configuration key
 * @exit_key: Exit configuration key
 * @ldn_reg: Logical Device Number register address
 */
struct sio_config {
	u16 index_port;
	u16 data_port;
	u8 enter_key;
	u8 exit_key;
	u8 ldn_reg;
};

/**
 * struct sio_operation - SIO operations
 * 
 * @probe: Function to probe the SIO chip
 * @enter: Function to enter SIO configuration mode
 * @exit: Function to exit SIO configuration mode
 * @select: Function to select Logical Device Number
 * @read8: Function to read 8-bit value from SIO register
 * @write8: Function to write 8-bit value to SIO register
 * @read16: Function to read 16-bit value from SIO register
 * @write16: Function to write 16-bit value to SIO register
 */
struct sio_operation {
	s32 (*probe)(void);

	void (*enter)(void);
	void (*exit)(void);
	void (*select)(u8 ldn);

	u8 (*read8)(u8 reg);
	void (*write8)(u8 reg, u8 value);

	u16 (*read16)(u8 reg);
	void (*write16)(u8 reg, u16 value);
};

struct sio_device {
	struct sio_config *cfg;
	struct sio_operation *ops;
};

/* Common SIO helper functions */

/**
 * sio_enter - Enter SIO configuration mode
 * @dev: Pointer to sio_device structure
 */
static inline void sio_enter(struct sio_device *dev)
{
	if (!dev || !dev->cfg) {
		log_err("Invalid device or configuration\n");
		return;
	}

	/* Send enter key sequence */
	hal_port_write8(dev->cfg->index_port, dev->cfg->enter_key);
	hal_port_write8(dev->cfg->index_port, dev->cfg->enter_key);
	log_debug("Entered SIO [0x%04X][0x%04X]\n", dev->cfg->index_port,
		  dev->cfg->data_port);
}

/**
 * sio_exit - Exit SIO configuration mode
 * @dev: Pointer to sio_device structure
 */
static inline void sio_exit(struct sio_device *dev)
{
	if (!dev || !dev->cfg) {
		log_err("Invalid device or configuration\n");
		return;
	}

	hal_port_write8(dev->cfg->index_port, dev->cfg->exit_key);
	log_debug("Exited SIO [0x%04X][0x%04X]\n", dev->cfg->index_port,
		  dev->cfg->data_port);
}

/**
 * sio_read8 - Read from SIO register
 * @dev: Pointer to sio_device structure
 * @reg: Register to read from
 * @return: Value read from the register
 */
static inline u8 sio_read8(struct sio_device *dev, u8 reg)
{
	u8 val;

	if (!dev || !dev->cfg) {
		log_err("Invalid device or configuration\n");
		return 0xFF;
	}

	hal_port_write8(dev->cfg->index_port, reg);
	val = hal_port_read8(dev->cfg->data_port);
	log_debug("SIO[0x%04X][0x%04X] read8[0x%02X]: 0x%02X\n",
		  dev->cfg->index_port, dev->cfg->data_port, reg, val);
	return val;
}

/** 
 * sio_write8 - Write to SIO register
 * @dev: Pointer to sio_device structure
 * @reg: Register to write to
 * @val: Value to write
 */
static inline void sio_write8(struct sio_device *dev, u8 reg, u8 val)
{
	if (!dev || !dev->cfg) {
		log_err("Invalid device or configuration\n");
		return;
	}

	hal_port_write8(dev->cfg->index_port, reg);
	hal_port_write8(dev->cfg->data_port, val);
	log_debug("SIO[0x%04X][0x%04X] write8[0x%02X]: 0x%02X\n",
		  dev->cfg->index_port, dev->cfg->data_port, reg, val);
}

/**
 * sio_select_ldn - Select Logical Device Number
 * 
 * @dev: Pointer to sio_device structure
 * @ldn: Logical Device Number to select
 */
static inline void sio_select_ldn(struct sio_device *dev, u8 ldn)
{
	sio_write8(dev, dev->cfg->ldn_reg, ldn);
	log_debug("SIO[0x%04X][0x%04X] selected LDN: 0x%02X\n",
		  dev->cfg->index_port, dev->cfg->data_port, ldn);
}

#endif /* __HAL_SIO_COMMON_H__ */
