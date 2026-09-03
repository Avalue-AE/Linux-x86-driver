# Linux X86 API — Avalue Driver 4.0

This page documents the Linux X86 driver, version **4.0**.
Everything a customer needs — installation, tool commands, code samples — is
in this page; you should not need anything besides this page and your board.

Driver 4.0 does not invent its own APIs. It exposes four standard Linux
kernel interfaces, and you talk to it with ordinary, widely available
userspace tools:

| Feature | Kernel interface | Recommended tool(s) |
|---|---|---|
| Digital I/O | a `gpiochip` character device | `libgpiod` (`gpiodetect`, `gpioinfo`, `gpioget`, `gpioset`) |
| Hardware monitor (voltage / temperature / fan / PWM) | a `hwmon` class device | `lm-sensors` (`sensors`), or raw sysfs |
| Watchdog | `/dev/watchdog` | the `watchdog` daemon |
| Board EC registers (COM port mode, EC firmware version, etc.) | `/dev/misc` + per-channel sysfs files | `cat`/`echo` on sysfs, or `ioctl()` |

Not every interface exists on every board. Which ones a given board builds
depends on that board's own hardware and configuration; if a module for a
feature your board does not have refuses to build, that board simply does
not implement that feature (see the "Prerequisites" note under each section
below).

---

## 1. Digital I/O (GPIO)

Onboard GPIO pins (controlled through the SMBus/I2C bridge or the embedded
controller, depending on the board) are exposed through the standard Linux
GPIO character-device subsystem. You control these pins with the standard
Linux GPIO tooling.

### 1.1 Prerequisites

* **Driver loaded:** the GPIO kernel module (e.g. `gpio.ko`) must be loaded.
  Check with `lsmod | grep gpio`.
* **Device node:** a character device node named `/dev/gpiochip` plus a
  number (for example `/dev/gpiochip0`) must exist.
* **Root privileges:** accessing GPIO hardware normally requires `sudo`.

### 1.2 Install `libgpiod`

We strongly recommend **`libgpiod`**, the standard C library and command-line
tools for the Linux GPIO character-device API. This driver does not use the
sysfs GPIO export mechanism.

**Debian / Ubuntu / Raspberry Pi OS:**

```bash
sudo apt-get update
sudo apt-get install gpiod libgpiod-dev
```

**RHEL / CentOS / Fedora:**

```bash
sudo dnf install libgpiod-utils
```

### 1.3 Finding your GPIO chip

You do not "export" pins manually. The driver
exposes a **chip** (controller), and the chip owns a set of numbered
**lines** (pins).

A system can have more than one GPIO chip (PCH GPIO, SIO GPIO, this driver's
own chip, etc.), so identify the right one first:

```bash
gpiodetect
```

Example output:

```text
gpiochip0 [Intel-PCH] (100 lines)
gpiochip1 [gpio] (16 lines)   <-- this driver's chip
```

This driver's chip reports the label `gpio`. Once you have identified it
(here, `gpiochip1`), use that name for every command below — substitute your
own chip number.

### 1.4 Checking pin status — `gpioinfo`

```bash
sudo gpioinfo gpiochip1
```

Example output:

```text
gpiochip1 - 16 lines:
        line   0:      unnamed       unused   input  active-high
        line   1:      "sys_led"     output   active-high [used]
        line   2:      unnamed       unused   input  active-high
```

* **line** — the pin offset (0 to N-1).
* **unused / kernel** — "unused" means the line is free for userspace to
  claim; "kernel" means a driver already owns it.
* **input / output** — the line's current electrical direction.

### 1.5 Reading a pin — `gpioget`

```bash
# Read line 2 on gpiochip1
sudo gpioget gpiochip1 2
# -> 0 (low) or 1 (high)
```

### 1.6 Writing a pin — `gpioset`

```bash
# Set line 1 HIGH
sudo gpioset gpiochip1 1=1

# Set line 1 LOW
sudo gpioset gpiochip1 1=0
```

> **⚠️ `gpioset` only holds a value while it is running.** Once the process
> exits, the line can revert to its default state or to input mode,
> depending on the driver and the hardware. If you need an output to *stay*
> set (e.g. an LED that must remain on), use `gpioset`'s `-m time`/`-m wait`
> hold modes, or drive it from a small daemon script such as the one below.

### 1.7 Quick-start script

This script finds the chip by its `gpio` label (so it works whether the
kernel numbers it `gpiochip0` or `gpiochip7`) and blinks one pin. Save it as
`test_gpio.sh`, `chmod +x test_gpio.sh`, then run `sudo ./test_gpio.sh`.

```bash
#!/bin/bash

# ================= Configuration =================
TARGET_LABEL="gpio"   # the label this driver's chip reports
TEST_PIN=0             # the line offset to toggle
# ===================================================

CHIP_NAME=$(gpiodetect | grep "$TARGET_LABEL" | awk '{print $1}')

if [ -z "$CHIP_NAME" ]; then
    echo "Error: GPIO chip with label '$TARGET_LABEL' not found!"
    echo "Is the driver loaded?"
    exit 1
fi

echo "Found driver: $TARGET_LABEL at /dev/$CHIP_NAME"

echo "Reading pin $TEST_PIN status..."
gpioinfo "$CHIP_NAME" | grep "line\s\+$TEST_PIN"

echo "----------------------------------------"
echo "Starting blink test on pin $TEST_PIN. Press CTRL+C to stop."

while true; do
    echo "Writing 1 (high)"
    gpioset "$CHIP_NAME" "$TEST_PIN"=1
    sleep 1

    echo "Writing 0 (low)"
    gpioset "$CHIP_NAME" "$TEST_PIN"=0
    sleep 1
done
```

---

## 2. Hardware monitor (hwmon)

Onboard supply **voltages**, **temperatures**, **fan** tachometers and
**PWM** fan outputs are exposed through the standard Linux **hwmon**
subsystem. Because it is a normal hwmon device, you read it with ordinary
tools such as `lm-sensors` (`sensors`), or directly from
`/sys/class/hwmon/`, with no proprietary API.

Which channels exist, their labels and their scaling are all defined by your
board's own configuration, not by anything hard-coded per-channel in the
driver.

**On kernel 4.15 the sysfs file paths are one level deeper than on newer
kernels.** There the driver registers through plain sysfs attribute groups,
so its sensor files sit in `in/`, `temp/`, `fan/` and `pwm/` subdirectories
under `/sys/class/hwmon/hwmonN/`, instead of the flat `inN_input`-style
files. `sensors` does not read that layout, so Section 2.2 below does not
work on kernel 4.15 — use Sections 2.3 and 2.4, with the extra subdirectory
in the path. Finding the device (Section 2.1) is unchanged. Kernel 5.4 and
newer carry both layouts, so everything in this section applies to them as
written.

### 2.1 Prerequisites

* **Driver loaded:** the hwmon kernel module (e.g. `hwm.ko`) must be loaded.
  Check with `lsmod | grep hwm`.
* **hwmon device present:** a class device must exist under
  `/sys/class/hwmon/`.

```bash
# Verify the driver is loaded
lsmod | grep hwm

# Find this driver's hwmon device among possibly several on the system
# (CPU package, NVMe, etc. also register their own hwmonX nodes)
grep -H . /sys/class/hwmon/hwmon*/name
# e.g. /sys/class/hwmon/hwmon3/name:ite
```

Note the `hwmonX` number that reports your board's chip name — the examples
below use `hwmon3`; substitute your own.

### 2.2 Using `lm-sensors` (recommended, kernel 5.4+)

`lm-sensors` is the standard toolset for reading hwmon devices. On kernel
4.15, `sensors` prints nothing for this driver — read sysfs directly
instead (Section 2.3).

**Installation — Debian / Ubuntu:**

```bash
sudo apt update
sudo apt install lm-sensors
```

**Installation — RHEL / CentOS / Fedora:**

```bash
sudo dnf install lm_sensors
```

This driver registers itself automatically, so you do **not** need to run
`sensors-detect` for it — just run:

```bash
sensors
```

Example output:

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

The channel names (`VIN`, `VCORE`, `CPU`, ...) come from your board's own
configuration. To rename channels or set a friendlier chip title, add an
entry under `/etc/sensors.d/` (see `man sensors.conf`).

### 2.3 Reading directly from sysfs

Every channel is a plain sysfs file under `/sys/class/hwmon/hwmonX/` — on
kernel 4.15, under the `in/`, `temp/`, `fan/` and `pwm/` subdirectories of
that path. Values use standard hwmon units, so no conversion is needed:

| Attribute | Quantity | Unit |
|---|---|---|
| `inN_input` | voltage | millivolts (mV) |
| `tempN_input` | temperature | millidegrees Celsius (m°C) |
| `fanN_input` | fan speed | revolutions per minute (RPM) |
| `pwmN` | fan duty | percent, 0–100 (see Section 2.4) |
| `<attr>_label` | channel name | text |

**Channel numbers are the same in both layouts.** `pwm/pwm1` and `pwm1` name
one channel, `fan/fan1_input` and `fan1_input` name one channel, and so on —
pick whichever path your kernel has and the number means the same thing. The
numbering is the hwmon core's: temperatures, fans and PWM start at 1,
voltages start at 0.

Those numbers are **not** your board's configuration indexes, which always
start at 0. For temperatures, fans and PWM the file number is one higher:
configuration index 0 is `temp1_input`/`fan1_input`/`pwm1`, index 2 is
`temp3_input`. Voltages line up directly. Read the `_label` files rather
than counting.

The flat layout has no PWM label files, and cannot — the kernel's hwmon core
has no PWM label attribute. `pwm/pwmN_label` (subdirectory layout) is the
only place a PWM channel says what it drives.

```bash
cd /sys/class/hwmon/hwmon3
# Flat layout (kernel 5.4+). On kernel 4.15 these files do not exist here
# -- use the subdirectory block further down instead.

# List every channel with its label
for f in in*_label temp*_label fan*_label pwm*_label; do
    [ -e "$f" ] && printf "%-12s %s\n" "$f" "$(cat "$f")"
done

# Read one voltage rail (mV) and its label
cat in1_label   # e.g. VIN
cat in1_input   # e.g. 12090   -> 12.090 V

# Temperature (m°C -> divide by 1000 for °C)
cat temp1_input # e.g. 45000   -> 45.0 °C

# Fan speed (RPM)
cat fan1_input  # e.g. 3245
```

**On kernel 4.15**, the same listing has to walk the four subdirectories:

```bash
cd /sys/class/hwmon/hwmon3

for d in in temp fan pwm; do
    for f in "$d"/*_label; do
        [ -e "$f" ] && printf "%-20s %s\n" "$f" "$(cat "$f")"
    done
done

cat in/in1_input      # e.g. 12090   -> 12.090 V
cat temp/temp1_input  # e.g. 45000   -> 45.0 °C
cat fan/fan1_input    # e.g. 3245
```

Kernel 5.4 and newer carry both layouts, so this block works there too.

**Gaps in the channel numbering are normal, not a bug.** The number in a
file name follows the channel's index in your board's own configuration; it
is not a running count of what exists. A board that enables voltage indexes
1, 2 and 4, for example, exposes `in1_input`, `in2_input` and `in4_input` —
there is no `in0_input` and no `in3_input`. Most boards that have a hardware
monitor at all have at least one such gap. List the directory and read the
`_label` files to see what your specific board actually has, rather than
assuming the channel you want is the lowest number.

### 2.4 Controlling fan PWM

If your board exposes PWM channels, `pwmN` is writable. **Note:** unlike the
standard hwmon range of 0–255, this driver uses a **duty percentage,
0–100**.

```bash
# The pwm/ subdirectory is present on every kernel and carries the labels, so
# use it to identify PWM channels on any kernel version. Its numbers are the
# same ones the flat path uses:
cd /sys/class/hwmon/hwmon3/pwm

# Always look first -- which PWM channels does THIS board have, and what are they?
for f in *_label; do printf "%-12s %s\n" "$f" "$(cat "$f")"; done
# e.g. pwm1_label   Backlight
#      pwm2_label   CPU Fan PWM
#      pwm3_label   System Fan PWM

# Then read and set the channel you found. Substitute your own number.
cat pwm2                  # current duty, 0-100 (%)
echo 60 | sudo tee pwm2   # set it to 60% duty (root)

# ../pwm2 is the same channel and takes the same write -- use whichever path
# your kernel has. Only the labels are exclusive to pwm/.
echo 60 | sudo tee ../pwm2
```

> **⚠️ Read the label before you write.** Not every board's lowest-numbered
> PWM channel is a fan — on some boards the lowest channel present controls
> the **display backlight** instead, and writing a fan duty value to it will
> dim or brighten the screen instead of changing fan speed. Always read the
> `_label` file for the exact channel number you are about to write, on the
> board in front of you, before writing to it.

On kernel 5.4 and newer, the flat `/sys/class/hwmon/hwmon3/` path carries
the same channels one number higher than the `pwm/` subdirectory path (e.g.
index 1 becomes `pwm2`, index 3 becomes `pwm4`), and it has **no** PWM label
files at all — from the flat path alone there is no way to tell which
channel is the fan. The `pwm/` subdirectory path above reads the same on
every kernel version and always has the labels; prefer it when identifying a
channel.

### 2.5 Troubleshooting

* **No hwmon device, or nothing under `/name`.** Confirm the module is
  loaded (`lsmod | grep hwm`) and that your board actually has a hardware
  monitor configured. A board with no hardware monitor, or an incompletely
  configured one, builds no hwmon module at all.
* **`sensors` prints nothing for this driver, on kernel 4.15.** Expected —
  see the note at the top of this section. The device is still present
  (`cat /sys/class/hwmon/hwmon*/name` still shows your board's chip name);
  read the values from sysfs instead (Section 2.3). Kernel 5.4 and newer are
  not affected.
* **Readings look wrong** (off by a constant factor, an implausible fan
  RPM). The per-channel scaling is set in your board's own configuration.
  If a reading looks off by a constant factor, or a fan reads a nonsense
  RPM, that scaling may need correcting for your specific board revision —
  contact support with the exact reading and the expected value.
* **Which chip am I looking at?** Match by `cat /sys/class/hwmon/hwmon*/name`
  — other `hwmonX` nodes on the same system (CPU package, NVMe, etc.) are
  unrelated to this driver.
* **`sensors` shows raw `inN` names instead of labels.** Labels are exposed
  through the `inN_label`-style files; a very old `libsensors` may ignore
  them. Reading sysfs directly (Section 2.3) always shows the real labels.

---

## 3. Watchdog

The kernel driver registers a standard Linux watchdog device. Feeding
("kicking") it periodically prevents the board from resetting; letting it
expire without a kick resets the board. This section shows the recommended
way to drive it — the standard Linux **watchdog daemon**.

### 3.1 Prerequisites

```bash
# Verify the driver is loaded
lsmod | grep wdt

# Verify the device node exists
ls -l /dev/watchdog*
```

### 3.2 Installing and configuring the watchdog daemon

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install watchdog
```

**CentOS/RHEL:**

```bash
sudo yum install watchdog
```

Edit `/etc/watchdog.conf`:

```bash
sudo nano /etc/watchdog.conf
```

Uncomment and set these lines to match this driver's device node:

```ini
# The device node created by this driver
watchdog-device = /dev/watchdog   # or /dev/watchdog0

# Interval between heartbeats (seconds)
# Must be smaller than the hardware timeout (commonly 60s by default)
interval = 10
```

Enable and start the service:

```bash
sudo systemctl enable watchdog
sudo systemctl start watchdog
```

Check its status:

```bash
sudo systemctl status watchdog
```

### 3.3 Verifying the hardware watchdog actually resets the board

**⚠️ WARNING: the following steps will cause the board to reboot.**

To confirm the watchdog hardware genuinely resets the system when it stops
being fed (not just that the daemon runs):

**This test works the same under `nowayout=0` or `nowayout=1`.** `kill -9`
(step 2 below) is `SIGKILL`: the process cannot catch it or run any
shutdown code, so it never reaches the daemon's own magic-close write of
`V` (see [3.5](#35-checking-whether-the-watchdog-is-running-and-enabling-or-disabling-it)
below). Closing `/dev/watchdog0` without `V` does not stop the watchdog
under either setting, so once step 3 confirms nothing else holds
`/dev/watchdog0`, the board still resets. Stopping the daemons the
clean way instead, with `nowayout=0` loaded, will **not** reset the board --
see 3.5 for the command and why.

1. **Check the current timeout:**

   ```bash
   cat /sys/class/watchdog/watchdog0/timeleft
   ```

2. **Stop every process feeding the watchdog**, to simulate a system freeze:

   ```bash
   sudo killall -9 watchdog
   sudo killall -9 wd_keepalive
   ```

   **On Debian/Ubuntu, systemd starts `wd_keepalive` again moments after
   this.** `watchdog.service` is `Type=forking`, so killing its main
   process this way still runs `ExecStopPost=`, which fails on purpose
   while `run_wd_keepalive=1` and puts the unit in `failed`;
   `OnFailure=wd_keepalive.service` then starts the second daemon, which
   reopens `/dev/watchdog0` and feeds it
   ([`debian/watchdog.service`](https://sources.debian.org/src/watchdog/5.16-1/debian/watchdog.service/)).
   The two `killall` commands above run one right after the other, so
   this respawn usually lands only after both have already been typed.
   RHEL/CentOS ships no such handoff, so there this step is the whole
   story.

3. **Make sure nothing still holds `/dev/watchdog0` open:**

   ```bash
   sudo lsof /dev/watchdog0
   ```

   If something is still holding it, kill that process explicitly:

   ```bash
   sudo kill -9 <pid>   # e.g. sudo kill -9 234
   ```

   **On Debian/Ubuntu, this normally finds `wd_keepalive` holding the
   device -- that is the previous step's respawn, not a leftover.**
   Killing it here is a required part of the test, not a formality: skip
   it and `wd_keepalive` keeps feeding the watchdog, so the board never
   resets. Once killed it stays dead -- `debian/wd_keepalive.service`
   carries no `OnFailure=` and no `Restart=`
   ([`debian/wd_keepalive.service`](https://sources.debian.org/src/watchdog/5.16-1/debian/wd_keepalive.service/)),
   so there is no loop to repeat.

4. **Wait out the timeout** (commonly 60 seconds). The board should reset on
   its own once the timeout elapses with nobody feeding it.

### 3.4 Reading and setting the timeout

`/sys/class/watchdog/watchdog0/timeout` is **read-only**. Writing to it has
no effect — do not try to change the timeout this way.

The timeout is set through `/dev/watchdog0` instead. The `watchdog` daemon
does this for you from its own `watchdog-timeout` line in
`/etc/watchdog.conf`:

```ini
# Timeout in seconds
watchdog-timeout = 60
```

If you are writing your own program instead of using the daemon, issue the
`WDIOC_SETTIMEOUT` ioctl directly on an open `/dev/watchdog0` file
descriptor.

**The ceiling differs per board line.** An ITE EC board accepts up to 65,535
seconds; an F81966 or NCT61x6D board accepts up to 255 seconds. Asking for
more than your board's ceiling still succeeds — the driver does not fail the
request, it uses the nearest value the chip can actually hold.

Because of that clamping, always read the timeout back after setting it, to
learn whether your request was honoured as asked or rounded down: either
`WDIOC_GETTIMEOUT`, or the value `WDIOC_SETTIMEOUT` itself now writes back
into the same variable you passed it.

A `WDIOC_SETTIMEOUT` of `0` disables the watchdog timer; `1` second is the
shortest timeout that keeps it running. Do not send `0` unless you mean to
turn the watchdog off.

### 3.5 Checking whether the watchdog is running, and enabling or disabling it

`WDIOC_GETSTATUS` returns the kernel core's own `WDIOF_*` status flags:
keep-alive-ping (the watchdog has been fed since your last read) and
magic-close (you have written `V` to `/dev/watchdog0`). It never reports a
hardware fault bit -- overheat, fan fault, card reset -- because the core
reads those out of `bootstatus`, and this driver never sets it.

`WDIOC_GETSTATUS` also does not tell you whether the watchdog is currently
running. To check that, read `/sys/class/watchdog/watchdog0/state`
(`active` or `inactive`).

**The `nowayout` module parameter defaults to on.** A customer who does
nothing sees exactly today's behaviour: once the watchdog is running,
nothing in software can stop it -- not `WDIOC_SETOPTIONS` with
`WDIOS_DISABLECARD`, not closing `/dev/watchdog0` without the magic sequence
below.

* `WDIOC_SETOPTIONS` with `WDIOS_ENABLECARD` starts the watchdog (unaffected
  by `nowayout`).
* On the default (`nowayout=1`), `WDIOC_SETOPTIONS` with `WDIOS_DISABLECARD`
  returns `-EBUSY`. It never stops the watchdog.
* On the default, closing `/dev/watchdog0` -- including the `watchdog`
  daemon closing it on `sudo systemctl stop watchdog` -- does not stop it,
  and does not keep it alive either. The kernel prints `watchdog0: watchdog
  did not stop!` in `dmesg`, feeds the watchdog one last time, and then
  stops feeding it. **The board resets about one timeout period after you
  close the file.** Keep the file open and keep feeding it for as long as
  you want the system to stay up.
* **On Debian/Ubuntu, this reset may not happen even on the default.** The
  `watchdog` package ships with `run_wd_keepalive=1` in
  `/etc/default/watchdog` by default
  ([Debian source package `watchdog` 5.16-1, `debian/postinst`, line 53](https://sources.debian.org/src/watchdog/5.16-1/debian/postinst/#L53);
  the sysvinit path sets the same default in
  [`debian/init`, line 33](https://sources.debian.org/src/watchdog/5.16-1/debian/init/#L33)),
  which starts a second daemon, `wd_keepalive`, right after
  `watchdog.service` stops
  ([`debian/watchdog.service`](https://sources.debian.org/src/watchdog/5.16-1/debian/watchdog.service/)).
  `wd_keepalive` reopens `/dev/watchdog0` and resumes feeding it, so
  stopping `watchdog` alone does not reset the board -- something else is
  now feeding it. RHEL/CentOS ships no such handoff: upstream's own
  `redhat/` init script's `stop()` just sends `SIGTERM`, nothing else
  ([same package, `redhat/watchdog.init`](https://sources.debian.org/src/watchdog/5.16-1/redhat/watchdog.init/)).
* **`sudo systemctl stop watchdog wd_keepalive` does not reach that state
  on Debian/Ubuntu.** `wd_keepalive` is not running yet when you type that
  command -- it only starts *after* `watchdog.service` stops and enters the
  `failed` state, which is what `OnFailure=` above is waiting for. So
  naming it in the same command stops nothing, and it comes up moments
  later holding `/dev/watchdog0` again. Use the package's own switch
  instead:
  ```bash
  sudo sed -i 's/^run_wd_keepalive=.*/run_wd_keepalive=0/' /etc/default/watchdog
  sudo systemctl stop watchdog
  ```
  With `run_wd_keepalive=0`, `ExecStopPost=` succeeds instead of failing,
  `watchdog.service` never enters `failed`, and `wd_keepalive` never
  starts. Confirm both are stopped:
  ```bash
  systemctl is-active watchdog wd_keepalive
  ```
  Both should print `inactive`. If you already tried the broken command
  above first, `wd_keepalive` can still be `active` here: `stop watchdog` on
  an already-stopped service is a no-op that exits `0`, and
  `wd_keepalive`'s own unit only reacts to `watchdog.service` *starting*,
  not stopping
  ([`debian/wd_keepalive.service`, `Conflicts=`](https://sources.debian.org/src/watchdog/5.16-1/debian/wd_keepalive.service/)).
  Stop it directly: `sudo systemctl stop wd_keepalive`.

  Once both print `inactive`, nothing is feeding the watchdog anymore. On
  this default (`nowayout=1`), that is the point of doing this: the board
  resets about one timeout period later, the same reset described above.
  With `nowayout=0` loaded first (below), it does not reset -- the watchdog
  is genuinely off.

**Turning it off: `sudo modprobe wdt nowayout=0`.** To make the setting stick
across reboots, create `/etc/modprobe.d/avalue-wdt.conf` containing:

```ini
options wdt nowayout=0
```

`modinfo wdt` shows the `nowayout` parameter and its description, but it
reads the `.ko` file on disk, so it always reports the compiled-in
**default** (`true`) -- never the value a running module was actually
loaded with. To see what is really loaded, check `dmesg` after
`modprobe`:

```bash
dmesg | grep nowayout
```

The driver logs it once at load time:

```
nowayout=0: watchdog can be stopped by software once running
```

With `nowayout=0` loaded, two things really stop the watchdog:

* `WDIOC_SETOPTIONS` with `WDIOS_DISABLECARD` now reaches the chip and really
  stops it.
* The magic-close sequence: write the single byte `V` to `/dev/watchdog0`,
  then close it.
  ```bash
  printf 'V' > /dev/watchdog0
  ```
  Bash opens the file descriptor, writes the single byte `V` with no
  trailing newline, and closes it when the command completes -- that
  open-write-close *is* the magic-close sequence the kernel checks for.

  **`/dev/watchdog0` is single-open, so this only works when nothing else
  already has it open.** A second `open()` while the file is already open
  returns `-EBUSY`, so the command above fails with `Device or resource
  busy` if the `watchdog` service, or a customer's own daemon, is running
  and feeding the board -- the ordinary case. There, the magic close has to
  come from *that* process, on its own file descriptor: it writes `V` and
  then closes, the same way stopping the service (above) does rather than
  a shell command run against a file another process holds. The command
  above only does something useful on its own when nothing holds the file
  yet (a no-op round trip: open, start the watchdog, write `V`, close
  again) or when the watchdog is running with nobody holding the file
  (someone closed it earlier without writing `V`, and it is counting down
  to a reset) -- there it genuinely stops the countdown.

  **Confirmed:** the standard `watchdog` daemon's own shutdown path,
  `close_watchdog()` in `src/keep_alive.c`, always writes `V`
  before closing the device it holds open
  ([Debian source package `watchdog` 5.16-1](https://sources.debian.org/src/watchdog/5.16-1/src/keep_alive.c/),
  line 284). On RHEL/CentOS this really leaves the watchdog off after
  `sudo systemctl stop watchdog`. On Debian/Ubuntu, remember the
  `wd_keepalive` caveat above: it reopens the device and feeds it again
  unless you also turn off `run_wd_keepalive` first, the way the
  Debian/Ubuntu note above describes.

**What does not change with `nowayout=0`:** closing `/dev/watchdog0` *without*
first writing `V` still does not stop the watchdog, and still does not keep
it alive either -- the kernel still prints `watchdog0: watchdog did not
stop!`, feeds it one last time, then stops feeding it, and the board still
resets about one timeout period later. Only a deliberate `V`-then-close
actually stops it.

Once you start the watchdog, the only way to keep the board alive without a
deliberate stop is to keep feeding it forever; otherwise it resets.

---

## 4. Misc (board EC registers)

Some boards expose a handful of board-specific embedded-controller (EC)
registers — things like a COM port's operating mode, or the EC firmware
version — through a small `misc` driver: a single ioctl character device,
`/dev/misc`, plus one plain sysfs file per register for read/write access
with no C code required.

### 4.1 Prerequisites

* **Driver loaded:** the misc kernel module (e.g. `misc.ko`) must be loaded.
  Check with `lsmod | grep misc`.
* **Device node:** a character device node `/dev/misc` must exist.
* **Not every board has this device.** Only a board whose configuration
  enables it builds this module and creates `/dev/misc`; a board that does
  not enable it has neither the module nor the sysfs files below. If
  `/dev/misc` does not exist on your board, your board simply does not
  expose any registers through this interface.
* **Permissions:** the `/dev/misc` device node itself is created world
  read/write (mode `0666`), but the individual per-channel sysfs files it
  exposes are more restrictive — see Section 4.3 and Section 4.4.

### 4.2 What this driver exposes

**The device node, `/dev/misc`.** Its file operations only wire up `open`,
`release` and `ioctl` — there is **no** `read`/`write` on `/dev/misc`
itself. In other words, `/dev/misc` exists purely so you can `open()` it and
issue `ioctl()` calls (Section 4.5); it is not a file you `cat` or `echo`
into directly.

**The sysfs attribute files.** For everyday use you usually do not need
`ioctl()` at all — every channel your board's configuration enables also
gets a plain sysfs attribute file, on the same device:

```
/sys/class/misc/misc/<LABEL>
```

`<LABEL>` is that channel's own name from your board's configuration (e.g.
`COM1_Mode`). The file is created mode `0644`, and reading or writing it
drives the same code path the `ioctl()` interface uses — `cat` triggers a
driver read, `echo N >` triggers a driver write.

### 4.3 Reading and writing channels via sysfs

**Reading a channel:**

```bash
cat /sys/class/misc/misc/COM1_Mode
```

This prints the current register value as a decimal number.

**Writing a channel:**

```bash
echo 1 | sudo tee /sys/class/misc/misc/COM1_Mode
```

The driver parses the string as an unsigned 32-bit integer (any base) and
writes it to the hardware register.

### 4.4 Read-only channels — the write guarantee

> **⚠️ Mode `0644` does not mean every channel is writable.** A channel with
> no write command defined for it — the driver checks this at runtime, not
> just by convention — has no write handler on the hardware side at all. For
> example, a channel that only exposes something like an EC firmware-version
> register is read-only in practice, even though its sysfs file's own
> permission bits still read `0644`.

Writing such a channel fails plainly, over **both** paths this driver
offers, and touches no EC register either way:

* **Over sysfs**, a write to a read-only channel returns `-EACCES`:

  ```bash
  $ echo 1 | sudo tee /sys/class/misc/misc/EC_FW_Version
  tee: /sys/class/misc/misc/EC_FW_Version: Permission denied
  ```

* **Over `ioctl()`** (Section 4.5), the same channel's write command — or
  any command number no channel defines at all, including command `0`,
  which every board reserves as "no command here" — is rejected with
  `-ENOTTY` instead of `-EACCES`. The ioctl path does not distinguish
  "a command that exists but is read-only" from "a command that was never
  defined" — from that path's point of view they look the same, so both are
  answered `-ENOTTY`, and neither one ever reaches the EC.

Do not treat a file's `0644` mode, or the mere existence of an ioctl command
number you constructed, as a promise that a write will succeed — on a
read-only channel, on either path, it will not, and no hardware register is
touched when it is refused.

### 4.5 Reading a channel via `ioctl()`

If you are writing your own C program instead of shelling out to sysfs, you
talk to `/dev/misc` directly with `ioctl()`.

Each channel's ioctl numbers come from your board's own configuration, built
as:

```
<CHANNEL>_IOR = _IOR(MISC_IOCTL_BASE, <CHANNEL>_REG, u32)
<CHANNEL>_IOW = _IOW(MISC_IOCTL_BASE, <CHANNEL>_REG, u32)
```

`MISC_IOCTL_BASE` is `'A'` on every board that has this device, and
`<CHANNEL>_REG` is that channel's hardware register (e.g. `0x20` for
`COM1_Mode`). Build these two macros yourself from your board's own channel
register numbers before writing an ioctl call — do not guess a magic number.

A channel with no write command at all (Section 4.4) has nothing to build
for `_IOW`. Sending command `0` itself is always rejected with `-ENOTTY`, on
every board, regardless of what channels it has.

**Worked example — reading a `COM1_Mode`-style channel:**

```c
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define COM1_MODE_IOCTL_BASE 'A'
#define COM1_MODE_REG 0x20
#define COM1_MODE_IOR _IOR(COM1_MODE_IOCTL_BASE, COM1_MODE_REG, uint32_t)

int main(void)
{
	int fd = open("/dev/misc", O_RDONLY);
	uint32_t value = 0;

	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (ioctl(fd, COM1_MODE_IOR, &value) < 0) {
		perror("ioctl");
		close(fd);
		return 1;
	}

	printf("COM1 mode register = 0x%02x\n", value);
	close(fd);
	return 0;
}
```

---

## 5. Building and installing driver 4.0

Driver 4.0 builds from source with a plain `make`, driven entirely by your
board's own configuration file — there is no `DRIVER=` argument, and no
per-feature source tarball.

### 5.1 Prerequisites

**Debian/Ubuntu:**

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

**RHEL/CentOS/Fedora:**

```bash
sudo yum install gcc kernel-devel-$(uname -r) make
```

`kernel-devel` (or `linux-headers` on Debian/Ubuntu) must match the kernel
you are **running**, not just installed — check with `uname -r` first. A
mismatch here is the most common first build failure, and it looks like a
driver problem when it is not one.

### 5.2 The `make` interface

| Variable / target | Meaning |
|---|---|
| `BOARD_NAME=<name>` | Which board's configuration to build for. Auto-detected from the board's own DMI table on the target machine, so you normally omit it — pass it only to override, e.g. when cross-building for a different board off-target. |
| `KERNEL_SOURCE=<path>` | Which kernel build tree to compile against. Defaults to the running kernel's own headers; override it to build against a different prepared kernel tree, e.g. when cross-building. |
| `make` (no target) | Build every subsystem your board's configuration enables, in one run. |
| `make watchdog` / `make gpio` / `make hwmon` / `make misc` | Build just one subsystem. Asking for one your board does not have stops with a clear error, rather than a confusing build failure. |
| `make <subsystem>-debug` | Build one subsystem with verbose debug logging enabled, e.g. `make gpio-debug`. |
| `make clean` | Remove build artifacts and the generated board configuration header. |
| `make help` | Print this same option list from the build tree itself. |

Building natively on the target board (the common case):

```bash
make
```

Building for a specific board, or off-target:

```bash
make BOARD_NAME=ESM-KX60G
```

Building against a specific prepared kernel tree:

```bash
make BOARD_NAME=ESM-KX60G KERNEL_SOURCE=/path/to/kernel/source/tree
```

Building just one subsystem:

```bash
make BOARD_NAME=ESM-KX60G KERNEL_SOURCE=/path/to/kernel/source/tree watchdog
```

Cleaning build artifacts between builds:

```bash
make BOARD_NAME=ESM-KX60G KERNEL_SOURCE=/path/to/kernel/source/tree clean
```

**A board only builds the subsystems its configuration declares.** Asking
`make` for a subsystem your board does not have (e.g. `make misc` on a board
with no misc registers) stops immediately with a clear message naming the
missing subsystem, rather than failing deep inside the kernel build with an
opaque, hard-to-diagnose compiler error.

### 5.3 Install and uninstall

```bash
sudo make install
```

This copies the built module(s) into your kernel's module tree under a
board-specific directory, registers them to auto-load at boot
(`/etc/modules-load.d/`), updates the module dependency map (`depmod`), and
loads them immediately. If you only built specific subsystems (e.g.
`make watchdog`), `install` only installs what you actually built.

```bash
sudo make uninstall
```

This unloads the currently loaded module(s), removes the installed files
from your kernel's module tree, removes the auto-load configuration entry,
and updates the module dependency map.

---

*This page covers driver 4.0. If a command here does not match what your
board's driver actually does, first confirm you are running 4.0 and not an
older driver still installed on the system, then contact
support with the exact command, its output and your board name.*
