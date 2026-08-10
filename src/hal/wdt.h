

#ifndef __HAL_WDT_H__
#define __HAL_WDT_H__
#include <linux/types.h>

/**
 * Get the watchdog descriptor
 * @return Pointer to the watchdog descriptor structure
 */
extern struct bctrl_desc *hal_wdt_desc(void);

/**
 * Initialize the hardware watchdog timer
 * @return 0 on success, negative error code on failure
 */
extern s32 hal_wdt_init(void);

/**
 * Deinitialize the hardware watchdog timer
 */
extern void hal_wdt_exit(void);

/**
 * Enables the watchdog timer
 */
extern void hal_wdt_start(void);

/**
 * Disables the watchdog timer
 */
extern void hal_wdt_stop(void);

/**
 * Write the timeout value to the hardware watchdog timer
 * @time: Timeout value in seconds
 */
extern void hal_wdt_write(u8 time);

/**
 * Read the current timeout value from the hardware watchdog timer
 * @return Current timeout value in seconds
 */
extern u8 hal_wdt_read(void);

#endif /** __HAL_WDT_H__ */