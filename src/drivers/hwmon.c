
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include "configs/config.h"

#include "hal/bctrl.h"
#include "hal/hwm.h"
#include "log.h"

struct priv_data {
	struct platform_device *pdev;
	struct device *dev;
	struct bctrl_desc *desc;
	struct hal_hwm_sensor **sensors;
};

struct priv_data __priv;

/**
 * Identify and retrieve the sensor based on type and index
 * It maps the sensor type and index to the corresponding hal_hwm_sensor 
 * See priv_data->sensors (array of hal_hwm_sensor pointers)
 * 
 * @nr Sensor type index
 * @index Sensor index
 */

#define HWM_SENSOR_INDEX_VOLTAGE 0
#define HWM_SENSOR_INDEX_TEMPERATURE 1
#define HWM_SENSOR_INDEX_FAN 2
#define HWM_SENSOR_INDEX_PWM 3

static int __to_sensor(u8 nr, u8 index, struct hal_hwm_sensor **out)
{
	switch (nr) {
	case HWM_SENSOR_INDEX_VOLTAGE:
		*out = hal_hwm_in(index);
		break;

	case HWM_SENSOR_INDEX_TEMPERATURE:
		*out = hal_hwm_temp(index);
		break;

	case HWM_SENSOR_INDEX_FAN:
		*out = hal_hwm_fan(index);
		break;

	case HWM_SENSOR_INDEX_PWM:
		*out = hal_hwm_pwm(index);
		break;

	default:
		log_err("Invalid sensor index: %d\n", index);
		return -ENODEV;
	}

	if (*out == NULL) {
		log_err("Sensor not found for index %d nr %d\n", index, nr);
		return -ENODEV;
	}

	return 0;
}

static ssize_t __hwm_sensor_label_show(struct device *dev,
				       struct device_attribute *devattr,
				       char *buf)
{
	struct sensor_device_attribute_2 *sdattr = NULL;
	struct hal_hwm_sensor *sensor = NULL;

	s32 ret = 0;

	sdattr = to_sensor_dev_attr_2(devattr);
	if (!sdattr) {
		log_err("Invalid device attribute\n");
		return -ENODEV;
	}

	ret = __to_sensor(sdattr->nr, sdattr->index, &sensor);
	if (ret < 0) {
		log_err("Failed to get sensor for index %d nr %d\n",
			sdattr->index, sdattr->nr);
		return ret;
	}

	return sprintf(buf, "%s\n", sensor->label);
}

static ssize_t __hwm_sensor_register_show(struct device *dev,
					  struct device_attribute *devattr,
					  char *buf)
{
	struct sensor_device_attribute_2 *sdattr = NULL;
	struct hal_hwm_sensor *sensor = NULL;

	s32 ret = 0;

	sdattr = to_sensor_dev_attr_2(devattr);
	if (!sdattr) {
		log_err("Invalid device attribute\n");
		return -ENODEV;
	}

	ret = __to_sensor(sdattr->nr, sdattr->index, &sensor);
	if (ret < 0) {
		log_err("Failed to get sensor for index %d nr %d\n",
			sdattr->index, sdattr->nr);
		return ret;
	}

	ret = sensor->read(sensor);
	if (ret < 0) {
		log_err("Failed to read sensor %s\n", sensor->label);
		return ret;
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t __hwm_sensor_register_store(struct device *dev,
					   struct device_attribute *devattr,
					   const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sdattr = NULL;
	struct hal_hwm_sensor *sensor = NULL;

	s32 ret = 0;

	sdattr = to_sensor_dev_attr_2(devattr);
	if (!sdattr) {
		log_err("Invalid device attribute\n");
		return -ENODEV;
	}

	ret = __to_sensor(sdattr->nr, sdattr->index, &sensor);
	if (ret < 0) {
		log_err("Failed to get sensor for index %d nr %d\n",
			sdattr->index, sdattr->nr);
		return ret;
	}

	ret = sensor->write(sensor, buf, count);
	if (ret < 0) {
		log_err("Failed to write sensor %s\n", sensor->label);
		return ret;
	}

	return count;
}

/**
 * Fixed sensor device attribute for voltage
 * The Max number of voltage sensors is 8
 */

// ===== Voltage 0 =====
#if CONFIG_HWM_VOLTAGE_0_ENABLE
static SENSOR_DEVICE_ATTR_2(in0_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 0);
static SENSOR_DEVICE_ATTR_2(in0_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 0);
#endif

// ===== Voltage 1 =====
#if CONFIG_HWM_VOLTAGE_1_ENABLE
static SENSOR_DEVICE_ATTR_2(in1_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 1);
static SENSOR_DEVICE_ATTR_2(in1_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 1);
#endif

// ===== Voltage 2 =====
#if CONFIG_HWM_VOLTAGE_2_ENABLE
static SENSOR_DEVICE_ATTR_2(in2_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 2);
static SENSOR_DEVICE_ATTR_2(in2_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 2);
#endif

// ===== Voltage 3 =====
#if CONFIG_HWM_VOLTAGE_3_ENABLE
static SENSOR_DEVICE_ATTR_2(in3_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 3);
static SENSOR_DEVICE_ATTR_2(in3_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 3);
#endif

// ===== Voltage 4 =====
#if CONFIG_HWM_VOLTAGE_4_ENABLE
static SENSOR_DEVICE_ATTR_2(in4_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 4);
static SENSOR_DEVICE_ATTR_2(in4_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 4);
#endif

// ===== Voltage 5 =====
#if CONFIG_HWM_VOLTAGE_5_ENABLE
static SENSOR_DEVICE_ATTR_2(in5_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 5);
static SENSOR_DEVICE_ATTR_2(in5_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 5);
#endif

// ===== Voltage 6 =====
#if CONFIG_HWM_VOLTAGE_6_ENABLE
static SENSOR_DEVICE_ATTR_2(in6_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 6);
static SENSOR_DEVICE_ATTR_2(in6_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 6);
#endif

// ===== Voltage 7 =====
#if CONFIG_HWM_VOLTAGE_7_ENABLE
static SENSOR_DEVICE_ATTR_2(in7_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 7);
static SENSOR_DEVICE_ATTR_2(in7_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_VOLTAGE, 7);
#endif

static struct attribute *__hwm_in_attrs[] = {
// Voltage input 0
#if CONFIG_HWM_VOLTAGE_0_ENABLE
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
#endif

// Voltage input 1
#if CONFIG_HWM_VOLTAGE_1_ENABLE
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
#endif

// Voltage input 2
#if CONFIG_HWM_VOLTAGE_2_ENABLE
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_label.dev_attr.attr,
#endif

// Voltage input 3
#if CONFIG_HWM_VOLTAGE_3_ENABLE
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_label.dev_attr.attr,
#endif

// Voltage input 4
#if CONFIG_HWM_VOLTAGE_4_ENABLE
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_label.dev_attr.attr,
#endif

// Voltage input 5
#if CONFIG_HWM_VOLTAGE_5_ENABLE
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in5_label.dev_attr.attr,
#endif

// Voltage input 6
#if CONFIG_HWM_VOLTAGE_6_ENABLE
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in6_label.dev_attr.attr,
#endif

// Voltage input 7
#if CONFIG_HWM_VOLTAGE_7_ENABLE
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in7_label.dev_attr.attr,
#endif
	NULL,
};

static struct attribute_group __hwm_in_group = {
	.name = "in",
	.attrs = __hwm_in_attrs,
};

/**
 * Fixed sensor device attribute for temperature
 * The Max number of temperature sensors is 3
 */

// ===== Temperature 0 =====
#if CONFIG_HWM_TEMPERATURE_0_ENABLE
static SENSOR_DEVICE_ATTR_2(temp0_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_TEMPERATURE, 0);
static SENSOR_DEVICE_ATTR_2(temp0_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_TEMPERATURE, 0);
#endif

// ===== Temperature 1 =====
#if CONFIG_HWM_TEMPERATURE_1_ENABLE
static SENSOR_DEVICE_ATTR_2(temp1_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_TEMPERATURE, 1);
static SENSOR_DEVICE_ATTR_2(temp1_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_TEMPERATURE, 1);
#endif

// ===== Temperature 2 =====
#if CONFIG_HWM_TEMPERATURE_2_ENABLE
static SENSOR_DEVICE_ATTR_2(temp2_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_TEMPERATURE, 2);
static SENSOR_DEVICE_ATTR_2(temp2_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_TEMPERATURE, 2);
#endif

static struct attribute *__hwm_temp_attrs[] = {
// Temperature input 0
#if CONFIG_HWM_TEMPERATURE_0_ENABLE
	&sensor_dev_attr_temp0_input.dev_attr.attr,
	&sensor_dev_attr_temp0_label.dev_attr.attr,
#endif

// Temperature input 1
#if CONFIG_HWM_TEMPERATURE_1_ENABLE
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
#endif

// Temperature input 2
#if CONFIG_HWM_TEMPERATURE_2_ENABLE
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_label.dev_attr.attr,
#endif
	NULL,
};

static struct attribute_group __hwm_temp_group = {
	.name = "temp",
	.attrs = __hwm_temp_attrs,
};

/**
 * Fixed sensor device attribute for fan speed
 */

// ===== Fan 0 =====
#if CONFIG_HWM_FAN_0_ENABLE
static SENSOR_DEVICE_ATTR_2(fan0_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_FAN, 0);
static SENSOR_DEVICE_ATTR_2(fan0_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_FAN, 0);
#endif

// ===== Fan 1 =====
#if CONFIG_HWM_FAN_1_ENABLE
static SENSOR_DEVICE_ATTR_2(fan1_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_FAN, 1);
static SENSOR_DEVICE_ATTR_2(fan1_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_FAN, 1);
#endif

// ===== Fan 2 =====
#if CONFIG_HWM_FAN_2_ENABLE
static SENSOR_DEVICE_ATTR_2(fan2_input, 0444, __hwm_sensor_register_show, NULL,
			    HWM_SENSOR_INDEX_FAN, 2);
static SENSOR_DEVICE_ATTR_2(fan2_label, 0444, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_FAN, 2);
#endif

static struct attribute *__hwm_fan_attrs[] = {
// Fan input 0
#if CONFIG_HWM_FAN_0_ENABLE
	&sensor_dev_attr_fan0_input.dev_attr.attr,
	&sensor_dev_attr_fan0_label.dev_attr.attr,
#endif

// Fan input 1
#if CONFIG_HWM_FAN_1_ENABLE
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_label.dev_attr.attr,
#endif

// Fan input 2
#if CONFIG_HWM_FAN_2_ENABLE
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_label.dev_attr.attr,
#endif

	NULL,
};

static struct attribute_group __hwm_fan_group = {
	.name = "fan",
	.attrs = __hwm_fan_attrs,
};

/**
 * Fixed sensor device attribute for PWM
 */

// ===== PWM 0 =====
#if CONFIG_HWM_PWM_0_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm0, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    0);
static SENSOR_DEVICE_ATTR_2(pwm0_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 0);
#endif

// ===== PWM 1 =====
#if CONFIG_HWM_PWM_1_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm1, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    1);
static SENSOR_DEVICE_ATTR_2(pwm1_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 1);
#endif

// ===== PWM 2 =====
#if CONFIG_HWM_PWM_2_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm2, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    2);
static SENSOR_DEVICE_ATTR_2(pwm2_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 2);
#endif

// ===== PWM 3 =====
#if CONFIG_HWM_PWM_3_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm3, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    3);
static SENSOR_DEVICE_ATTR_2(pwm3_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 3);
#endif

// ===== PWM 4 =====
#if CONFIG_HWM_PWM_4_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm4, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    4);
static SENSOR_DEVICE_ATTR_2(pwm4_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 4);
#endif

// ===== PWM 5 =====
#if CONFIG_HWM_PWM_5_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm5, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    5);
static SENSOR_DEVICE_ATTR_2(pwm5_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 5);
#endif

// ===== PWM 6 =====
#if CONFIG_HWM_PWM_6_ENABLE
static SENSOR_DEVICE_ATTR_2(pwm6, 0644, __hwm_sensor_register_show,
			    __hwm_sensor_register_store, HWM_SENSOR_INDEX_PWM,
			    6);
static SENSOR_DEVICE_ATTR_2(pwm6_label, 0644, __hwm_sensor_label_show, NULL,
			    HWM_SENSOR_INDEX_PWM, 6);
#endif

static struct attribute *__hwm_pwm_attrs[] = {
// PWM 0
#if CONFIG_HWM_PWM_0_ENABLE
	&sensor_dev_attr_pwm0.dev_attr.attr,
	&sensor_dev_attr_pwm0_label.dev_attr.attr,
#endif

// PWM 1
#if CONFIG_HWM_PWM_1_ENABLE
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_label.dev_attr.attr,
#endif

// PWM 2
#if CONFIG_HWM_PWM_2_ENABLE
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm2_label.dev_attr.attr,
#endif

// PWM 3
#if CONFIG_HWM_PWM_3_ENABLE
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm3_label.dev_attr.attr,
#endif

// PWM 4
#if CONFIG_HWM_PWM_4_ENABLE
	&sensor_dev_attr_pwm4.dev_attr.attr,
	&sensor_dev_attr_pwm4_label.dev_attr.attr,
#endif

// PWM 5
#if CONFIG_HWM_PWM_5_ENABLE
	&sensor_dev_attr_pwm5.dev_attr.attr,
	&sensor_dev_attr_pwm5_label.dev_attr.attr,
#endif

// PWM 6
#if CONFIG_HWM_PWM_6_ENABLE
	&sensor_dev_attr_pwm6.dev_attr.attr,
	&sensor_dev_attr_pwm6_label.dev_attr.attr,
#endif

	NULL,
};

static struct attribute_group __hwm_pwm_group = {
	.name = "pwm",
	.attrs = __hwm_pwm_attrs,
};

static const struct attribute_group *__hwm_groups[] = {
	&__hwm_in_group,
	&__hwm_temp_group,
	&__hwm_fan_group,
	&__hwm_pwm_group,
	NULL,
};

/*
 * HWMON_CHANNEL_INFO() is missing from kernel 4.15's <linux/hwmon.h> (present
 * from 5.4.302 onward, the earliest tree we have that side of the gap; the
 * exact version it arrives in is not measured -- see avalue-driver-4.0#25).
 * Guard on the macro itself rather than a guessed LINUX_VERSION_CODE
 * threshold, and fall back to the plain sysfs attribute groups below
 * (__hwm_groups), which already carry every sensor file and need no chip_info.
 */
#ifdef HWMON_CHANNEL_INFO
static umode_t __hwm_is_visible(const void *data, enum hwmon_sensor_types type,
				u32 attr, int channel)
{
	switch (type) {
	case hwmon_in:
		if (channel < CONFIG_HWM_VOLTAGE_NUM) {
			/* Check if voltage channel is enabled */
			switch (channel) {
#if CONFIG_HWM_VOLTAGE_0_ENABLE
			case 0:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_1_ENABLE
			case 1:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_2_ENABLE
			case 2:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_3_ENABLE
			case 3:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_4_ENABLE
			case 4:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_5_ENABLE
			case 5:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_6_ENABLE
			case 6:
				return 0444;
#endif
#if CONFIG_HWM_VOLTAGE_7_ENABLE
			case 7:
				return 0444;
#endif
			}
		}
		break;

	case hwmon_temp:
		if (channel < CONFIG_HWM_TEMPERATURE_NUM) {
			switch (channel) {
#if CONFIG_HWM_TEMPERATURE_0_ENABLE
			case 0:
				return 0444;
#endif
#if CONFIG_HWM_TEMPERATURE_1_ENABLE
			case 1:
				return 0444;
#endif
#if CONFIG_HWM_TEMPERATURE_2_ENABLE
			case 2:
				return 0444;
#endif
			}
		}
		break;

	case hwmon_fan:
		if (channel < CONFIG_HWM_FAN_NUM) {
			switch (channel) {
#if CONFIG_HWM_FAN_0_ENABLE
			case 0:
				return 0444;
#endif
#if CONFIG_HWM_FAN_1_ENABLE
			case 1:
				return 0444;
#endif
#if CONFIG_HWM_FAN_2_ENABLE
			case 2:
				return 0444;
#endif
			}
		}
		break;

	case hwmon_pwm:
		if (channel < CONFIG_HWM_PWM_NUM) {
			switch (channel) {
#if CONFIG_HWM_PWM_0_ENABLE
			case 0:
				return 0644;
#endif
#if CONFIG_HWM_PWM_1_ENABLE
			case 1:
				return 0644;
#endif
#if CONFIG_HWM_PWM_2_ENABLE
			case 2:
				return 0644;
#endif
#if CONFIG_HWM_PWM_3_ENABLE
			case 3:
				return 0644;
#endif
#if CONFIG_HWM_PWM_4_ENABLE
			case 4:
				return 0644;
#endif
#if CONFIG_HWM_PWM_5_ENABLE
			case 5:
				return 0644;
#endif
#if CONFIG_HWM_PWM_6_ENABLE
			case 6:
				return 0644;
#endif
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

static int __hwm_read(struct device *dev, enum hwmon_sensor_types type,
		      u32 attr, int channel, long *val)
{
	struct hal_hwm_sensor *sensor = NULL;
	s32 ret = 0;
	u8 nr = 0;

	switch (type) {
	case hwmon_in:
		nr = HWM_SENSOR_INDEX_VOLTAGE;
		break;
	case hwmon_temp:
		nr = HWM_SENSOR_INDEX_TEMPERATURE;
		break;
	case hwmon_fan:
		nr = HWM_SENSOR_INDEX_FAN;
		break;
	case hwmon_pwm:
		nr = HWM_SENSOR_INDEX_PWM;
		break;
	default:
		return -EOPNOTSUPP;
	}

	ret = __to_sensor(nr, channel, &sensor);
	if (ret < 0)
		return ret;

	ret = sensor->read(sensor);
	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}

static int __hwm_read_string(struct device *dev, enum hwmon_sensor_types type,
			     u32 attr, int channel, const char **str)
{
	struct hal_hwm_sensor *sensor = NULL;
	s32 ret = 0;
	u8 nr = 0;

	switch (type) {
	case hwmon_in:
		nr = HWM_SENSOR_INDEX_VOLTAGE;
		break;
	case hwmon_temp:
		nr = HWM_SENSOR_INDEX_TEMPERATURE;
		break;
	case hwmon_fan:
		nr = HWM_SENSOR_INDEX_FAN;
		break;
	case hwmon_pwm:
		nr = HWM_SENSOR_INDEX_PWM;
		break;
	default:
		return -EOPNOTSUPP;
	}

	ret = __to_sensor(nr, channel, &sensor);
	if (ret < 0)
		return ret;

	*str = sensor->label;
	return 0;
}

static int __hwm_write(struct device *dev, enum hwmon_sensor_types type,
		       u32 attr, int channel, long val)
{
	struct hal_hwm_sensor *sensor = NULL;
	s32 ret = 0;
	char buf[16];

	if (type != hwmon_pwm)
		return -EOPNOTSUPP;

	ret = __to_sensor(HWM_SENSOR_INDEX_PWM, channel, &sensor);
	if (ret < 0)
		return ret;

	snprintf(buf, sizeof(buf), "%ld", val);
	ret = sensor->write(sensor, buf, strlen(buf));
	if (ret < 0)
		return ret;

	return 0;
}

static const struct hwmon_ops __hwm_ops = {
	.is_visible = __hwm_is_visible,
	.read = __hwm_read,
	.read_string = __hwm_read_string,
	.write = __hwm_write,
};

static const struct hwmon_channel_info *__hwm_channel_info[] = {
	HWMON_CHANNEL_INFO(
		in, HWMON_I_INPUT | HWMON_I_LABEL,
		HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL,
		HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL,
		HWMON_I_INPUT | HWMON_I_LABEL, HWMON_I_INPUT | HWMON_I_LABEL,
		HWMON_I_INPUT | HWMON_I_LABEL),

	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL),

	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL),

	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT, HWMON_PWM_INPUT,
			   HWMON_PWM_INPUT, HWMON_PWM_INPUT, HWMON_PWM_INPUT,
			   HWMON_PWM_INPUT, HWMON_PWM_INPUT),
	NULL
};

static const struct hwmon_chip_info __hwm_info = {
	.ops = &__hwm_ops,
	.info = __hwm_channel_info,
};
#endif /* HWMON_CHANNEL_INFO */

static int __init hwm_init(void)
{
	int ret = 0;

	log_info("Initializing Hardware Monitor driver\n");

	ret = hal_hwm_init();
	if (ret < 0) {
		log_err("Failed to initialize HAL HWM: %d\n", ret);
		return ret;
	}

	__priv.pdev = platform_device_register_simple("hwmon", -1, NULL, 0);
	if (IS_ERR(__priv.pdev)) {
		ret = PTR_ERR(__priv.pdev);
		log_err("Failed to register platform device: %d\n", ret);
		return ret;
	}

	__priv.desc = hal_hwm_desc();
	if (IS_ERR(__priv.desc)) {
		ret = PTR_ERR(__priv.desc);
		log_err("Failed to get HAL HWM descriptor\n");
		return ret;
	}

	__priv.sensors = hal_hwm_sensors();
	if (IS_ERR(__priv.sensors)) {
		ret = PTR_ERR(__priv.sensors);
		log_err("Failed to get HAL HWM sensors\n");
		return ret;
	}

#ifdef HWMON_CHANNEL_INFO
	__priv.dev = hwmon_device_register_with_info(
		&__priv.pdev->dev, CONFIG_HWM_CHIPSET, &__priv, &__hwm_info, __hwm_groups);
#else
	__priv.dev = hwmon_device_register_with_groups(
		&__priv.pdev->dev, CONFIG_HWM_CHIPSET, &__priv, __hwm_groups);
#endif

	if (IS_ERR(__priv.dev)) {
		ret = PTR_ERR(__priv.dev);
		log_err("Failed to register hwmon device: %d\n", ret);
		return ret;
	}

	log_info("Hardware Monitor driver initialized successfully\n");
	return 0;
}

static void __exit hwm_exit(void)
{
	log_info("Exiting Hardware Monitor driver\n");

	hwmon_device_unregister(__priv.dev);

	platform_device_unregister(__priv.pdev);

	hal_hwm_exit();
}

module_init(hwm_init);
module_exit(hwm_exit);

MODULE_AUTHOR("Avalue Technology Inc.");
MODULE_AUTHOR("Arthur Huang <arthur_huang@avalue.com>");
MODULE_DESCRIPTION("Hardware Monitor driver for Avalue boards");
MODULE_LICENSE("GPL");
MODULE_VERSION(CONFIG_DRIVER_VERSION);