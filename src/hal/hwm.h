
#ifndef __HAL_HWM_H__
#define __HAL_HWM_H__
#include <linux/types.h>

enum hal_hwm_sensor_type {
	HWM_SENSOR_TYPE_NONE = 0,
	HWM_SENSOR_TYPE_VOLTAGE,
	HWM_SENSOR_TYPE_TEMPERATURE,
	HWM_SENSOR_TYPE_FAN_SPEED,
	HWM_SENSOR_TYPE_PWM,
	HWM_SENSOR_TYPE_MAX,
};

struct hal_hwm_sensor {
	const char *label;
	enum hal_hwm_sensor_type type;
	u8 reg;
	u32 lsb; // adc step value in millivolts or millidegrees
	u32 resistor_r1;
	u32 resistor_r2;
	s32 (*read)(struct hal_hwm_sensor *sensor);
	s32 (*write)(struct hal_hwm_sensor *sensor, const char *buf,
		     size_t count);
};

#define HWM_SENSOR_NONE                 \
	{ .label = NULL,                \
	  .type = HWM_SENSOR_TYPE_NONE, \
	  .reg = 0,                     \
	  .lsb = 0,                     \
	  .resistor_r1 = 0,             \
	  .resistor_r2 = 0,             \
	  .read = NULL,                 \
	  .write = NULL }

#define HWM_SENSOR_ATTR(_lab, _type, _reg, _lsb, _r1, _r2, _readfn, _writefn) \
	{ .label = _lab,                                                      \
	  .type = _type,                                                      \
	  .reg = _reg,                                                        \
	  .lsb = _lsb,                                                        \
	  .resistor_r1 = _r1,                                                 \
	  .resistor_r2 = _r2,                                                 \
	  .read = _readfn,                                                    \
	  .write = _writefn }

/**
 * Get the hardware monitor descriptor
 * @return Pointer to the hardware monitor descriptor structure
 */
extern struct bctrl_desc *hal_hwm_desc(void);

/**
 * Get all hardware monitor sensors
 * @return Array of pointers to hal_hwm_sensor structures
 */
extern struct hal_hwm_sensor **hal_hwm_sensors(void);

/**
 * Initialize the hardware monitor
 * @return 0 on success, negative error code on failure
 */
extern s32 hal_hwm_init(void);

/**
 * Exit the hardware monitor
 */
extern void hal_hwm_exit(void);

/**
 * Get voltage sensor by index
 * @index: Voltage sensor index
 * @return Pointer to hal_hwm_sensor structure, or NULL if index is out of range
 */
extern struct hal_hwm_sensor *hal_hwm_in(u8 index);

/**
 * Read voltage sensor value
 * @sensor: Pointer to hal_hwm_sensor structure
 * @return Voltage in millivolts
 */
extern s32 hal_hwm_in_read(struct hal_hwm_sensor *sensor);

/**
 * Get temperature sensor by index
 * @index: Temperature sensor index
 * @return Pointer to hal_hwm_sensor structure, or NULL if index is out of range
 */
extern struct hal_hwm_sensor *hal_hwm_temp(u8 index);

/**
 * Read temperature sensor value
 * @sensor: Pointer to hal_hwm_sensor structure
 * @return Temperature in millidegrees Celsius
 */
extern s32 hal_hwm_temp_read(struct hal_hwm_sensor *sensor);

/**
 * Get fan speed sensor by index
 * @index: Fan speed sensor index
 * @return Pointer to hal_hwm_sensor structure, or NULL if index is out of range
 */
extern struct hal_hwm_sensor *hal_hwm_fan(u8 index);

/**
 * Get fan speed sensor value
 * @sensor: Pointer to hal_hwm_sensor structure
 * @return Fan speed in RPM
 */
extern s32 hal_hwm_fan_read(struct hal_hwm_sensor *sensor);

/**
 * Get PWM sensor by index
 * @index: PWM sensor index
 * @return Pointer to hal_hwm_sensor structure, or NULL if index is out of range
 */
extern struct hal_hwm_sensor *hal_hwm_pwm(u8 index);

/**
 * Read PWM sensor value
 * @sensor: Pointer to hal_hwm_sensor structure
 * @return PWM duty cycle in percentage
 */
extern s32 hal_hwm_pwm_read(struct hal_hwm_sensor *sensor);

/**
 * Write PWM sensor value
 * @sensor: Pointer to hal_hwm_sensor structure
 * @buf: Buffer containing the value to write
 * @count: Size of the buffer
 * @return Number of bytes written, or negative error code on failure
 */
extern s32 hal_hwm_pwm_write(struct hal_hwm_sensor *sensor, const char *buf,
			     size_t count);

#endif /** __HAL_HWM_H__ */
