# Linux Kernel GPIO Driver User Guide

This driver exposes the onboard GPIO pins (controlled by the SMBus/I2C bridge) to the standard Linux GPIO subsystem. Unlike legacy drivers that required proprietary APIs, this driver allows you to control hardware using standard Linux tools and libraries.

## 1. Prerequisites

Before interacting with the GPIO driver, ensure the following requirements are met:

* **Driver Loaded:** The kernel module (e.g., `gpio.ko`) must be successfully loaded. Check with `lsmod`.
* **Device Node:** A character device node `/dev/gpiochipX` (where X is a number) must exist.
* **Root Privileges:** Accessing hardware directly usually requires `sudo` or root permissions.

---

## 2. Using Standard Linux GPIO Tools (`libgpiod`)

We strongly recommend using **`libgpiod`**. It is the standard C library and tools for interacting with the Linux GPIO character device. It replaces the legacy sysfs interface.

### 2.1 Installation

Depending on your Linux distribution, install the necessary packages:

**Debian / Ubuntu / Raspberry Pi OS:**

```bash
sudo apt-get update
sudo apt-get install gpiod libgpiod-dev

```

**RHEL / CentOS / Fedora:**

```bash
sudo dnf install libgpiod-utils

```
### 2.2 Configuration & Identification

Unlike the old sysfs method, you do not need to "export" pins manually. The driver exposes a "Chip" (Controller), and the Chip manages "Lines" (Pins).

To find your specific GPIO controller (especially if you have multiple, like PCH GPIO, SIO GPIO, etc.):

```bash
gpiodetect

```

**Example Output** (from a Tiger Lake board; the chip numbers, the platform controller's label and every line count depend on the board):

```text
gpiochip0 [INT34C5:00] (360 lines)
gpiochip1 [gpio] (16 lines)  <-- This is your target driver

```

The label in square brackets is what tells the chips apart. This driver's chip is the one labelled exactly `[gpio]`. Every line of `gpiodetect` output starts with the word `gpiochip`, so searching that output for the text `gpio` matches every chip, including the platform's own controller.

> **Warning:** Drive only the chip labelled `[gpio]`. The platform controller's lines carry board functions such as CPU voltage control; setting one of them as an output can power the board off at once.

### 2.3 GPIO Chip Management

Once you identified that your driver is `gpiochip1`, you will use this identifier for all commands.

* **Chip Name:** `gpiochip1`
* **Label:** `gpio` (Defined in driver code)

---

## 3. Verification & Testing

Use the following tools to verify driver functionality and hardware connectivity.

### `gpioinfo` - Check Pin Status

Displays the current status, direction, and configuration of all lines on a chip.

**Syntax:** `gpioinfo <chip_name>`

```bash
# Check status of gpio (assuming it is gpiochip1)
sudo gpioinfo gpiochip1

```

**Output Explanation:**

```text
gpiochip1 - 16 lines:
        line   0:      unnamed       unused   input  active-high 
        line   1:      "sys_led"     output   active-high [used]
        line   2:      unnamed       unused   input  active-high 

```

* **line:** The pin offset (0-15).
* **unused/kernel:** "unused" means available for userspace; "kernel" means a driver is claiming it.
* **input/output:** Current electrical direction.

### `gpioget` - Read Input Value

Reads the signal level of one or more GPIO lines.

**Syntax:** `gpioget <chip_name> <offset>`

```bash
# Read Pin 2 (Offset 2) on gpiochip1
sudo gpioget gpiochip1 2

# Output:
# 0  (Low)
# 1  (High)

```

### `gpioset` - Write Output Value

Sets the signal level of one or more GPIO lines.

**Syntax:** `gpioset <chip_name> <offset>=<value>`

```bash
# Set Pin 1 to HIGH (1)
sudo gpioset gpiochip1 1=1

# Set Pin 1 to LOW (0)
sudo gpioset gpiochip1 1=0

```

> **⚠️ Important Note on Persistence:**
> `gpioset` holds the line ONLY while the process is running. In some drivers/hardware, when `gpioset` exits, the pin might revert to its default state or input mode.
> If you need the LED to *stay* on, use the `-m time` (mode) flag or a daemon script (see Quick Start).

---

## 4. Quick Start (Shell Script)

This script automatically detects the GPIO chip by its label (so you don't worry if it's `gpiochip0` or `gpiochip99`) and performs a simple toggle test. It matches the label column exactly and stops unless exactly one chip carries that label, so it never falls back to the platform's own GPIO controller when this driver is not loaded (see the warning in section 2.2).

**File:** `test_gpio.sh`

```bash
#!/bin/bash

# ================= Configuration =================
# The 'label' name you defined in the driver (chip.label)
TARGET_LABEL="gpio"
# The Pin offset you want to test (0 ~ 15)
TEST_PIN=0
# =================================================

# 1. Find the gpiochip whose label column is exactly [$TARGET_LABEL].
#    A plain text search would also match the platform's own controller,
#    because every gpiodetect line starts with "gpiochip".
CHIP_NAME=$(gpiodetect | awk -v want="[$TARGET_LABEL]" '$2 == want {print $1}')

if [ "$(echo $CHIP_NAME | wc -w)" -ne 1 ]; then
    echo "Error: expected exactly one GPIO chip labelled '$TARGET_LABEL', found:" ${CHIP_NAME:-none}
    echo "Is the driver loaded?"
    exit 1
fi

echo "Found Driver: $TARGET_LABEL at /dev/$CHIP_NAME"

# 2. Read Current Direction and Value
echo "Reading Pin $TEST_PIN status..."
gpioinfo $CHIP_NAME | grep "line\s\+$TEST_PIN"

# 3. Perform a Toggle Test (Blink)
echo "----------------------------------------"
echo "Starting Blink Test on Pin $TEST_PIN..."
echo "Press CTRL+C to stop."

while true; do
    # Set High
    echo "Writing 1 (High)"
    # usage: gpioset [options] <chip> <offset>=<value>
    # Note: We don't exit gpioset immediately here, but in a script
    # we usually just set it. 
    gpioset $CHIP_NAME $TEST_PIN=1
    sleep 1

    # Set Low
    echo "Writing 0 (Low)"
    gpioset $CHIP_NAME $TEST_PIN=0
    sleep 1
done

```

### How to run:

1. Save as `test_gpio.sh`.
2. Make executable: `chmod +x test_gpio.sh`.
3. Run: `sudo ./test_gpio.sh`.

---

## 5. Troubleshooting

### 5.1 `insmod` fails with "Device or resource busy"

**Symptom.** Loading the module fails:

```text
$ sudo insmod gpio.ko
insmod: ERROR: could not insert module gpio.ko: Device or resource busy
```

`sudo modprobe gpio` fails the same way (`could not insert 'gpio': Device or resource busy`).

This section is for boards whose GPIO chip (NCT5655, PCA9555 or TCA9555) is reached through an Intel SMBus controller. A board whose GPIO sits on an ITE embedded controller, or whose SMBus controller is a Zhaoxin one, does not take these ports and does not print the lines below.

The kernel log says why. Run `sudo dmesg | tail -n 8`. The refused load writes four lines: one info line, then three error lines. The first error line names the cause (the chip line reads `PCA9555` on a PCA9555 or TCA9555 board):

```text
[ADP-226 INFO ] Initializing GPIO driver
[ADP-226 ERROR ] SMBus I/O ports 0x0580-0x0587 are held by driver "i801_smbus" (PCI 00:1f.4), not loading
[ADP-226 ERROR ] NCT5655 SMBus probe failed
[ADP-226 ERROR ] Failed to initialize HAL GPIO: -16
```

The text in square brackets is your board name. The port range and the PCI address are examples: they differ from board to board. A debug build (`make gpio-debug`) also prints the function name and the line number after the bracket. The `-16` is `-EBUSY`, the same "Device or resource busy" that `insmod` printed.

If the first error line reads `are held by another driver or resource` instead of naming a driver, something other than a bound driver holds those ports. `grep -i smbus /proc/ioports` shows which name covers the range.

**Why it happens.** The GPIO chip sits behind the board's SMBus controller, and this module talks to it through that controller's registers. On a stock Linux distribution the in-kernel `i2c_i801` driver binds the same controller at boot and runs its own transactions on the same registers. Two drivers on one set of registers abort each other's transactions, so this module takes the eight ports it needs when it loads. It refuses to load when another driver already holds any of them, and it touches no port before it refuses.

**Check who holds the ports:**

```bash
lsmod | grep i2c_i801
grep -i -e smbus -e i801 /proc/ioports
```

**Fix, for this boot.** Unload `i2c_i801`, then load this module:

```bash
sudo modprobe -r i2c_i801
sudo modprobe gpio
```

If `modprobe -r i2c_i801` says the module is in use, a program still has one of the bus's `/dev/i2c-*` device files open (the kernel counts an open device file as a user of the module). Stop that program, then run the command again. While `i2c_i801` is unloaded, everything else that uses this SMBus controller stops working: `i2cdetect`, `sensors` readings that come from devices on it, and memory-module data readers.

**Fix, for every boot.** Keep `i2c_i801` from loading by itself. Create `/etc/modprobe.d/avalue-gpio.conf` containing:

```ini
blacklist i2c_i801
```

Then rebuild the initial RAM disk, so that the boot code honours it: `sudo update-initramfs -u` on Debian and Ubuntu, `sudo dracut -f` on RHEL, CentOS and Fedora. `blacklist` stops only the automatic load. A `modprobe i2c_i801` typed by hand still loads it. `sudo make install` makes this module load at every boot, and with `i2c_i801` kept out it finds the ports free.

**If `lsmod` does not list `i2c_i801` but `/proc/ioports` still shows `i801_smbus`,** the kernel was built with that driver inside it (`CONFIG_I2C_I801=y`). It cannot be unloaded or blacklisted. This module cannot load on that kernel until the kernel is rebuilt with `CONFIG_I2C_I801=m`.

### 5.2 The reverse order: this module loaded first

This module holds its eight ports from `insmod` until `rmmod`. While it is loaded, `i2c_i801` cannot take that SMBus controller.

**Symptom.** `sudo modprobe i2c_i801` prints no error, because a PCI driver loads even when it fails to bind a device. But no SMBus adapter appears: `i2cdetect -l` has no `SMBus I801 adapter` line, and `dmesg` carries the kernel's own refusal for the controller:

```text
i801_smbus 0000:00:1f.4: Failed to request SMBus region 0x580-0x59f
```

**What to do.** The two drivers cannot share the controller, so choose which one owns it. For GPIO control, this module owns it and `i2c_i801` stays out (see 5.1). To give the controller back to `i2c_i801`, unload this module first:

```bash
sudo rmmod gpio
sudo modprobe i2c_i801
```

After that, this module cannot load again until `i2c_i801` is unloaded (5.1).
