
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/ec/ite.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/hwm.h"

#include "log.h"

extern struct ec_device ite_dev;
extern struct bctrl_desc ite_desc;

/** ITE Voltage */
static const u8 hal_hwm_in_map[CONFIG_HWM_VOLTAGE_NUM] = CONFIG_HWM_VOLTAGE_MAP;
struct hal_hwm_sensor ite_hwm_ins[CONFIG_HWM_VOLTAGE_NUM] = {

#if CONFIG_HWM_VOLTAGE_0_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_0_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_0_REG, CONFIG_HWM_VOLTAGE_0_LSB,
			CONFIG_HWM_VOLTAGE_0_R1, CONFIG_HWM_VOLTAGE_0_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_VOLTAGE_1_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_1_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_1_REG, CONFIG_HWM_VOLTAGE_1_LSB,
			CONFIG_HWM_VOLTAGE_1_R1, CONFIG_HWM_VOLTAGE_1_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_VOLTAGE_2_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_2_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_2_REG, CONFIG_HWM_VOLTAGE_2_LSB,
			CONFIG_HWM_VOLTAGE_2_R1, CONFIG_HWM_VOLTAGE_2_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_VOLTAGE_3_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_3_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_3_REG, CONFIG_HWM_VOLTAGE_3_LSB,
			CONFIG_HWM_VOLTAGE_3_R1, CONFIG_HWM_VOLTAGE_3_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_VOLTAGE_4_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_4_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_4_REG, CONFIG_HWM_VOLTAGE_4_LSB,
			CONFIG_HWM_VOLTAGE_4_R1, CONFIG_HWM_VOLTAGE_4_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif
};

/** ITE Temperature */
static const u8 hal_hwm_temp_map[CONFIG_HWM_TEMPERATURE_NUM] =
	CONFIG_HWM_TEMPERATURE_MAP;
static struct hal_hwm_sensor ite_hwm_temps[CONFIG_HWM_TEMPERATURE_NUM] = {
#if CONFIG_HWM_TEMPERATURE_0_ENABLE
	HWM_SENSOR_ATTR(
		CONFIG_HWM_TEMPERATURE_0_LABEL, HWM_SENSOR_TYPE_TEMPERATURE,
		CONFIG_HWM_TEMPERATURE_0_REG, 0, 0, 0, hal_hwm_temp_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_TEMPERATURE_1_ENABLE
	HWM_SENSOR_ATTR(
		CONFIG_HWM_TEMPERATURE_1_LABEL, HWM_SENSOR_TYPE_TEMPERATURE,
		CONFIG_HWM_TEMPERATURE_1_REG, 0, 0, 0, hal_hwm_temp_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_TEMPERATURE_2_ENABLE
	HWM_SENSOR_ATTR(
		CONFIG_HWM_TEMPERATURE_2_LABEL, HWM_SENSOR_TYPE_TEMPERATURE,
		CONFIG_HWM_TEMPERATURE_2_REG, 0, 0, 0, hal_hwm_temp_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif
};

/** ITE Fan */
static const u8 hal_hwm_fan_map[CONFIG_HWM_FAN_NUM] = CONFIG_HWM_FAN_MAP;
struct hal_hwm_sensor ite_hwm_fans[CONFIG_HWM_FAN_NUM] = {
#if CONFIG_HWM_FAN_0_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_FAN_0_LABEL, HWM_SENSOR_TYPE_FAN_SPEED,
			CONFIG_HWM_FAN_0_REG_SPEED, CONFIG_HWM_FAN_0_SPEED_STEP,
			0, 0, hal_hwm_fan_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_FAN_1_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_FAN_1_LABEL, HWM_SENSOR_TYPE_FAN_SPEED,
			CONFIG_HWM_FAN_1_REG_SPEED, CONFIG_HWM_FAN_1_SPEED_STEP,
			0, 0, hal_hwm_fan_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif
};

/** ITE PWM */
static const u8 hal_hwm_pwm_map[CONFIG_HWM_PWM_NUM] = CONFIG_HWM_PWM_MAP;
struct hal_hwm_sensor ite_hwm_pwms[CONFIG_HWM_PWM_NUM] = {
#if CONFIG_HWM_PWM_0_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_PWM_0_LABEL, HWM_SENSOR_TYPE_PWM,
			CONFIG_HWM_PWM_0_REG, 0, 0, 0, hal_hwm_pwm_read,
			hal_hwm_pwm_write),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_PWM_1_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_PWM_1_LABEL, HWM_SENSOR_TYPE_PWM,
			CONFIG_HWM_PWM_1_REG, 0, 0, 0, hal_hwm_pwm_read,
			hal_hwm_pwm_write),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_PWM_2_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_PWM_2_LABEL, HWM_SENSOR_TYPE_PWM,
			CONFIG_HWM_PWM_2_REG, 0, 0, 0, hal_hwm_pwm_read,
			hal_hwm_pwm_write),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_PWM_3_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_PWM_3_LABEL, HWM_SENSOR_TYPE_PWM,
			CONFIG_HWM_PWM_3_REG, 0, 0, 0, hal_hwm_pwm_read,
			hal_hwm_pwm_write),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_PWM_4_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_PWM_4_LABEL, HWM_SENSOR_TYPE_PWM,
			CONFIG_HWM_PWM_4_REG, 0, 0, 0, hal_hwm_pwm_read,
			hal_hwm_pwm_write),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_PWM_5_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_PWM_5_LABEL, HWM_SENSOR_TYPE_PWM,
			CONFIG_HWM_PWM_5_REG, 0, 0, 0, hal_hwm_pwm_read,
			hal_hwm_pwm_write),
#else
	HWM_SENSOR_NONE,
#endif
};

struct hal_hwm_sensor *hwm_sensors[] = {
	ite_hwm_ins, ite_hwm_temps, ite_hwm_fans, ite_hwm_pwms, NULL,
};

static struct hal_hwm_sensor *__get_hal_sensor(u8 index, const u8 *map,
					       struct hal_hwm_sensor *sensors,
					       u8 max)
{
	struct hal_hwm_sensor *sensor = NULL;
	if (index >= max)
		return NULL;

	if (map[index] == 0)
		return NULL;

	sensor = &sensors[index];
	if (sensor->type == HWM_SENSOR_TYPE_NONE)
		return NULL;

	log_debug("HAL HWM get sensor index %d type %d label %s\n", index,
		  sensor->type, sensor->label);

	return sensor;
}

struct hal_hwm_sensor *hal_hwm_in(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_in_map, ite_hwm_ins,
				CONFIG_HWM_VOLTAGE_NUM);
}

s32 hal_hwm_in_read(struct hal_hwm_sensor *sensor)
{
	u8 raw = 0;
	s32 voltage = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_VOLTAGE)
		return -EINVAL;

	raw = ite_dev.ops->read8(sensor->reg);
	voltage = (raw * sensor->lsb);

	if (sensor->resistor_r1 != 0 && sensor->resistor_r2 != 0)
		voltage = voltage *
			  (sensor->resistor_r1 + sensor->resistor_r2) /
			  sensor->resistor_r2;

	return voltage;
}

struct hal_hwm_sensor *hal_hwm_temp(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_temp_map, ite_hwm_temps,
				CONFIG_HWM_TEMPERATURE_NUM);
}

s32 hal_hwm_temp_read(struct hal_hwm_sensor *sensor)
{
	u8 raw = 0;
	s32 temp = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_TEMPERATURE)
		return -EINVAL;

	raw = ite_dev.ops->read8(sensor->reg);
	temp = (raw > 127) ? (raw - 256) * 1000 : raw * 1000;

	log_debug("HAL HWM temperature read[0x%02X]: raw=0x%02X, temp=%d mC\n",
		  sensor->reg, raw, temp);

	return temp;
}

struct hal_hwm_sensor *hal_hwm_fan(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_fan_map, ite_hwm_fans,
				CONFIG_HWM_FAN_NUM);
}

s32 hal_hwm_fan_read(struct hal_hwm_sensor *sensor)
{
	u16 raw = 0;
	s32 speed = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_FAN_SPEED)
		return -EINVAL;

	raw = ite_dev.ops->read16(sensor->reg);

	speed = (s32)(raw);

	log_debug("HAL HWM fan speed read[0x%04X]: raw=0x%04X, speed=%d RPM\n",
		  UINT16_FROM_BYTES(sensor->reg, sensor->reg + 1), raw, speed);

	return speed;
}

struct hal_hwm_sensor *hal_hwm_pwm(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_pwm_map, ite_hwm_pwms,
				CONFIG_HWM_PWM_NUM);
}

s32 hal_hwm_pwm_read(struct hal_hwm_sensor *sensor)
{
	u8 raw = 0;
	s32 duty = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_PWM)
		return -EINVAL;

	raw = ite_dev.ops->read16(sensor->reg);

	/* Convert 0-255 to 0-100% */
	duty = (raw * 100) / 255;

	log_debug("HAL HWM PWM read[0x%04X]: raw=0x%04X, duty=%d %%\n",
		  UINT16_FROM_BYTES(sensor->reg, sensor->reg + 1), raw, duty);

	return duty;
}

s32 hal_hwm_pwm_write(struct hal_hwm_sensor *sensor, const char *buf,
		      size_t count)
{
	u8 value = 0;
	u8 reg_value = 0;
	u32 duty = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_PWM)
		return -EINVAL;

	duty = simple_strtoul(buf, NULL, 10);
	if (duty > 100)
		duty = 100;

	reg_value = (duty * 255) / 100;
	ite_dev.ops->write16(sensor->reg, reg_value);

	log_debug("HAL HWM PWM write[0x%04X]: duty=%d%%, raw=0x%04X\n",
		  UINT16_FROM_BYTES(sensor->reg, sensor->reg + 1), duty,
		  reg_value);

	return count;
}

struct bctrl_desc *hal_hwm_desc(void)
{
	return &ite_desc;
}

struct hal_hwm_sensor **hal_hwm_sensors(void)
{
	return hwm_sensors;
}

s32 hal_hwm_init(void)
{
	return ite_dev.ops->probe();
}

void hal_hwm_exit(void)
{
	// no need for ITE chips
}