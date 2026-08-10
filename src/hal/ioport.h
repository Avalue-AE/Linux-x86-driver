
#ifndef __HAL_COMMON_IOPORT_H__
#define __HAL_COMMON_IOPORT_H__

#include <linux/io.h>
#include <linux/types.h>

#include "log.h"

/**
 * hal_port_read8 - Read 8-bit value from I/O port
 * @port: I/O port address
 *
 * Return: 8-bit value read from port
 */
static inline u8 hal_port_read8(u16 port)
{
	u8 v = inb(port);
	log_debug("[0x%04X]: 0x%02X\n", port, v);
	return v;
}

/**
 * hal_port_write8 - Write 8-bit value to I/O port
 * @port: I/O port address
 * @value: Value to write
 */
static inline void hal_port_write8(u16 port, u8 value)
{
	log_debug("[0x%04X]: 0x%02X\n", port, value);
	outb(value, port);
}

/**
 * hal_port_read16 - Read 16-bit value from I/O port
 * @port: I/O port address
 *
 * Return: 16-bit value read from port
 */
static inline u16 hal_port_read16(u16 port)
{
	u16 v = inw(port);
	log_debug("[0x%04X]: 0x%04X\n", port, v);
	return v;
}

/**
 * hal_port_write16 - Write 16-bit value to I/O port
 * @port: I/O port address
 * @value: Value to write
 */
static inline void hal_port_write16(u16 port, u16 value)
{
	log_debug("[0x%04X]: 0x%04X\n", port, value);
	outw(value, port);
}

#endif /** __HAL_COMMON_IOPORT_H__ */
