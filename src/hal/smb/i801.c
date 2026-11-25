
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"

#include "hal/smb/smb.h"

#include "log.h"

/* Intel i801 host status register (base+0) */
#define I801_STS_HOST_BUSY	BIT(0)
#define I801_STS_INTR		BIT(1)
#define I801_STS_DEV_ERR	BIT(2)
#define I801_STS_BUS_ERR	BIT(3)
#define I801_STS_FAILED		BIT(4)
#define I801_STS_INUSE		BIT(6)
#define I801_STS_BYTE_DONE	BIT(7)

#define I801_STS_ERRORS		(I801_STS_DEV_ERR | I801_STS_BUS_ERR | \
				 I801_STS_FAILED)
#define I801_STS_FLAGS		(I801_STS_BYTE_DONE | I801_STS_INTR | \
				 I801_STS_ERRORS)

/* Intel i801 host control register (base+2) */
#define I801_CTL_KILL		BIT(1)
#define I801_CTL_BYTE_DATA	0x08
#define I801_CTL_START		BIT(6)

/* 800 polls of usleep_range(250, 500) wait at least 200 ms */
#define I801_TIMEOUT_POLLS	800

/* The PCI BAR that holds an Intel SMBus controller's I/O ports */
#define I801_SMBBAR		4

/* The ports this HAL drives: host status (base+0) up to block data (base+7) */
#define I801_IO_SIZE		(SMBUS_HOST_BLOCK(0) + 1)

/* The name the port range is held under, as /proc/ioports shows it */
#define I801_REGION_NAME	"avalue-smbus"

/* Serialises whole transactions: the host runs one at a time */
static DEFINE_MUTEX(__i801_mutex);

/* The base port of the range smb_probe() holds, or 0 when it holds none */
static u16 __i801_claimed_port;

/**
 * SMBus i801-like operations
 * In general, SMBus operations are similar across different chips
 * This implementation assumes i801-like behavior for simplicity
 * 
 * Here is we implement basic smbus probe/read/write for chips as follows:
 * - PCA9555
 * - NCT5655
 */

/*
 * Reports, in one log line, that the port range could not be claimed and who
 * holds it. The in-kernel i2c-i801 driver binds the SMBus function whose BAR
 * holds the same ports, so the driver bound to that function is the holder.
 * A range that is no SMBus function's BAR is held by something this file
 * cannot name.
 *
 * The holder can unbind at any moment (root writes its PCI address to the
 * driver's unbind file), and the driver core clears dev.driver when it does.
 * So the field is read once, into a local pointer, and that one pointer is
 * both tested and dereferenced: a second read after the test could find NULL.
 */
static void __i801_report_conflict(u16 base)
{
	struct pci_dev *pdev = NULL;
	struct device_driver *drv;
	unsigned long first = base;
	unsigned long last = base + I801_IO_SIZE - 1;

	for (pdev = NULL;
	     (pdev = pci_get_class(PCI_CLASS_SERIAL_SMBUS << 8, pdev));) {
		if (pci_resource_end(pdev, I801_SMBBAR) < first ||
		    pci_resource_start(pdev, I801_SMBBAR) > last)
			continue;

		drv = READ_ONCE(pdev->dev.driver);
		if (drv) {
			log_err("SMBus I/O ports 0x%04lX-0x%04lX are held by driver \"%s\" (PCI %02x:%02x.%x), not loading\n",
				first, last, drv->name,
				pdev->bus->number, PCI_SLOT(pdev->devfn),
				PCI_FUNC(pdev->devfn));
		} else {
			log_err("SMBus I/O ports 0x%04lX-0x%04lX are held by another driver or resource (PCI %02x:%02x.%x has no driver bound), not loading\n",
				first, last, pdev->bus->number,
				PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		}
		pci_dev_put(pdev);
		return;
	}

	log_err("SMBus I/O ports 0x%04lX-0x%04lX are held by another driver or resource, not loading\n",
		first, last);
}

/*
 * Takes the port range this HAL drives, so no other driver can use it while
 * this one does. Fails with -EBUSY, before any port access, when another
 * driver holds any of it (the in-kernel i2c-i801 driver, usually): it runs its
 * own transactions on the same registers under a lock this HAL cannot take.
 */
static s32 __i801_claim(struct smb_device *dev)
{
	u16 base = dev->cfg->base_port;

	if (!request_region(base, I801_IO_SIZE, I801_REGION_NAME)) {
		__i801_report_conflict(base);
		return -EBUSY;
	}

	__i801_claimed_port = base;
	return 0;
}

/**
 * smb_probe - Probe for i801-like SMBus controller and take its I/O ports
 * Return: 0 if detected and the ports are ours, -ENODEV if not found,
 *         -EBUSY if another driver holds the ports
 */
s32 smb_probe(struct smb_device *dev)
{
	struct pci_dev *pdev = NULL;
	u16 port = 0;

	// check base port is designated
	if (dev->cfg->base_port != 0) {
		log_debug("SMBus base port specified: 0x%04X\n",
			  dev->cfg->base_port);
		return __i801_claim(dev);
	}

	// scan pci bus for SMBus devices
	for (pdev = NULL;
	     (pdev = pci_get_class(PCI_CLASS_SERIAL_SMBUS << 8, pdev));) {
		port = pci_resource_start(pdev, I801_SMBBAR);
		if (port > 0) {
			dev->cfg->base_port = port;
			log_debug(
				"Detected SMBus controller at PCI %02x:%02x.%x, base port 0x%04X\n",
				pdev->bus->number, PCI_SLOT(pdev->devfn),
				PCI_FUNC(pdev->devfn), dev->cfg->base_port);
			pci_dev_put(pdev);
			return __i801_claim(dev);
		}
	}

	return -ENODEV;
}

/**
 * smb_remove - Give back the port range smb_probe() took
 * @dev: SMBus device handle
 */
void smb_remove(struct smb_device *dev)
{
	(void)dev;

	if (__i801_claimed_port == 0)
		return;

	release_region(__i801_claimed_port, I801_IO_SIZE);
	__i801_claimed_port = 0;
}

/*
 * Waits for the host to go idle, then clears the completion and error flags an
 * earlier transaction left behind. The caller holds __i801_mutex.
 */
static s32 __i801_wait_idle(struct smb_device *dev)
{
	u8 status;
	u32 count;

	for (count = 0; count < I801_TIMEOUT_POLLS; count++) {
		status = hal_port_read8(SMBUS_HOST_STATUS(dev->cfg->base_port));
		if (!(status & I801_STS_HOST_BUSY)) {
			hal_port_write8(SMBUS_HOST_STATUS(dev->cfg->base_port),
					I801_STS_FLAGS);
			return 0;
		}
		usleep_range(250, 500);
	}

	return -EBUSY;
}

/*
 * Clears the status flags and hands the INUSE semaphore back to BIOS/ACPI, as
 * the in-kernel i2c-i801 driver does after each transaction. The caller holds
 * __i801_mutex.
 */
static void __i801_release(struct smb_device *dev)
{
	hal_port_write8(SMBUS_HOST_STATUS(dev->cfg->base_port),
			I801_STS_INUSE | I801_STS_FLAGS);
}

/*
 * Runs one byte-data transaction and waits for the host to report completion
 * or an error. The caller holds __i801_mutex.
 */
static s32 __i801_transaction(struct smb_device *dev, u8 addr, u8 cmd,
			      bool read, u8 *data)
{
	u8 status;
	u32 count;
	u8 control;

	s32 ret;

	ret = __i801_wait_idle(dev);
	if (ret)
		return ret;

	hal_port_write8(SMBUS_HOST_ADDRESS(dev->cfg->base_port),
			read ? addr + 1 : addr);
	hal_port_write8(SMBUS_HOST_COMMAND(dev->cfg->base_port), cmd);
	if (!read)
		hal_port_write8(SMBUS_HOST_DATA0(dev->cfg->base_port), *data);

	hal_port_write8(SMBUS_HOST_CONTROL(dev->cfg->base_port),
			I801_CTL_START | I801_CTL_BYTE_DATA);

	for (count = 0; count < I801_TIMEOUT_POLLS; count++) {
		usleep_range(250, 500);
		status = hal_port_read8(SMBUS_HOST_STATUS(dev->cfg->base_port));
		if (!(status & I801_STS_HOST_BUSY) &&
		    (status & (I801_STS_INTR | I801_STS_ERRORS)))
			break;
	}

	if (count == I801_TIMEOUT_POLLS) {
		control = hal_port_read8(SMBUS_HOST_CONTROL(dev->cfg->base_port));
		hal_port_write8(SMBUS_HOST_CONTROL(dev->cfg->base_port),
				control | I801_CTL_KILL);
		usleep_range(1000, 2000);
		control = hal_port_read8(SMBUS_HOST_CONTROL(dev->cfg->base_port));
		hal_port_write8(SMBUS_HOST_CONTROL(dev->cfg->base_port),
				control & (u8)~I801_CTL_KILL);
		log_debug("Transaction timed out\n");
		return -ETIMEDOUT;
	}

	if (status & I801_STS_FAILED) {
		log_debug("Transaction failed\n");
		return -EIO;
	}
	if (status & I801_STS_BUS_ERR) {
		log_debug("Bus collision\n");
		return -EAGAIN;
	}
	if (status & I801_STS_DEV_ERR) {
		log_debug("Device not ACK\n");
		return -ENXIO;
	}

	if (read)
		*data = hal_port_read8(SMBUS_HOST_DATA0(dev->cfg->base_port));

	log_debug("Transaction completed\n");
	return 0;
}

/**
 * smb_ready - Check if SMBus host is ready for a new transaction
 * @dev: SMBus device handle
 * Return: 0 if ready, -EBUSY if the host stays busy
 */
s32 smb_ready(struct smb_device *dev)
{
	s32 ret;

	mutex_lock(&__i801_mutex);
	ret = __i801_wait_idle(dev);
	__i801_release(dev);
	mutex_unlock(&__i801_mutex);

	return ret;
}

/**
 * smb_read8 - Read one byte from SMBus device
 * @dev: SMBus device handle
 * @addr: 8-bit device address (write address)
 * @cmd: Command/register address
 * @out: Output buffer for read byte
 * Return: 0 on success, -ENXIO no ACK, -EAGAIN bus collision, -EIO failed,
 *         -ETIMEDOUT did not complete, -EBUSY host stayed busy
 */
s32 smb_read8(struct smb_device *dev, u8 addr, u8 cmd, u8 *out)
{
	u8 data;
	s32 ret;

	mutex_lock(&__i801_mutex);
	ret = __i801_transaction(dev, addr, cmd, true, &data);
	__i801_release(dev);
	mutex_unlock(&__i801_mutex);

	if (ret) {
		log_debug("smb_read8 failed: %d\n", ret);
		return ret;
	}

	log_debug("smb_read8: addr=0x%02X, cmd=0x%02X, data=0x%02X\n",
		  addr, cmd, data);
	*out = data;

	return 0;
}

/**
 * smb_write8 - Write one byte to SMBus device
 * @dev: SMBus device handle
 * @addr: 8-bit device address (write address)
 * @cmd: Command/register address
 * @value: Value to write
 * Return: 0 on success, -ENXIO no ACK, -EAGAIN bus collision, -EIO failed,
 *         -ETIMEDOUT did not complete, -EBUSY host stayed busy
 */
s32 smb_write8(struct smb_device *dev, u8 addr, u8 cmd, u8 value)
{
	s32 ret;

	mutex_lock(&__i801_mutex);
	ret = __i801_transaction(dev, addr, cmd, false, &value);
	__i801_release(dev);
	mutex_unlock(&__i801_mutex);

	if (ret) {
		log_debug("smb_write8 failed: %d\n", ret);
		return ret;
	}

	log_debug("smb_write8: addr=0x%02X, cmd=0x%02X, value=0x%02X\n",
		  addr, cmd, value);

	return 0;
}
