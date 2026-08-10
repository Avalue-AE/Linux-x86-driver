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

**Example Output:**

```text
gpiochip0 [Intel-PCH] (100 lines)
gpiochip1 [gpio] (16 lines)  <-- This is your target driver

```

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

This script automatically detects the GPIO chip by its label (so you don't worry if it's `gpiochip0` or `gpiochip99`) and performs a simple toggle test.

**File:** `test_gpio.sh`

```bash
#!/bin/bash

# ================= Configuration =================
# The 'label' name you defined in the driver (chip.label)
TARGET_LABEL="gpio"
# The Pin offset you want to test (0 ~ 15)
TEST_PIN=0
# =================================================

# 1. Find the gpiochip device number based on the label
CHIP_NAME=$(gpiodetect | grep "$TARGET_LABEL" | awk '{print $1}')

if [ -z "$CHIP_NAME" ]; then
    echo "Error: GPIO Chip with label '$TARGET_LABEL' not found!"
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
