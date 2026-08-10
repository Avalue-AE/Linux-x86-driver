
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

/*
 * Zhaoxin (兆芯) SMBus host controller protocol.
 *
 * This is the second implementation of the pluggable SMBus protocol
 * interface declared in smb.h (smb_probe/smb_ready/smb_read8/smb_write8);
 * i801.c is the other. Exactly one protocol object is linked per board,
 * selected by MAKE_GPIO_PROTOCOL in the board .conf, so both files may
 * define the same smb_* symbols without colliding.
 *
 * The byte-data transaction (status/control/command/address/data at
 * base+0/2/3/4/5, control command 0x48) is identical to i801; only the
 * base-port discovery and the ready/completion polling differ, mirroring
 * the legacy driver's fun_ctl_type_zhaoxin path (core/pca9555.c).
 */

/* Zhaoxin SMBus controller is at PCI 00:11.0; its I/O base lives in a
 * config-space register rather than a BAR (legacy dio_port_designate). */
#define ZHAOXIN_SMBUS_PCI_BUS 0x00
#define ZHAOXIN_SMBUS_PCI_DEV 0x11
#define ZHAOXIN_SMBUS_PCI_FUNC 0x00
#define ZHAOXIN_SMBUS_BASE_REG 0xD0
#define ZHAOXIN_SMBUS_BASE_MASK 0xFFFE

/* Host status bits polled during a transaction (base+0) */
#define ZHAOXIN_STS_CLEAR_MASK 0xBF /* clear bit6 before starting */
#define ZHAOXIN_STS_READY 0x40 /* host ready value after clear */
#define ZHAOXIN_STS_DONE 0x02 /* transaction-complete bit */

#define ZHAOXIN_POLL_LOOPS 10

DEFINE_SPINLOCK(__zhaoxin_spinlock);

static void __zhaoxin_lock(void)
{
	spin_lock(&__zhaoxin_spinlock);
}

static void __zhaoxin_unlock(void)
{
	spin_unlock(&__zhaoxin_spinlock);
}

/**
 * smb_probe - locate the Zhaoxin SMBus controller I/O base
 * Return: 0 on success, -ENODEV if not found
 */
s32 smb_probe(struct smb_device *dev)
{
	struct pci_dev *pdev = NULL;
	u16 base = 0;

	// honour an explicit base port from the board config
	if (dev->cfg->base_port != 0) {
		log_debug("Zhaoxin SMBus base port specified: 0x%04X\n",
			  dev->cfg->base_port);
		return 0;
	}

	pdev = pci_get_domain_bus_and_slot(
		0, ZHAOXIN_SMBUS_PCI_BUS,
		PCI_DEVFN(ZHAOXIN_SMBUS_PCI_DEV, ZHAOXIN_SMBUS_PCI_FUNC));
	if (pdev == NULL) {
		log_err("Zhaoxin SMBus PCI device %02x:%02x.%x not found\n",
			ZHAOXIN_SMBUS_PCI_BUS, ZHAOXIN_SMBUS_PCI_DEV,
			ZHAOXIN_SMBUS_PCI_FUNC);
		return -ENODEV;
	}

	pci_read_config_word(pdev, ZHAOXIN_SMBUS_BASE_REG, &base);
	pci_dev_put(pdev);

	base &= ZHAOXIN_SMBUS_BASE_MASK;
	if (base == 0) {
		log_err("Zhaoxin SMBus base register empty (reg 0x%02X)\n",
			ZHAOXIN_SMBUS_BASE_REG);
		return -ENODEV;
	}

	dev->cfg->base_port = base;
	log_debug("Detected Zhaoxin SMBus at base port 0x%04X\n", base);
	return 0;
}

/**
 * smb_ready - prepare the controller for a transaction (pre-poll)
 *
 * Clears the host-status "start" bit and waits for the ready value, as the
 * legacy zhaoxin path did before each transfer. Best-effort like the legacy
 * driver: a timeout is logged but not treated as fatal.
 */
s32 smb_ready(struct smb_device *dev)
{
	u16 sts = SMBUS_HOST_STATUS(dev->cfg->base_port);
	u8 val;
	s32 i;

	val = hal_port_read8(sts);
	hal_port_write8(sts, val & ZHAOXIN_STS_CLEAR_MASK);

	for (i = 0; i < ZHAOXIN_POLL_LOOPS; i++) {
		if (hal_port_read8(sts) == ZHAOXIN_STS_READY)
			return 0;
		msleep(1);
	}

	log_debug("Zhaoxin SMBus ready poll timed out (best-effort)\n");
	return 0;
}

/**
 * __zhaoxin_wait_done - wait for transaction completion (post-poll)
 */
static void __zhaoxin_wait_done(struct smb_device *dev)
{
	u16 sts = SMBUS_HOST_STATUS(dev->cfg->base_port);
	s32 i;

	for (i = 0; i < ZHAOXIN_POLL_LOOPS; i++) {
		if ((hal_port_read8(sts) & ZHAOXIN_STS_DONE) == ZHAOXIN_STS_DONE)
			return;
		msleep(1);
	}
	log_debug("Zhaoxin SMBus completion poll timed out (best-effort)\n");
}

/**
 * smb_read8 - read one byte from an SMBus slave register
 * Return: 0 on success
 */
s32 smb_read8(struct smb_device *dev, u8 addr, u8 cmd, u8 *out)
{
	u16 base = dev->cfg->base_port;

	smb_ready(dev);

	__zhaoxin_lock();
	hal_port_write8(SMBUS_HOST_DATA0(base), 0x00);
	hal_port_write8(SMBUS_HOST_ADDRESS(base), addr + 1); // read bit set
	hal_port_write8(SMBUS_HOST_COMMAND(base), cmd);
	hal_port_write8(SMBUS_HOST_CONTROL(base), 0x48);
	__zhaoxin_unlock();

	__zhaoxin_wait_done(dev);

	*out = hal_port_read8(SMBUS_HOST_DATA0(base));
	log_debug("Zhaoxin read8 addr 0x%02X cmd 0x%02X: 0x%02X\n", addr, cmd,
		  *out);
	return 0;
}

/**
 * smb_write8 - write one byte to an SMBus slave register
 * Return: 0 on success
 */
s32 smb_write8(struct smb_device *dev, u8 addr, u8 cmd, u8 val)
{
	u16 base = dev->cfg->base_port;

	smb_ready(dev);

	__zhaoxin_lock();
	hal_port_write8(SMBUS_HOST_ADDRESS(base), addr); // write bit clear
	hal_port_write8(SMBUS_HOST_COMMAND(base), cmd);
	hal_port_write8(SMBUS_HOST_DATA0(base), val);
	hal_port_write8(SMBUS_HOST_CONTROL(base), 0x48);
	__zhaoxin_unlock();

	__zhaoxin_wait_done(dev);

	log_debug("Zhaoxin write8 addr 0x%02X cmd 0x%02X: 0x%02X\n", addr, cmd,
		  val);
	return 0;
}
