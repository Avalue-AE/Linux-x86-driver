
#ifndef __HAL_COMMON_BYTES_H__
#define __HAL_COMMON_BYTES_H__

#include <linux/types.h>

/**
 * @def UINT16_HIBYTE
 * @brief Extract high byte from u16
 * @param val 16-bit unsigned value
 * @return High byte (MSB)
 */
#define UINT16_HIBYTE(val) ((u8)(((val) >> 8) & 0xFF))

/**
 * @def UINT16_LOBYTE
 * @brief Extract low byte from u16
 * @param val 16-bit unsigned value
 * @return Low byte (LSB)
 */
#define UINT16_LOBYTE(val) ((u8)((val) & 0xFF))

/**
 * @brief Extract low byte from u8
 * @param low Low byte (LSB)
 */
#define UINT16_FROM_LOBYTE(low) (u16)(low)

/**
 * @brief Extract high byte from u8
 * @param high High byte (MSB)
 *
 */
#define UINT16_FROM_HIBYTE(high) ((u16)((u16)(high) << 8))

/**
 * @def UINT16_FROM_BYTES
 * @brief Combine two bytes into u16 (big-endian)
 * @param high High byte (MSB)
 * @param low Low byte (LSB)
 * @return Combined u16 value
 */
#define UINT16_FROM_BYTES(high, low) \
	((u16)(((u16)(high) << 8) | (u16)(low)))

/**
 * @def UINT16_TO_BYTES
 * @brief Write u16 to two separate bytes (big-endian)
 *
 * Splits a 16-bit value into high and low bytes.
 *
 * @param buf1 Variable to receive high byte (MSB)
 * @param buf2 Variable to receive low byte (LSB)
 * @param val 16-bit unsigned value to split
 */
#define UINT16_TO_BYTES(buf1, buf2, val) \
	do {                                 \
		(buf1) = UINT16_HIBYTE(val);     \
		(buf2) = UINT16_LOBYTE(val);     \
	} while (0)

/**
 * @def INT16_HIBYTE
 * @brief Extract high byte from int16_t
 * @param val 16-bit signed value
 * @return High byte (MSB)
 */
#define INT16_HIBYTE(val) ((u8)(((val) >> 8) & 0xFF))

/**
 * @def INT16_LOBYTE
 * @brief Extract low byte from int16_t
 * @param val 16-bit signed value
 * @return Low byte (LSB)
 */
#define INT16_LOBYTE(val) ((u8)((val) & 0xFF))

/**
 * @def INT16_FROM_BYTES
 * @brief Combine two bytes into int16_t (big-endian)
 * @param high High byte (MSB)
 * @param low Low byte (LSB)
 * @return Combined int16_t value
 */
#define INT16_FROM_BYTES(high, low) \
	((int16_t)(((int16_t)(high) << 8) | (int16_t)(low)))

/**
 * @def INT16_TO_BYTES
 * @brief Write int16_t to two separate bytes (big-endian)
 *
 * Splits a 16-bit signed value into high and low bytes.
 *
 * @param buf1 Variable to receive high byte (MSB)
 * @param buf2 Variable to receive low byte (LSB)
 * @param val 16-bit signed value to split
 */
#define INT16_TO_BYTES(buf1, buf2, val) \
	do {                                \
		(buf1) = INT16_HIBYTE(val);     \
		(buf2) = INT16_LOBYTE(val);     \
	} while (0)

#endif /** __HAL_COMMON_BYTES_H__ */