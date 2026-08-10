
#include <linux/io.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "configs/config.h"

#include "hal/bytes.h"
#include "hal/bctrl.h"

#include "hal/smb/smb.h"

#include "log.h"

DEFINE_SPINLOCK(__i801_spinlock);

static void __i801_lock(void)
{
	spin_lock(&__i801_spinlock);
}

static void __i801_unlock(void)
{
	spin_unlock(&__i801_spinlock);
}

/**
 * SMBus i801-like operations
 * In general, SMBus operations are similar across different chips
 * This implementation assumes i801-like behavior for simplicity
 * 
 * Here is we implement basic smbus probe/read/write for chips as follows:
 * - PCA9555
 * - NCT5655
 */

/**
 * smb_probe - Probe for i801-like SMBus controller
 * Return: 0 if detected, -ENODEV if not found
 */
s32 smb_probe(struct smb_device *dev)
{
	struct pci_dev *pdev = NULL;
	u16 port = 0;

	// check base port is designated
	if (dev->cfg->base_port != 0) {
		log_debug("SMBus base port specified: 0x%04X\n",
			  dev->cfg->base_port);
		return 0;
	}

	// scan pci bus for SMBus devices
	for (pdev = NULL;
	     (pdev = pci_get_class(PCI_CLASS_SERIAL_SMBUS << 8, pdev));) {
		port = pci_resource_start(pdev, 4);
		if (port > 0) {
			dev->cfg->base_port = port;
			log_debug(
				"Detected SMBus controller at PCI %02x:%02x.%x, base port 0x%04X\n",
				pdev->bus->number, PCI_SLOT(pdev->devfn),
				PCI_FUNC(pdev->devfn), dev->cfg->base_port);
			return 0;
		}
	}

	return -ENODEV;
}

/**
 * smb_ready - Wait until SMBus controller is ready
 * Return: 0 if ready, -ETIMEDOUT on timeout
 */
s32 smb_ready(struct smb_device *dev)
{
	u8 status = 0;
	s32 timeout = 1000; // 1 second timeout

	hal_port_write8(SMBUS_HOST_STATUS(dev->cfg->base_port), 0xFF);

	while (timeout--) {
		status = hal_port_read8(SMBUS_HOST_STATUS(dev->cfg->base_port));

		if ((status & SMBUS_ERROR_BUSY) == 0)
			break;

		usleep_range(1000, 2000); // sleep 1 ms
	}

	log_debug("status=0x%02X, timeout=%d\n", status, timeout);
	return (status & SMBUS_ERROR_BUSY) > 0 || timeout == 0 ? -ETIMEDOUT : 0;
}

/**
 * smb_read8 - Read 8-bit value from SMBus device
 * Return: 0 on success, negative error code on failure
 */
s32 smb_read8(struct smb_device *dev, u8 addr, u8 cmd, u8 *out)
{
	s32 ret = 0;

	ret = smb_ready(dev);
	if (ret < 0)
		return ret;

	__i801_lock();
	// hal_port_write8(SMBUS_HOST_DATA0(dev->cfg->base_port), 0x00);
	hal_port_write8(SMBUS_HOST_ADDRESS(dev->cfg->base_port), addr + 1);
	hal_port_write8(SMBUS_HOST_COMMAND(dev->cfg->base_port), cmd);
	hal_port_write8(SMBUS_HOST_CONTROL(dev->cfg->base_port), 0x48);
	__i801_unlock();
	usleep_range(500, 1000);

	ret = smb_ready(dev);
	if (ret < 0)
		return ret;

	*out = hal_port_read8(SMBUS_HOST_DATA0(dev->cfg->base_port));
	log_debug("read8 from addr 0x%02X cmd 0x%02X: 0x%02X\n", addr, cmd,
		  *out);
	return ret;
}

/**
 * smb_write8 - Write 8-bit value to SMBus device
 * Return: 0 on success, negative error code on failure
 */
s32 smb_write8(struct smb_device *dev, u8 addr, u8 cmd, u8 val)
{
	s32 ret = 0;

	ret = smb_ready(dev);
	if (ret < 0)
		return ret;

	__i801_lock();
	hal_port_write8(SMBUS_HOST_ADDRESS(dev->cfg->base_port), addr + 0);
	hal_port_write8(SMBUS_HOST_COMMAND(dev->cfg->base_port), cmd);
	hal_port_write8(SMBUS_HOST_DATA0(dev->cfg->base_port), val);
	hal_port_write8(SMBUS_HOST_CONTROL(dev->cfg->base_port), 0x48);
	__i801_unlock();
	usleep_range(500, 1000);

	ret = smb_ready(dev);
	if (ret < 0)
		return ret;

	return ret;
}
