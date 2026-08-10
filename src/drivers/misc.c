
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

#include "configs/config.h"

#include "hal/bctrl.h"
#include "hal/misc.h"
#include "log.h"

static long __misc_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static int __misc_open(struct inode *inode, struct file *file);
static int __misc_release(struct inode *inode, struct file *file);
static ssize_t __misc_show(struct device *dev, struct device_attribute *attr,
			   char *buf);
static ssize_t __misc_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count);

extern struct hal_misc_config hal_misc_configs[];

struct miscattr {
	struct device_attribute attr;
	struct hal_misc_config *cfg;
};

#define MISC_DEV_ATTR(_idx) \
	struct miscattr misc_attr_##_idx = {                              \
        .attr = {                                                     \
            .attr = {                                                 \
                .name = CONFIG_MISC_##_idx##_LABEL, \
                .mode = 0644,                                         \
            },                                                        \
            .show = __misc_show,                                      \
            .store = __misc_store,                                    \
        },                                                            \
        .cfg = &hal_misc_configs[_idx],                               \
    }

#if CONFIG_MISC_0_ENABLE
MISC_DEV_ATTR(0);
#endif

#if CONFIG_MISC_1_ENABLE
MISC_DEV_ATTR(1);
#endif

#if CONFIG_MISC_2_ENABLE
MISC_DEV_ATTR(2);
#endif

#if CONFIG_MISC_3_ENABLE
MISC_DEV_ATTR(3);
#endif

#if CONFIG_MISC_4_ENABLE
MISC_DEV_ATTR(4);
#endif

#if CONFIG_MISC_5_ENABLE
MISC_DEV_ATTR(5);
#endif

#if CONFIG_MISC_6_ENABLE
MISC_DEV_ATTR(6);
#endif

#if CONFIG_MISC_7_ENABLE
MISC_DEV_ATTR(7);
#endif

#if CONFIG_MISC_8_ENABLE
MISC_DEV_ATTR(8);
#endif

#if CONFIG_MISC_9_ENABLE
MISC_DEV_ATTR(9);
#endif

static long __misc_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 udata = 0;
	u32 kdata = 0;

	void __user *argp = (void __user *)arg;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = copy_from_user(&udata, argp, sizeof(udata));
		if (ret != 0) {
			return -EFAULT;
		}
	}

	ret = hal_misc_ioctl(cmd, &udata, &kdata);
	if (ret != 0) {
		return ret;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		ret = copy_to_user(argp, &kdata, sizeof(kdata));
		if (ret != 0) {
			return -EFAULT;
		}
	}

	return 0;
}

static ssize_t __misc_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct miscattr *mattr = container_of(attr, struct miscattr, attr);
	struct hal_misc_config *cfg = mattr->cfg;
	s32 ret = 0;
	u32 data = 0;

	if (cfg->ior == 0)
		return -EACCES;

	ret = hal_misc_read(cfg, &data);
	if (ret != 0) {
		log_err("Failed to read misc data: %d", ret);
		return ret;
	}

	return sprintf(buf, "%u\n", data);
}

static ssize_t __misc_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct miscattr *mattr = container_of(attr, struct miscattr, attr);
	struct hal_misc_config *cfg = mattr->cfg;
	s32 ret = 0;
	u32 data = 0;

	if (cfg->iow == 0)
		return -EACCES;

	ret = kstrtou32(buf, 0, &data);
	if (ret != 0) {
		log_err("Failed to convert string to u32: %d", ret);
		return ret;
	}

	ret = hal_misc_write(cfg, &data);
	if (ret != 0) {
		log_err("Failed to write misc data: %d", ret);
		return ret;
	}

	return count;
}

static int __misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int __misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct attribute *miscattrs[] = {
#if CONFIG_MISC_0_ENABLE
	&misc_attr_0.attr.attr,
#endif

#if CONFIG_MISC_1_ENABLE
	&misc_attr_1.attr.attr,
#endif

#if CONFIG_MISC_2_ENABLE
	&misc_attr_2.attr.attr,
#endif

#if CONFIG_MISC_3_ENABLE
	&misc_attr_3.attr.attr,
#endif

#if CONFIG_MISC_4_ENABLE
	&misc_attr_4.attr.attr,
#endif

#if CONFIG_MISC_5_ENABLE
	&misc_attr_5.attr.attr,
#endif

#if CONFIG_MISC_6_ENABLE
	&misc_attr_6.attr.attr,
#endif

#if CONFIG_MISC_7_ENABLE
	&misc_attr_7.attr.attr,
#endif

#if CONFIG_MISC_8_ENABLE
	&misc_attr_8.attr.attr,
#endif

#if CONFIG_MISC_9_ENABLE
	&misc_attr_9.attr.attr,
#endif

	NULL
};

static struct attribute_group misc_attr_group = {
	.attrs = miscattrs,
};

struct file_operations miscfops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = __misc_ioctl,
	.open = __misc_open,
	.release = __misc_release,
};

struct miscdevice miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &miscfops,
	.name = "misc",
	.mode = 0666,
};

static int __init misc_init(void)
{
	int ret = 0;

	ret = hal_misc_init();
	if (ret < 0) {
		log_err("Failed to initialize HAL misc: %d", ret);
		return ret;
	}

	ret = misc_register(&miscdev);
	if (ret < 0) {
		log_err("Failed to register misc device: %d", ret);
		return ret;
	}

	ret = sysfs_create_group(&miscdev.this_device->kobj, &misc_attr_group);
	if (ret < 0) {
		log_err("Failed to create sysfs group: %d", ret);
		return ret;
	}

	return 0;
}

static void __exit misc_exit(void)
{
	misc_deregister(&miscdev);
}

module_init(misc_init);
module_exit(misc_exit);

MODULE_AUTHOR("Avalue Technology Inc.");
MODULE_AUTHOR("Arthur Huang <arthur_huang@avalue.com>");
MODULE_DESCRIPTION("Miscellaneous driver for Avalue boards");
MODULE_LICENSE("GPL");
MODULE_VERSION(CONFIG_DRIVER_VERSION);
