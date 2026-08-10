#ifndef _LOG_H_
#define _LOG_H_

#include <linux/printk.h>

#include "configs/config.h"

#ifdef DEBUG
#define LOG_PREFIX(_level) "[" CONFIG_BOARD_NAME " " _level "] %s:%d: "
#define log_info(fmt, ...) pr_info(LOG_PREFIX("INFO") fmt, __func__, __LINE__, ##__VA_ARGS__)
#define log_err(fmt, ...) pr_err(LOG_PREFIX("ERROR") fmt, __func__, __LINE__, ##__VA_ARGS__)
#define log_warn(fmt, ...) pr_warn(LOG_PREFIX("WARN") fmt, __func__, __LINE__, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug(LOG_PREFIX("DEBUG") fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_PREFIX(_level) "[" CONFIG_BOARD_NAME " " _level " ] "
#define log_info(fmt, ...) pr_info(LOG_PREFIX("INFO") fmt, ##__VA_ARGS__)
#define log_err(fmt, ...) pr_err(LOG_PREFIX("ERROR") fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) pr_warn(LOG_PREFIX("WARN") fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug(LOG_PREFIX("DEBUG") fmt, ##__VA_ARGS__)
#endif

#endif // _LOG_H_
