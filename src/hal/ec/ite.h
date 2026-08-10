
#ifndef __HAL_EC_ITE_H__
#define __HAL_EC_ITE_H__

#include "hal/ec/ec.h"

#define ITE_EC_INDEX_PORT(n) (0x2E + (n) * 2)
#define ITE_EC_DATA_PORT(n) (ITE_EC_INDEX_PORT(n) + 1)

#define ITE_EC_BRAM_INDEX_PORT(n) (0x912 + (n) * 2)
#define ITE_EC_BRAM_DATA_PORT(n) (ITE_EC_BRAM_INDEX_PORT(n) + 1)

#define ITE_REG_DEVICE_ID_HIGH 0x20
#define ITE_REG_DEVICE_ID_LOW 0x21

/** Logical device enable register */
#define ITE_REG_LDN_EN 0x30

/** Base logical device */
#define ITE_LDN_REG 0x07

/** Logical device BRAM */
#define ITE_LDN_BRAM 0x10

/** BRAM bank registers */
#define ITE_REG_BRAM_BANK_HIGH(n) (0x60 + (n) * 2)
#define ITE_REG_BRAM_BANK_LOW(n) (0x61 + (n) * 2)

/** Watchdog timer registers (16-bit) */
#define ITE_REG_WDT_TIMEOUT 0x48

/** Hwmon Voltage registers 0~4 */
#define ITE_REG_HWM_VOLTAGE(n) (0x30 + (n))

/** Hwmon Temperature registers 0~1*/
#define ITE_REG_HWM_TEMPERATURE(n) (0x3B + (n))

/** Hwmon Fan speed registers 0~2 (16-bit) */
#define ITE_REG_HWM_FAN_SPEED(n) (0x35 + (n) * 2)

/** Hwmon PWM registers 0~5 */
#define ITE_REG_HWM_PWM(n) (0x40 + (n))

/**
 * GPIO / DIO registers (it85x8 EC).
 *
 * The register roles are board-configurable, not hard-coded: EC firmware
 * BRAM maps differ per board, so a board sets CONFIG_GPIO_REG_* / the mode
 * selector in its .conf and the generated board.h defines them ahead of this
 * header. Two access models are supported, chosen by CONFIG_GPIO_DIR_FIXED:
 *
 *  0 - configurable direction (common it85x8, e.g. EBM-APLV):
 *        REG_INPUT      pin-level readback
 *        REG_OUTPUT     output latch, read-modify-write
 *        REG_DIRECTION  per-pin mode, 1 = input / 0 = output
 *
 *  1 - fixed direction, split input/output banks (e.g. ESM-KX60G, whose EC
 *      BRAM exposes 0x5A GPI input / 0x5B GPO output, no direction register):
 *        REG_INPUT      input bank
 *        REG_OUTPUT     output bank, read-modify-write
 *        DIR_MASK       bit per logical pin, 1 = output line
 *
 * None of these keys has a fallback here: each names a real EC register, and
 * scripts/config.sh requires every one of them from the board file before any
 * compiler runs.
 */

#endif /** __HAL_EC_ITE_H__ */