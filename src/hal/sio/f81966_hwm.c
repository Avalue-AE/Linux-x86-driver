
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/sio/f81966.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/hwm.h"

#include "log.h"

extern struct sio_device f81966_dev;
extern struct bctrl_desc f81966_desc;

/** F81966 Hwmon descriptor */
struct bctrl_desc *hal_hwm_desc(void);

/** F81966 Voltage */
static const u8 hal_hwm_in_map[CONFIG_HWM_VOLTAGE_NUM] = CONFIG_HWM_VOLTAGE_MAP;
static struct hal_hwm_sensor f81966_hwm_ins[CONFIG_HWM_VOLTAGE_NUM] = {

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

#if CONFIG_HWM_VOLTAGE_5_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_5_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_5_REG, CONFIG_HWM_VOLTAGE_5_LSB,
			CONFIG_HWM_VOLTAGE_5_R1, CONFIG_HWM_VOLTAGE_5_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_VOLTAGE_6_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_6_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_6_REG, CONFIG_HWM_VOLTAGE_6_LSB,
			CONFIG_HWM_VOLTAGE_6_R1, CONFIG_HWM_VOLTAGE_6_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

#if CONFIG_HWM_VOLTAGE_7_ENABLE
	HWM_SENSOR_ATTR(CONFIG_HWM_VOLTAGE_7_LABEL, HWM_SENSOR_TYPE_VOLTAGE,
			CONFIG_HWM_VOLTAGE_7_REG, CONFIG_HWM_VOLTAGE_7_LSB,
			CONFIG_HWM_VOLTAGE_7_R1, CONFIG_HWM_VOLTAGE_7_R2,
			hal_hwm_in_read, NULL),
#else
	HWM_SENSOR_NONE,
#endif

};

/** F81966 Temperature */
static const u8 hal_hwm_temp_map[CONFIG_HWM_TEMPERATURE_NUM] =
	CONFIG_HWM_TEMPERATURE_MAP;
static struct hal_hwm_sensor f81966_hwm_temps[CONFIG_HWM_TEMPERATURE_NUM] = {
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

/** F81966 Fan */
static const u8 hal_hwm_fan_map[CONFIG_HWM_FAN_NUM] = CONFIG_HWM_FAN_MAP;
struct hal_hwm_sensor f81966_hwm_fans[CONFIG_HWM_FAN_NUM] = {
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

/** F81966 PWM */
static const u8 hal_hwm_pwm_map[CONFIG_HWM_PWM_NUM] = CONFIG_HWM_PWM_MAP;
struct hal_hwm_sensor f81966_hwm_pwms[CONFIG_HWM_PWM_NUM] = {
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
};

struct hal_hwm_sensor *hwm_sensors[] = {
	f81966_hwm_ins, f81966_hwm_temps, f81966_hwm_fans, f81966_hwm_pwms,
	NULL,
};

static struct hal_hwm_sensor *
__get_hal_sensor(u8 index, const u8 *map, struct hal_hwm_sensor *sensors, u8 max)
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
	return __get_hal_sensor(index, hal_hwm_in_map, f81966_hwm_ins,
				CONFIG_HWM_VOLTAGE_NUM);
}

s32 hal_hwm_in_read(struct hal_hwm_sensor *sensor)
{
	u8 raw = 0;
	s32 voltage = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_VOLTAGE)
		return -EINVAL;

	raw = f81966_dev.ops->read8(sensor->reg);
	voltage = raw * sensor->lsb;

	// If voltage divider is used
	if (sensor->resistor_r1 != 0 && sensor->resistor_r2 != 0) {
		voltage =
			voltage * (sensor->resistor_r1 + sensor->resistor_r2);
		voltage = voltage / sensor->resistor_r2;
	}

	log_debug("HAL HWM voltage read[0x%02X]: raw=0x%02X, voltage=%d mV\n",
		  sensor->reg, raw, voltage);

	return voltage;
}

struct hal_hwm_sensor *hal_hwm_temp(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_temp_map, f81966_hwm_temps,
				CONFIG_HWM_TEMPERATURE_NUM);
}

s32 hal_hwm_temp_read(struct hal_hwm_sensor *sensor)
{
	u8 raw = 0;
	s32 temp = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_TEMPERATURE)
		return -EINVAL;

	raw = f81966_dev.ops->read8(sensor->reg);
	temp = raw * 1000;

	log_debug("HAL HWM temperature read[0x%02X]: raw=0x%02X, temp=%d mC\n",
		  sensor->reg, raw, temp);

	return temp;
}

struct hal_hwm_sensor *hal_hwm_fan(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_fan_map, f81966_hwm_fans,
				CONFIG_HWM_FAN_NUM);
}

s32 hal_hwm_fan_read(struct hal_hwm_sensor *sensor)
{
	u16 raw = 0;
	s32 speed = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_FAN_SPEED)
		return -EINVAL;

	raw = f81966_dev.ops->read16(sensor->reg);

	speed = sensor->lsb / raw;

	log_debug("HAL HWM fan speed read[0x%04X]: raw=0x%04X, speed=%d RPM\n",
		  UINT16_FROM_BYTES(sensor->reg, sensor->reg + 1), raw, speed);

	return speed;
}

struct hal_hwm_sensor *hal_hwm_pwm(u8 index)
{
	return __get_hal_sensor(index, hal_hwm_pwm_map, f81966_hwm_pwms,
				CONFIG_HWM_PWM_NUM);
}

s32 hal_hwm_pwm_read(struct hal_hwm_sensor *sensor)
{
	u8 raw = 0;
	s32 duty = 0;

	if (sensor == NULL || sensor->type != HWM_SENSOR_TYPE_PWM)
		return -EINVAL;

	raw = f81966_dev.ops->read16(sensor->reg);

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
	f81966_dev.ops->write16(sensor->reg, reg_value);

	log_debug("HAL HWM PWM write[0x%04X]: duty=%d%%, raw=0x%04X\n",
		  UINT16_FROM_BYTES(sensor->reg, sensor->reg + 1), duty,
		  reg_value);

	return count;
}

struct bctrl_desc *hal_hwm_desc(void)
{
	return &f81966_desc;
}

struct hal_hwm_sensor **hal_hwm_sensors(void)
{
	return hwm_sensors;
}

s32 hal_hwm_init(void)
{
	s32 ret = 0;
	u16 addr = 0;
	u8 val = 0, addrl = 0, addrh = 0;
	u8 i = 0;

	ret = f81966_dev.ops->probe();
	if (ret < 0) {
		log_err("F81966 SIO probe failed\n");
		return ret;
	}

	f81966_dev.ops->select(F81966_LD4_HWM);

	addrl = f81966_dev.ops->read8(F81966_HWM_REG_BASE_ADDR_L);
	if (addrl == 0xFF || addrl == 0x00) {
		log_err("F81966 HWM base address low register invalid: 0x%02X\n",
			addrl);
		return -ENODEV;
	}

	addrh = f81966_dev.ops->read8(F81966_HWM_REG_BASE_ADDR_H);
	if (addrh == 0xFF || addrh == 0x00) {
		log_err("F81966 HWM base address high register invalid: 0x%02X\n",
			addrh);
		return -ENODEV;
	}

	log_debug("F81966 HWM base address registers: 0x%02X 0x%02X\n", addrh,
		  addrl);

	addr = (UINT16_FROM_BYTES(addrh, addrl) & 0xFFF0);

	// Update SIO device config ports for HWM
	f81966_dev.cfg->index_port = addr + 5;
	f81966_dev.cfg->data_port = addr + 6;

	log_debug("F81966 HWM initialized at I/O port 0x%04X\n", addr);
	log_debug("F81966 HWM SIO index port: 0x%04X, data port: 0x%04X\n",
		  f81966_dev.cfg->index_port, f81966_dev.cfg->data_port);

	// debug register configure
	for (i = 0; i < CONFIG_HWM_VOLTAGE_NUM; i++) {
		log_debug("HAL HWM voltage sensor %d: label=%s, reg=0x%02X\n",
			  i, f81966_hwm_ins[i].label, f81966_hwm_ins[i].reg);
		hal_hwm_in_read(&f81966_hwm_ins[i]);
	}

	for (i = 0; i < CONFIG_HWM_TEMPERATURE_NUM; i++) {
		log_debug(
			"HAL HWM temperature sensor %d: label=%s, reg=0x%02X\n",
			i, f81966_hwm_temps[i].label, f81966_hwm_temps[i].reg);
		hal_hwm_temp_read(&f81966_hwm_temps[i]);
	}

	for (i = 0; i < CONFIG_HWM_FAN_NUM; i++) {
		log_debug("HAL HWM fan sensor %d: label=%s, reg=0x%02X\n", i,
			  f81966_hwm_fans[i].label, f81966_hwm_fans[i].reg);
		hal_hwm_fan_read(&f81966_hwm_fans[i]);
	}

	for (i = 0; i < CONFIG_HWM_PWM_NUM; i++) {
		log_debug("HAL HWM PWM sensor %d: label=%s, reg=0x%02X\n", i,
			  f81966_hwm_pwms[i].label, f81966_hwm_pwms[i].reg);
		hal_hwm_pwm_read(&f81966_hwm_pwms[i]);
	}

	return 0;
}

void hal_hwm_exit(void) {
	// no need for f81966
}