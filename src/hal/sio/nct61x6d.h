/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Nuvoton NCT6106D / NCT6126D Super I/O Chip Header
 * Hardware Abstraction Layer - SIO
 *
 * Ported from the legacy driver (Avalue_driver_deprecate/core/nct61x6d.*).
 */

#ifndef __HAL_SIO_NCT61X6D_H__
#define __HAL_SIO_NCT61X6D_H__

#include <linux/types.h>

#include "hal/sio/sio.h"

/*
 * Device identification. The family is identified by the high byte of the
 * device-ID register (0x20); the low byte (0x21) is the revision. Two
 * variants are supported, matching the legacy driver.
 */
#define NCT6106D_ID_HIGH 0xC4
#define NCT6126D_ID_HIGH 0xD2

/** Logical Device Number select register */
#define NCT61X6D_LDN_REG 0x07

/** Logical Devices */
#define NCT61X6D_LD_GPIO 0x07
#define NCT61X6D_LD_WDT 0x08
#define NCT61X6D_LD_HWM 0x0B

/** GPIO logical device */
#define NCT61X6D_GPIO_REG_STATUS 0x30
#define NCT61X6D_GPIO_ONLY_7_ACTIVE 0x80

/** Watchdog logical device (LD 0x08) */
#define NCT61X6D_WDT_REG_CONTROL 0xF0
#define NCT61X6D_WDT_REG_TIMEOUT 0xF1
#define NCT61X6D_WDT_REG_CSR 0xF2
#define NCT61X6D_WDT_REG_GPIO_MULTI 0xE7
#define NCT61X6D_WDT_REG_STATUS 0x30

#define NCT61X6D_WDT_STATUS_INACTIVE 0x00
#define NCT61X6D_WDT_STATUS_ACTIVE 0x01

#define NCT61X6D_WDT_MODE_SECOND 0x00
#define NCT61X6D_WDT_MODE_MINUTE 0x08
#define NCT61X6D_WDT_CONTROL_CLEAR 0x20

/** HWM logical device (LD 0x0B) */
#define NCT61X6D_HWM_REG_BANK_SEL 0x4E
#define NCT61X6D_HWM_REG_BASE_ADDR_H 0x60
#define NCT61X6D_HWM_REG_BASE_ADDR_L 0x61

/*
 * HWM banks. The controller multiplexes its channel registers through a bank
 * select register (0x4E, bit7 preserved). VIN / TEMP / FAN-RPM share bank 0;
 * PWM and FAN-MODE live in bank 1.
 */
#define NCT61X6D_HWM_BANK_VIN 0
#define NCT61X6D_HWM_BANK_TEMP 0
#define NCT61X6D_HWM_BANK_FAN_RPM 0
#define NCT61X6D_HWM_BANK_FAN_MODE 1
#define NCT61X6D_HWM_BANK_PWM 1

/*
 * HWM channel register reference (for .conf authors).
 *
 * These are NOT macros: config.sh wraps any bare UPPER_CASE token in quotes,
 * so a CONFIG_HWM_*_REG that names a macro would emit a broken string. The
 * per-channel registers are non-contiguous (no base+n formula), so board
 * .conf files must use the raw literal offset below in CONFIG_HWM_*_REG.
 *
 *   Voltage (bank 0):
 *     VCORE 0x00  VIN1 0x01  VIN2 0x04  VIN3 0x05
 *     AVCC  0x02  3VSB 0x07  3VCC 0x03  VBAT 0x08
 *   Temperature (bank 0):
 *     CPU 0x10  PECI 0x11  SYS 0x12
 *   Fan tachometer (bank 0, 16-bit big-endian at reg / reg+1):
 *     CPU 0x32  SYS 0x30  AUX 0x34
 *   PWM duty (bank 1):
 *     PWM0 0x29  PWM1 0x19
 *
 * Voltage step (CONFIG_HWM_VOLTAGE_n_LSB): the legacy SIO path multiplied the
 * raw reading by the per-channel unit (8 mV for most rails, 16 mV for AVCC /
 * 3VCC) with no divider, so set LSB to 8 or 16 and leave R1=R2=0.
 *
 * Fan step (CONFIG_HWM_FAN_n_SPEED_STEP): RPM = step / raw_count, with the
 * Nuvoton 1.35 MHz tachometer base clock -> use 1350000.
 */

#endif /* __HAL_SIO_NCT61X6D_H__ */
