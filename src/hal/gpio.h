
#ifndef __HAL_DIO_H__
#define __HAL_DIO_H__

#include <linux/types.h>

union hal_gpio_port {
	u16 full;

	struct {
		u8 low;
		u8 high;
	} byte;

	struct {
		u8 pin0 : 1;
		u8 pin1 : 1;
		u8 pin2 : 1;
		u8 pin3 : 1;
		u8 pin4 : 1;
		u8 pin5 : 1;
		u8 pin6 : 1;
		u8 pin7 : 1;

		u8 pin8 : 1;
		u8 pin9 : 1;
		u8 pin10 : 1;
		u8 pin11 : 1;
		u8 pin12 : 1;
		u8 pin13 : 1;
		u8 pin14 : 1;
		u8 pin15 : 1;
	} pin;

	struct {
		u8 gpi0 : 1;
		u8 gpo0 : 1;
		u8 gpi1 : 1;
		u8 gpo1 : 1;
		u8 gpi2 : 1;
		u8 gpo2 : 1;
		u8 gpi3 : 1;
		u8 gpo3 : 1;

		u8 gpi4 : 1;
		u8 gpo4 : 1;
		u8 gpi5 : 1;
		u8 gpo5 : 1;
		u8 gpi6 : 1;
		u8 gpo6 : 1;
		u8 gpi7 : 1;
		u8 gpo7 : 1;
	} group;
} __attribute__((packed));

// Forward declaration
struct hal_gpio_device;

struct hal_gpio_device {
	union hal_gpio_port direction; // Direction of each GPIO pin
	union hal_gpio_port value; // Value of each GPIO pin
};

/**
 * Get the digital I/O descriptor
 * @return Pointer to the digital I/O descriptor structure
 */
extern struct bctrl_desc *hal_gpio_desc(void);

/**
 * Initialize the digital I/O pins
 * @return s32 
 */
extern s32 hal_gpio_init(void);

/**
 * Deinitialize the digital I/O pins
 */
extern void hal_gpio_exit(void);

/**
 * Read the value of a GPIO pin
 * @dev: Pointer to the gpio_device structure
 * @pin: Pin number to read
 * @out: Pointer to store the read value (0 = low, 1 = high)
 * @return Postive value on success, negative error code on failure
 */
extern s32 hal_gpio_read_value(struct bctrl_desc *dev, u16 pin, u16 *out);

/**
 * Write a value to a GPIO pin
 * @dev: Pointer to the gpio_device structure
 * @pin: Pin number to write
 * @val: Value to write (0 = low, 1 = high)
 * @return 0 on success, negative error code on failure
 */
extern s32 hal_gpio_write_value(struct bctrl_desc *dev, u16 pin, u16 val);

/**
 * Read the direction of a GPIO pin
 * @dev: Pointer to the gpio_device structure
 * @pin: Pin number to read
 * @out: Pointer to store the read direction (0 = input, 1 = output)
 * @return Postive value on success, negative error code on failure
 */
extern s32 hal_gpio_read_direction(struct bctrl_desc *dev, u16 pin, u16 *out);

/**
 * Write a direction to a GPIO pin
 * @dev: Pointer to the gpio_device structure
 * @pin: Pin number to write
 * @dir: Direction to write (0 = input, 1 = output)
 * @return 0 on success, negative error code on failure
 */
extern s32 hal_gpio_write_direction(struct bctrl_desc *dev, u16 pin, u16 dir);
#endif /** __HAL_DIO_H__ */