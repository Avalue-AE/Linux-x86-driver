
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/types.h>

#include "configs/config.h"

#include "hal/ec/ite.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"
#include "hal/gpio.h"

#include "log.h"

/*
 * ITE it85x8 EC GPIO / DIO.
 *
 * These lines hang off the on-board EC and are reached through the shared ite
 * EC ops (which already route to the EC BRAM window) -- no SMBus host
 * controller and therefore no MAKE_GPIO_PROTOCOL object.
 *
 * The EC read path differs between boards (their EC firmware BRAM maps differ),
 * so nothing is hard-coded: the register roles and access model come from the
 * board .conf via CONFIG_GPIO_* (see ite.h). Two models are supported:
 *
 *   CONFIG_GPIO_DIR_FIXED=0  configurable direction, single value register
 *                            plus a direction register.
 *   CONFIG_GPIO_DIR_FIXED=1  fixed direction, separate input/output banks;
 *                            CONFIG_GPIO_DIR_MASK says which logical pins are
 *                            outputs (e.g. ESM-KX60G: 0x5A GPI / 0x5B GPO).
 *
 * Per-line bit positions come from CONFIG_GPIO_PIN_MAP either way.
 */

extern struct ec_device ite_dev;
extern struct bctrl_desc ite_desc;

static const u16 hal_gpio_map[CONFIG_GPIO_PIN_NUM] = CONFIG_GPIO_PIN_MAP;

#if CONFIG_GPIO_DIR_FIXED
static inline bool __pin_is_output(u16 pin)
{
	return (CONFIG_GPIO_DIR_MASK & BIT(pin)) != 0;
}
#endif

/*
 * Some EC firmwares are slow to update their own read-back of
 * CONFIG_GPIO_REG_OUTPUT after a write (observed lag up to ~110ms on
 * ESM-KX60G), and this EC's BRAM map has no busy/ready bit to poll instead.
 * hal_gpio_lock serializes read_value()/write_value() so a plain read can
 * never land mid-write_value()'s read-modify-write; last_written(_valid) is
 * the byte write_value() last sent, used purely as a comparison oracle to
 * tell a caught-up read-back from a stale one -- never as a substitute for
 * an actual register read.
 */
static DEFINE_MUTEX(hal_gpio_lock);
static u8 last_written;
static bool last_written_valid;

#define HAL_GPIO_WRITE_POLL_TIMEOUT_MS 150
#define HAL_GPIO_WRITE_POLL_SLEEP_MIN_US 5000
#define HAL_GPIO_WRITE_POLL_SLEEP_MAX_US 8000

struct bctrl_desc *hal_gpio_desc(void)
{
	return &ite_desc;
}

s32 hal_gpio_init(void)
{
	s32 ret = ite_dev.ops->probe();

	if (ret < 0) {
		log_err("ITE EC probe failed for GPIO\n");
		return ret;
	}

	log_debug("ITE EC GPIO initialized\n");
	return 0;
}

void hal_gpio_exit(void)
{
	// No specific deinitialization needed for ITE EC GPIO
}

s32 hal_gpio_read_value(struct bctrl_desc *dev, u16 pin, u16 *out)
{
	u16 mask;
	u8 buf;

	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

	mask = BIT(hal_gpio_map[pin]);

	mutex_lock(&hal_gpio_lock);

#if CONFIG_GPIO_DIR_FIXED
	// output pins read back from the output bank, inputs from the input bank
	buf = ite_dev.ops->read8(__pin_is_output(pin) ? CONFIG_GPIO_REG_OUTPUT :
							  CONFIG_GPIO_REG_INPUT);
#else
	// actual pin level from the input/feedback register
	buf = ite_dev.ops->read8(CONFIG_GPIO_REG_INPUT);
#endif

	mutex_unlock(&hal_gpio_lock);

	*out = (buf & mask) > 0 ? 1 : 0;
	log_debug("Read GPIO value at pin %u: %u\n", pin, *out);
	return 0;
}

s32 hal_gpio_write_value(struct bctrl_desc *dev, u16 pin, u16 val)
{
	u16 mask;
	u8 buf;

	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

#if CONFIG_GPIO_DIR_FIXED
	if (!__pin_is_output(pin)) {
		log_err("GPIO pin %u is a fixed input, cannot drive it\n", pin);
		return -EPERM;
	}
#endif

	mask = BIT(hal_gpio_map[pin]);

	mutex_lock(&hal_gpio_lock);

	if (last_written_valid) {
		unsigned long deadline = jiffies +
			msecs_to_jiffies(HAL_GPIO_WRITE_POLL_TIMEOUT_MS);

		// the EC firmware can lag behind a write when its output
		// bank is read back too soon; wait for a fresh read to catch
		// up with what we last wrote before trusting it as the base
		// for this read-modify-write
		buf = ite_dev.ops->read8(CONFIG_GPIO_REG_OUTPUT);
		while (buf != last_written && time_before(jiffies, deadline)) {
			usleep_range(HAL_GPIO_WRITE_POLL_SLEEP_MIN_US,
				HAL_GPIO_WRITE_POLL_SLEEP_MAX_US);
			buf = ite_dev.ops->read8(CONFIG_GPIO_REG_OUTPUT);
		}

		if (buf != last_written)
			log_warn("GPIO output register 0x%x still stale after %ums, "
				 "proceeding with 0x%02x (expected 0x%02x)\n",
				 CONFIG_GPIO_REG_OUTPUT, HAL_GPIO_WRITE_POLL_TIMEOUT_MS,
				 buf, last_written);
	} else {
		// no prior write to compare against yet (first write since
		// load); nothing to wait for
		buf = ite_dev.ops->read8(CONFIG_GPIO_REG_OUTPUT);
	}

	// read-modify-write the output register
	buf = val > 0 ? (buf | mask) : (buf & ~mask);
	ite_dev.ops->write8(CONFIG_GPIO_REG_OUTPUT, buf);

	last_written = buf;
	last_written_valid = true;

	mutex_unlock(&hal_gpio_lock);

	log_debug("Wrote GPIO value at pin %u: %u\n", pin, val);
	return 0;
}

s32 hal_gpio_read_direction(struct bctrl_desc *dev, u16 pin, u16 *out)
{
	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

#if CONFIG_GPIO_DIR_FIXED
	// direction is hardwired; 1 = input, 0 = output
	*out = __pin_is_output(pin) ? 0 : 1;
#else
	// it85x8 DIR register: 1 = input (GPI), 0 = output (GPO)
	*out = (ite_dev.ops->read8(CONFIG_GPIO_REG_DIRECTION) &
		BIT(hal_gpio_map[pin])) > 0 ?
		       1 :
		       0;
#endif

	log_debug("Read GPIO direction at pin %u: %s\n", pin,
		  (*out > 0) ? "input" : "output");
	return 0;
}

s32 hal_gpio_write_direction(struct bctrl_desc *dev, u16 pin, u16 dir)
{
	u16 mask;
	u8 buf;

	if (pin >= CONFIG_GPIO_PIN_NUM) {
		log_err("Invalid GPIO pin number: %u\n", pin);
		return -EINVAL;
	}

#if CONFIG_GPIO_DIR_FIXED
	// hardwired direction: accept a request that matches, reject a change
	if ((dir > 0 ? 1 : 0) != (__pin_is_output(pin) ? 0 : 1)) {
		log_err("GPIO pin %u has a fixed direction, cannot change it\n",
			pin);
		return -EOPNOTSUPP;
	}
	return 0;
#else
	mask = BIT(hal_gpio_map[pin]);
	buf = ite_dev.ops->read8(CONFIG_GPIO_REG_DIRECTION);
	buf = dir > 0 ? (buf | mask) : (buf & ~mask);
	ite_dev.ops->write8(CONFIG_GPIO_REG_DIRECTION, buf);

	log_debug("Wrote GPIO direction at pin %u: %s\n", pin,
		  (dir > 0) ? "input" : "output");
	return 0;
#endif
}
