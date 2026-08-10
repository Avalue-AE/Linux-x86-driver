/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Fintek F81966 Super I/O Chip Header
 * Hardware Abstraction Layer - SIO
 */

#ifndef __HAL_SIO_F81966_H__
#define __HAL_SIO_F81966_H__

#include <linux/types.h>

#include "hal/sio/sio.h"

// Fintek F81966
#define SIO_DEVICE_ID_F81966 0x1502

/** F81966 Logical Devices */

/** Base Logical Device */
#define F81966_LDN_REG 0x07
/** Logical Device 4 (HWM) */
#define F81966_LD4_HWM 0x04
/** Logical Device 6 (GPIO) */
#define F81966_LD6_DIO 0x06
/** Logical Device 7 (WDT) */
#define F81966_LD7_WDT 0x07

/** F81966 Watchdog Timer Registers */

/** WDT Control Register */
#define F81966_WDT_REG_CONTROL 0xF5
/** WDT Timer Register */
#define F81966_WDT_REG_TIMER 0xF6
/** WDT Power Management Event Register */
#define F81966_WDT_REG_PME 0xFA

/** F81966 WDT Control bits */

/** Enable watchdog timer counting */
#define F81966_WDT_REG_CONTROL_BIT_EN BIT(5)

/** Select time unit (0: 1sec, 1: 60 sec) of watchdog timer by setting this bit. */
#define F81966_WDT_REG_CONTROL_BIT_UNIT BIT(3)

/** F81966 WDT PME bits */

/** Watchdog power management event mask*/

/** 0: disable, 1: enable Watchdog time out output via WDTRST#. */
#define F81966_WDT_REG_PME_BIT_WDTEN BIT(0)

/** F81966 HWM Registers */

/** Base Address High Register */
#define F81966_HWM_REG_BASE_ADDR_H 0x60

/** Base Address Low Register */
#define F81966_HWM_REG_BASE_ADDR_L 0x61

/** F81966 HWM Voltage Registers */
#define F81966_HWM_REG_VOL(x) (0x20 + (x))

// /** F81966 HWM I/O Port Registers */
// /** HWM I/O Port Register - Voltage Reading and Limit - 3VCC reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_3VCC 0x20
// /** HWM I/O Port Register - Voltage Reading and Limit - VIN1 (Vcore) reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_VIN1 0x21
// /** HWM I/O Port Register - Voltage Reading and Limit - VIN2 reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_VIN2 0x22
// /** HWM I/O Port Register - Voltage Reading and Limit - VIN3 reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_VIN3 0x23
// /** HWM I/O Port Register - Voltage Reading and Limit - VIN4 reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_VIN4 0x24
// /** HWM I/O Port Register - Voltage Reading and Limit - VSB3V reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_VSB3V 0x25
// /** HWM I/O Port Register - Voltage Reading and Limit - VBAT reading. The unit of reading is 8mV. */
// #define F81966_HWM_LPC_REG_VOL_VBAT 0x26
// /** HWM I/O Port Register - Voltage Reading and Limit - 5VSB reading. The unit of reading is 8 mV.
//  * The 5VSB voltage to be monitored is internally divided by 3. */
// #define F81966_HWM_LPC_REG_VOL_5VSB 0x27

/** F81966 HWM Temperature Registers */
#define F81966_HWM_REG_TEMP(x) (0x70 + (2 * (x)))

// /** HWM I/O Port Register - Temperature 1 */
// #define F81966_HWM_LPC_REG_TEMP_SYS1 0x72
// /** HWM I/O Port Register - Temperature 2 */
// #define F81966_HWM_LPC_REG_TEMP_SYS2 0x74
// /** HWM I/O Port Register - Temperature CPU */
// #define F81966_HWM_LPC_REG_TEMP_CPU 0x7E

#define F81966_HWM_REG_FAN_SPEED(x) (0xA0 + (16 * (x)))
#define F81966_HWM_REG_FAN_DUTY(x) (0xA2 + (16 * (x)))

// /** HWM I/O Port Register - FAN Mode Select Register */
// #define F81966_HWM_LPC_REG_FAN_MODE 0x96
// /** FAN 2 Mode [3:2] */
// #define F81966_HWM_LPC_REG_FAN_MODE_MASK2 0x0C
// /** FAN 1 Mode [1:0] */
// #define F81966_HWM_LPC_REG_FAN_MODE_MASK1 0x03
// /** Auto fan speed control. Fan speed will follow different temperature by different duty cycle defined in 0xA6 - 0xAE. */
// #define F81966_HWM_LPC_REG_FAN_MODE_AUTO 0x01
// /** Manual mode fan control, user can write expected duty cycle(PWM fan type) or voltage(linear fan type) to 0xA3, and F81966/A will output this desired duty or voltage to control fan speed. */
// #define F81966_HWM_LPC_REG_FAN_MODE_MANUAL 0x03

// /** HWM I/O Port Register - FAN 1 Count High Byte */
// #define F81966_HWM_LPC_REG_FAN_COUNT1_H 0xA0
// /** HWM I/O Port Register - FAN 1 Count Low Byte */
// #define F81966_HWM_LPC_REG_FAN_COUNT1_L 0xA1

// /** HWM I/O Port Register - FAN 1 Duty Write High Byte (Duty mode reserved) */
// #define F81966_HWM_LPC_REG_FAN_DUTY1_H 0xA2
// /** HWM I/O Port Register - FAN 1 Duty Write Low Byte */
// #define F81966_HWM_LPC_REG_FAN_DUTY1_L 0xA3

// /** HWM I/O Port Register - VT 1 Boundary 1 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND1TMP1 0xA6
// /** HWM I/O Port Register - VT 1 Boundary 2 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND2TMP1 0xA7
// /** HWM I/O Port Register - VT 1 Boundary 3 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND3TMP1 0xA8
// /** HWM I/O Port Register - VT 1 Boundary 4 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND4TMP1 0xA9

// /** HWM I/O Port Register - FAN1 SEGMENT 5 SPEED COUNT Register */
// #define F81966_HWM_LPC_REG_FAN_SEC5PEED1 0xAE

// /** HWM I/O Port Register - FAN 2 Count High Byte */
// #define F81966_HWM_LPC_REG_FAN_COUNT2_H 0xB0
// /** HWM I/O Port Register - FAN 2 Count Low Byte */
// #define F81966_HWM_LPC_REG_FAN_COUNT2_L 0xB1

// /** HWM I/O Port Register - FAN 2 Duty Write High Byte (Duty mode reserved) */
// #define F81966_HWM_LPC_REG_FAN_DUTY2_H 0xB2
// /** HWM I/O Port Register - FAN 2 Duty Write Low Byte */
// #define F81966_HWM_LPC_REG_FAN_DUTY2_L 0xB3

// /** HWM I/O Port Register - VT 2 Boundary 1 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND1TMP2 0xB6
// /** HWM I/O Port Register - VT 2 Boundary 2 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND2TMP2 0xB7
// /** HWM I/O Port Register - VT 2 Boundary 3 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND3TMP2 0xB8
// /** HWM I/O Port Register - VT 2 Boundary 4 Temperature Register */
// #define F81966_HWM_LPC_REG_FAN_BOUND4TMP2 0xB9

// /** HWM I/O Port Register - FAN2 SEGMENT 5 SPEED COUNT Register */
// #define F81966_HWM_LPC_REG_FAN_SEC5PEED2 0xBE

#endif /* __HAL_SIO_F81966_H__ */
