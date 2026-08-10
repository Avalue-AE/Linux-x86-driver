# Hardware Monitor (HWM) User Guide

This driver exposes the onboard hardware monitor — supply **voltages**,
**temperatures**, **fan** tachometers, and **PWM** fan outputs — through the
standard Linux **hwmon** subsystem. Because it is a normal hwmon device, you can
read it with ordinary tools such as `lm-sensors` (`sensors`) or directly from
`/sys/class/hwmon/`, with no proprietary API.

Which channels appear, their labels, and their scaling are defined entirely by
the board's `configs/boards/<BOARD_NAME>.conf` (the `CONFIG_HWM_*` keys) — not
hard-coded in the driver. That file is supplied with the board and is not bundled with every copy of this source — the public GitHub mirror, for example, ships without it.

**On kernel 4.15 the file paths are one level deeper.** There the driver registers through plain sysfs attribute groups, so its sensor files sit in `in/`, `temp/`, `fan/` and `pwm/` subdirectories under `/sys/class/hwmon/hwmonN/`, instead of the flat `inN_input` files. `sensors` does not read that layout, so section 2 does not work on 4.15 — use sections 3 and 4 with the extra subdirectory in the path. Finding the device (section 1) is unchanged. Kernel 5.4 and newer have both layouts, so everything below applies to them as written. This 4.15 path has been built and linked against a real 4.15 tree with its own `Module.symvers`, and modpost resolved every symbol it imports — but it has not been run on 4.15 hardware.

## 1. Prerequisites

* **Driver loaded:** the `hwm.ko` module must be loaded (`make ... hwmon` then
  `sudo make ... install`, or `sudo insmod hwm.ko`). Check with `lsmod`.
* **hwmon device present:** a class device must exist under
  `/sys/class/hwmon/`.

```bash
# Verify the driver is loaded
lsmod | grep hwm

# Find this driver's hwmon device (it registers under the board's HWM chip
# name -- one of "ite", "nct61x6d" or "f81966", from CONFIG_HWM_CHIPSET)
grep -H . /sys/class/hwmon/hwmon*/name
# e.g. /sys/class/hwmon/hwmon3/name:ite
```

Note the `hwmonX` number that reports your board's chip name — the examples
below use `hwmon3`; substitute your own.

---

## 2. Using `lm-sensors` (recommended)

`lm-sensors` is the standard toolset for reading hwmon devices.

On kernel 4.15, `sensors` will not show this driver's channels. Read them from sysfs instead (section 3) — see the 4.15 note at the top of this guide.

### 2.1 Installation

**Debian / Ubuntu:**

```bash
sudo apt update
sudo apt install lm-sensors
```

**RHEL / CentOS / Fedora:**

```bash
sudo dnf install lm_sensors
```

### 2.2 Reading the sensors

This driver registers itself automatically, so you do **not** need to run
`sensors-detect` for it — just run:

```bash
sensors
```

**Example output:**

```text
ite-isa-0000
Adapter: ISA adapter
VIN:          12.09 V
VCORE:         0.85 V
VDDQ:          1.20 V
CPU:          +45.0°C
System:       +38.0°C
CPU Fan:      3245 RPM
```

The channel names (`VIN`, `VCORE`, `CPU`, …) are the labels from the board's
`.conf`. If you want a friendlier chip title or to rename channels, add an entry
under `/etc/sensors.d/` (see `man sensors.conf`).

---

## 3. Reading directly from sysfs

Every channel is a plain sysfs file under `/sys/class/hwmon/hwmonX/` — on kernel 4.15, under the `in/`, `temp/`, `fan/` and `pwm/` subdirectories of that path. Values use the standard hwmon units, so no conversion is needed:

| Attribute      | Quantity     | Unit                         |
|----------------|--------------|------------------------------|
| `inN_input`    | voltage      | millivolts (mV)              |
| `tempN_input`  | temperature  | millidegrees Celsius (m°C)   |
| `fanN_input`   | fan speed    | revolutions per minute (RPM) |
| `pwmN`         | fan duty     | percent, 0–100 (see §4)      |
| `<attr>_label` | channel name | text, from the `.conf`       |

**Channel numbers differ between the two layouts.** In the subdirectory layout the numbers are the driver's own and start at 0: `in/in0_input`, `temp/temp0_input`, `fan/fan0_input`, `pwm/pwm0`. In the flat layout the hwmon core picks the numbers, and for temperatures, fans and PWM it adds 1 to the `.conf` index: index 0 becomes `temp1_input`, `fan1_input`, `pwm1`; index 2 becomes `temp3_input`, and so on. Voltages are the exception — the core starts them at 0, so `in0_input` is the same channel in both layouts. The flat layout also has no PWM label files; read `pwm/pwm0_label` for those.

```bash
cd /sys/class/hwmon/hwmon3
# On kernel 4.15 the files are one level deeper, in in/, temp/, fan/ and pwm/.
# The loop below finds nothing there -- use the 4.15 block that follows this one.

# List every channel with its label
for f in in*_label temp*_label fan*_label pwm*_label; do
    [ -e "$f" ] && printf "%-12s %s\n" "$f" "$(cat "$f")"
done

# Read one voltage rail (mV) and its label
cat in1_label   # e.g. VIN
cat in1_input   # e.g. 12090   -> 12.090 V

# Temperature (m°C -> divide by 1000 for °C); the core numbers these from 1
cat temp1_input # e.g. 45000   -> 45.0 °C

# Fan speed (RPM); numbered from 1 here as well
cat fan1_input  # e.g. 3245
```

**On kernel 4.15**, the same listing has to walk the four subdirectories, because the flat names above do not exist there:

```bash
cd /sys/class/hwmon/hwmon3

# List every channel with its label
for d in in temp fan pwm; do
    for f in "$d"/*_label; do
        [ -e "$f" ] && printf "%-20s %s\n" "$f" "$(cat "$f")"
    done
done

# The same reads as above, one directory down
cat in/in1_input      # e.g. 12090   -> 12.090 V
cat temp/temp0_input  # e.g. 45000   -> 45.0 °C
cat fan/fan0_input    # e.g. 3245
```

Kernel 5.4 and newer carry both layouts, so this block works there too. The flat block above works on those kernels as well, with the channel numbers the core assigns — see the note under the table. Like the rest of the 4.15 path in this guide, these paths have been built and linked against a real 4.15 tree with its own `Module.symvers`, and modpost resolved every symbol they import — but they have not been run on 4.15 hardware.

Only the channels the board enables (`CONFIG_HWM_*_ENABLE=1`) are present, **and the numbering keeps the gaps**. The number in a file name is the channel's index in the board `.conf`; it is not a count of what is there. A board that enables voltage indexes 1, 2 and 4 exposes `in1_input`, `in2_input` and `in4_input` — there is no `in0_input` and no `in3_input`. This is the normal case, not a corner case: 94 of the 108 board `.conf` files that have a hardware monitor have at least one gap. List the directory and read the `_label` files to see what your board actually has, rather than assuming the channel you want is number 0.

---

## 4. Controlling fan PWM

If the board exposes PWM channels, `pwmN` is writable. **Note:** unlike the
standard hwmon range of 0–255, this driver uses a **duty percentage 0–100**.

```bash
# The pwm/ subdirectory is present on every kernel and its numbers are the
# board .conf indexes, so use it for PWM on all versions:
cd /sys/class/hwmon/hwmon3/pwm

# Always look first -- which PWM channels does THIS board have, and what are they?
for f in *_label; do printf "%-12s %s\n" "$f" "$(cat "$f")"; done
# e.g. pwm1_label   Backlight
#      pwm3_label   CPU Fan PWM      <- this board has no pwm0 and no pwm2

# Then read and set the channel you found. Substitute your own number.
cat pwm3            # current duty, 0-100 (%)
echo 60 | sudo tee pwm3   # set it to 60% duty (root)
```

**Read the labels before you write.** `pwm0` does not exist on every board — on ARC-SKLU, EBM-BYTS, EMX-MTLP, EPX-APLP, EPX-EHLP and EZX-EHLP the PWM indexes start higher, and on most of those the lowest channel present is the **backlight**, not a fan. Writing a fan duty to it dims the display. The `_label` listing above is what tells you which number is the fan on the board in front of you.

On kernel 5.4 and newer the flat `/sys/class/hwmon/hwmon3/` path carries the same channels one number higher — `.conf` index 1 is `pwm2`, index 3 is `pwm4` — and it has no PWM label files at all, so from the flat path alone there is no way to tell which channel is the fan. The `pwm/` path above reads the same on every kernel and has the labels.

---

## 5. Notes & troubleshooting

* **No hwmon device / no chip name under `/name`.** Confirm `hwm.ko` is
  loaded (`lsmod | grep hwm`) and that the board's `.conf` declares a
  complete HWM block — both `MAKE_HWM_DEVICE` and `MAKE_HWM_CHIPSET`. A board
  without a hardware monitor builds no `hwm.ko`; a board whose HWM block
  leaves the chipset empty also builds none, and prints `skipping hwmon`
  during `make`.
* **`sensors` prints nothing for this driver on kernel 4.15.** On 4.15 the driver registers with plain attribute groups, so its files are inside `in/`, `temp/`, `fan/` and `pwm/` subdirectories and `libsensors` does not look there. The device itself is present -- `cat /sys/class/hwmon/hwmon*/name` still prints the board's chip name -- so read the values from sysfs (section 3). Kernel 5.4 and newer are not affected.
* **Readings look wrong (off by a constant factor, implausible fan RPM).** The
  per-channel scaling comes from the `.conf` (`_LSB`, `_R1`/`_R2` for voltages,
  `_SPEED_STEP` for fans). Board configs ported from the legacy driver are
  marked **NOT HARDWARE-VALIDATED** until bench-tested, so an unconfirmed `LSB`
  or divider shows up as a voltage off by a constant factor, and a wrong fan
  tach register reads a nonsense RPM. Verify these against the board's EC
  register map before trusting the numbers.
* **Which chip am I looking at?** This driver registers under the board's
  HWM chip name (`ite`, `nct61x6d` or `f81966`, from `CONFIG_HWM_CHIPSET`);
  other `hwmonX` nodes on the system (CPU package, NVMe, etc.) are unrelated.
  Match by `cat /sys/class/hwmon/hwmon*/name`.
* **`sensors` shows raw `inN` names instead of labels.** The labels are exposed
  via `inN_label`; a very old `libsensors` may ignore them. Reading sysfs
  directly (§3) always shows the `.conf` labels.
