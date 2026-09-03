# Board Configuration Guide

This directory holds the **per-board configuration files** that drive the whole
build. Each file in [`boards/`](boards/) fully describes the onboard features of
one Avalue SBC / embedded system (Watchdog, Hardware Monitor, GPIO, and MISC),
and the build system turns it into a C header that is compiled into the kernel
modules.

> **Audience:** This document is written so that a human *or an AI agent* can take
> a request like *"add support for a brand-new board `XYZ-123`"* and produce a
> correct `.conf` file plus the tests needed to validate it — without reading the
> driver source first.

---

## 1. How the configuration pipeline works

A board is described **once**, in a plain `KEY=VALUE` text file. Nothing about the
board is hard-coded in the driver `.c` files.

```
configs/boards/<BOARD_NAME>.conf     <-- you write this
            │
            │  make  (reads /sys/class/dmi/id/board_name, or BOARD_NAME=xxx)
            ▼
scripts/config.sh <conf> src/configs/board.h
            │
            │  generates #define lines
            ▼
src/configs/board.h                  <-- auto-generated, DO NOT EDIT
            │
            │  -include board.h  (see Kbuild)
            ▼
wdt.ko / gpio.ko / hwm.ko / misc.ko  <-- the compiled drivers
```

Two independent things are read from the same `.conf` file:

| Prefix        | Consumed by         | Purpose                                                                 |
|---------------|---------------------|-------------------------------------------------------------------------|
| `MAKE_*`      | `Makefile` / `Kbuild` | Selects **which** HAL source files get compiled and linked (build-time wiring). |
| `CONFIG_*`    | `scripts/config.sh` → `board.h` | Becomes `#define`s the driver C code reads (registers, labels, pins, math). |

`config.sh` deliberately **skips every `MAKE_*` line** — those never reach the C
code. Everything else becomes a `#define`.

### Board name / file name

The build picks the board in this order (see `Makefile`):

1. `make BOARD_NAME=XYZ-123` (explicit override), otherwise
2. auto-detect from `/sys/class/dmi/id/board_name`.

The detected DMI string is sanitized with `sed 's/[^a-zA-Z0-9_-].*//'` (keeps
letters, digits, `-`, `_`; truncates at the first other character).

**The `.conf` file name MUST equal the resulting board name**, e.g. board
`ECM-ASL` → `configs/boards/ECM-ASL.conf`, and the file must set
`CONFIG_BOARD_NAME="ECM-ASL"` to match. If the file is missing, the build stops
with `Config file '...' not found!`.

---

## 2. The four subsystems

Every board config is organised into up to four feature blocks. A block is built
when both its `MAKE_*_DEVICE` and `MAKE_*_CHIPSET` lines are present and
non-empty. The pair becomes the source path `src/hal/<DEVICE>/<CHIPSET>.o`, so a
name that matches no file under `src/hal/` fails during the build, not while the
config is read. Omit a block entirely to disable that feature for the board.

Each subsystem is implemented by a **device** (the access method / bus) plus a
**chipset** (the specific controller). The build composes source paths as
`src/hal/<DEVICE>/<CHIPSET>.o` and `src/hal/<DEVICE>/<CHIPSET>_<subsystem>.o`
(see `Kbuild`), so `MAKE_*_DEVICE` / `MAKE_*_CHIPSET` must name files that exist.

### Supported HAL matrix

Only these combinations currently have HAL source. Do **not** invent new
`device`/`chipset` names in a `.conf` — a new combination requires new C files
under `src/hal/` first.

| Subsystem        | `MAKE_*_DEVICE` | `MAKE_*_CHIPSET` | Extra              | HAL source                                   |
|------------------|-----------------|------------------|--------------------|----------------------------------------------|
| Watchdog (WDT)   | `ec`            | `ite`            | board-wide `CONFIG_CHIPID` (e.g. `0x5782`) | `src/hal/ec/ite.c`, `ite_wdt.c`        |
| Watchdog (WDT)   | `sio`           | `f81966`         | board-wide `CONFIG_CHIPID` (e.g. `0x1502`) | `src/hal/sio/f81966.c`, `f81966_wdt.c` |
| Watchdog (WDT)   | `sio`           | `nct61x6d`       | board-wide `CONFIG_CHIPID` (e.g. `0xD282`) | `src/hal/sio/nct61x6d.c`, `nct61x6d_wdt.c` |
| Hardware Monitor | `ec`            | `ite`            | board-wide `CONFIG_CHIPID` | `src/hal/ec/ite.c`, `ite_hwm.c`              |
| Hardware Monitor | `sio`           | `f81966`         | board-wide `CONFIG_CHIPID` | `src/hal/sio/f81966.c`, `f81966_hwm.c`       |
| Hardware Monitor | `sio`           | `nct61x6d`       | board-wide `CONFIG_CHIPID` | `src/hal/sio/nct61x6d.c`, `nct61x6d_hwm.c`   |
| GPIO             | `smb`           | `nct5655`        | `MAKE_GPIO_PROTOCOL=i801` | `src/hal/smb/nct5655.c`, `i801.c`, `nct5655_gpio.c` |
| GPIO             | `smb`           | `pca9555`        | `MAKE_GPIO_PROTOCOL=i801` | `src/hal/smb/pca9555.c`, `i801.c`, `pca9555_gpio.c` |
| GPIO             | `smb`           | `pca9555`        | `MAKE_GPIO_PROTOCOL=zhaoxin` | `src/hal/smb/pca9555.c`, `zhaoxin.c`, `pca9555_gpio.c` |
| GPIO             | `ec`            | `ite`            | board-wide `CONFIG_CHIPID`; `CONFIG_GPIO_REG_*` optional | `src/hal/ec/ite.c`, `ite_gpio.c` |
| MISC             | `ec`            | `ite`            | board-wide `CONFIG_CHIPID` | `src/hal/ec/ite.c`, `ite_misc.c`             |

> **`CONFIG_CHIPID` is one key per board, not one per subsystem.** A board
> names the chip it carries a single time — in whichever subsystem block
> comes first in its `.conf` — and every subsystem that board builds over the
> `ite` / `f81966` / `nct61x6d` chip HALs reads that same key. `scripts/config.sh`
> requires it for any board that builds a subsystem over one of these three
> chip HALs, and each HAL's own C source carries a build-time check that
> rejects a declared value that chip can never be.

> **Tip for a new board:** the fastest correct starting point is to copy the
> `.conf` of an existing board that uses the **same super-I/O / EC chipset**, then
> adjust labels, enabled channels, and resistor/scaling values. There are three
> reference families: ITE-EC boards (`ECM-ASL`, `EMS-ARH`, `HID-2340`,
> `NUC-RPU`), Nuvoton super-I/O boards (`EAX-Q170KP` and others), and the
> Fintek super-I/O board (`MX610H`). Copying across families is the common way
> to get a board wrong, and the copy looks right because the families overlap:
> `MAKE_HWM_DEVICE` is `sio` for both super-I/O families, and all three families
> declare 3 temperatures, so a matching device or temperature count proves
> nothing. `CONFIG_CHIPID` does differ in all three (`0x5782` ITE, `0xD282`
> Nuvoton, `0x1502` Fintek). After copying across families, re-check the chip ID
> and all four channel counts against the table in
> [§2.2](#22-hardware-monitor-hwm).

### 2.1 Watchdog

```ini
# ec | sio ; omit the whole block to disable WDT
MAKE_WDT_DEVICE=ec
# ite (ec) | f81966 (sio) | nct61x6d (sio)
MAKE_WDT_CHIPSET=ite
# mirror of MAKE_WDT_CHIPSET, seen by the C code -- GPIO and MISC keep no
# such mirror any more; only this WDT copy is ever read, by
# src/drivers/watchdog.c for the /sys/class/watchdog/*/identity string
CONFIG_WDT_CHIPSET=ite
# chip identification register value -- one CONFIG_CHIPID per board, not
# one per subsystem; every subsystem this board builds over a chip HAL
# reads this same key
CONFIG_CHIPID=0x5782
```

> **Comments must be on their own line** — see §3. A trailing `# ...` after a
> value becomes *part of the value*, so `CONFIG_CHIPID=0x5782 # note` would
> emit `#define CONFIG_CHIPID "0x5782 # note"` (a broken string, not a
> number). Every example below follows this rule.

### 2.2 Hardware Monitor (HWM)

HWM has four channel groups: **Voltage**, **Temperature**, **Fan**, **PWM**. Each
group uses the same pattern:

* A fixed `CONFIG_HWM_<GROUP>_NUM` (the number of *physical* channels the chip
  exposes — this is chip-defined, **not** the number you enable). Every board of
  a given chipset declares the same four counts, so copy the row for your chip
  rather than guessing:

  | `CONFIG_HWM_CHIPSET` | Voltage | Temperature | Fan | PWM |
  |----------------------|---------|-------------|-----|-----|
  | `ite`                | 5       | 3           | 2   | 6   |
  | `nct61x6d`           | 8       | 3           | 3   | 2   |
  | `f81966`             | 8       | 3           | 2   | 2   |

  `nct61x6d` is the only family with a third fan. Set `CONFIG_HWM_FAN_NUM=3`
  there and enable channel 2, or `fan2_input` never appears. The `f81966` row is
  taken from the one board that uses that chip (`MX610H`); treat its PWM count
  as that board's value until a second F81966 board confirms it. These counts
  are enforced at build time by `scripts/config.sh`, which requires a board's
  own `CONFIG_HWM_<GROUP>_NUM` to match its HAL's real slot count -- see this
  repo's `CLAUDE.md` (`` `_NUM`/`_MAP` keys are mandatory ``). This table is
  only a reader's copy for convenience, so a stale cell here is a reading
  error, not a build error, and it deliberately carries no automated guard
.
* Per-channel `CONFIG_HWM_<GROUP>_<n>_ENABLE=0|1`.
* A `CONFIG_HWM_<GROUP>_MAP={ ..._0_ENABLE, ..._1_ENABLE, ... }` array that lists
  **every** channel's enable macro, in order, up to `NUM`.

For each **enabled** channel, provide its detail keys. Disabled channels only
need the `_ENABLE=0` line.

**Voltage channel keys**

```ini
CONFIG_HWM_VOLTAGE_1_ENABLE=1
# register macro (defined in HAL C)
CONFIG_HWM_VOLTAGE_1_REG=ITE_REG_HWM_VOLTAGE(1)
# human-readable sensor name
CONFIG_HWM_VOLTAGE_1_LABEL="VIN"
# step in mV per count
CONFIG_HWM_VOLTAGE_1_LSB=13
# upper voltage-divider resistor (ohm)
CONFIG_HWM_VOLTAGE_1_R1=200000
# lower voltage-divider resistor (ohm)
CONFIG_HWM_VOLTAGE_1_R2=20000
# R1=R2=0 means the rail is measured directly (no divider).
# Reported value = raw * LSB * (R1 + R2) / R2
```

**Temperature channel keys**

```ini
CONFIG_HWM_TEMPERATURE_0_ENABLE=1
CONFIG_HWM_TEMPERATURE_0_REG=ITE_REG_HWM_TEMPERATURE(0)
CONFIG_HWM_TEMPERATURE_0_LABEL="CPU Temp"
```

**Fan channel keys**

```ini
CONFIG_HWM_FAN_0_ENABLE=1
CONFIG_HWM_FAN_0_REG_SPEED=ITE_REG_HWM_FAN_SPEED(2)
CONFIG_HWM_FAN_0_LABEL="CPU Fan"
# divisor/step used to convert raw -> RPM
CONFIG_HWM_FAN_0_SPEED_STEP=1
```

**PWM channel keys**

```ini
CONFIG_HWM_PWM_0_ENABLE=1
CONFIG_HWM_PWM_0_REG=ITE_REG_HWM_PWM(0)
CONFIG_HWM_PWM_0_LABEL="CPU Fan PWM"
```

> **Register macros** (`ITE_REG_HWM_VOLTAGE(n)`, `F81966_HWM_REG_VOL(x)`, …) are
> **not** defined in the `.conf`. The `#define ...` lines you see near the top of
> each group are *comments* (they begin with `#`, so `config.sh` skips them) that
> document the macro that already exists in the HAL C source
> (`ite_hwm.c` / `f81966_hwm.c`). Only reference a macro that is actually defined
> there; otherwise the build fails to compile `board.h`. `nct61x6d` defines no
> such macro at all — its HAL (`nct61x6d.h`, `nct61x6d_hwm.c`) has no per-channel
> register accessor, so every `nct61x6d` board's `CONFIG_HWM_<GROUP>_<n>_REG` is a
> raw literal instead (e.g. `CONFIG_HWM_VOLTAGE_0_REG=0x00`).

### 2.3 GPIO

```ini
MAKE_GPIO_DEVICE=smb
# SMBus host controller protocol
MAKE_GPIO_PROTOCOL=i801
MAKE_GPIO_CHIPSET=nct5655

# 0 = auto-detect the SMBus base I/O port
CONFIG_SMBUS_BASE_PORT=0x0000
# 7-bit SMBus slave address of the GPIO expander
CONFIG_SMBUS_SLAVE_ADDR1=0x40
CONFIG_SMBUS_SLAVE_ADDR2=0x40

# One offset per exposed GPIO line, then a NUM and a MAP:
CONFIG_GPIO_PIN0_OFFSET=0
CONFIG_GPIO_PIN1_OFFSET=1
...
CONFIG_GPIO_PIN7_OFFSET=11
CONFIG_GPIO_PIN_NUM=8
CONFIG_GPIO_PIN_MAP={ CONFIG_GPIO_PIN0_OFFSET, ... , CONFIG_GPIO_PIN7_OFFSET }
```

`CONFIG_GPIO_PIN_NUM` is the number of lines the driver exposes as
`/dev/gpiochipX`. The `_OFFSET` values are the hardware register bit positions
each logical line maps to (they need not be contiguous — note the `0,1,2,3,
8,9,10,11` pattern used by the NCT5655 boards).

### 2.4 MISC

MISC exposes ad-hoc register bit-fields (e.g. UART RS-232/422/485 mode) through
an ioctl character device.

```ini
MAKE_MISC_DEVICE=ec
MAKE_MISC_CHIPSET=ite
# board-wide chip id -- already set once in the WDT/HWM block above if this
# board builds those too; shown here in case MISC is the only ite subsystem
CONFIG_CHIPID=0x5782

# ioctl magic (single-quoted char literal)
CONFIG_MISC_IOCTL_BASE='A'

CONFIG_MISC_0_ENABLE=1
# target register
CONFIG_MISC_0_REG=0x20
# bit-field mask within the register
CONFIG_MISC_0_MASK=0x03
CONFIG_MISC_0_IOR=_IOR(CONFIG_MISC_IOCTL_BASE, CONFIG_MISC_0_REG, u32)
CONFIG_MISC_0_IOW=_IOW(CONFIG_MISC_IOCTL_BASE, CONFIG_MISC_0_REG, u32)
CONFIG_MISC_0_LABEL="COM1_Mode"

CONFIG_MISC_NUM=10
CONFIG_MISC_MAP= { CONFIG_MISC_0_ENABLE, ... , CONFIG_MISC_9_ENABLE }
```

---

## 3. Value formatting rules (what `config.sh` emits)

`config.sh` decides how to print each `#define` based on the value shape. Get the
shape right or the generated header won't compile. In order of precedence:

| Value looks like…                    | Example                          | Emitted as                         |
|--------------------------------------|----------------------------------|------------------------------------|
| Single-quoted char                   | `'A'`                            | `#define K 'A'`                    |
| Already double-quoted string         | `"VIN"`                          | `#define K "VIN"`                  |
| Hex number                           | `0x5782`                         | `#define K 0x5782`                 |
| Integer                              | `13`                             | `#define K 13`                     |
| `MACRO(...)` call (UPPER/underscore) | `ITE_REG_HWM_VOLTAGE(1)`         | `#define K ITE_REG_HWM_VOLTAGE(1)` |
| Array `{ ... }`                      | `{ A, B }`                       | `#define K { A, B }`               |
| Key ends in `_MAP` or `_CONFIG`      | `CONFIG_..._MAP=...`             | emitted **unquoted**               |
| anything else                        | `ite`                            | wrapped in quotes → `#define K "ite"` |

Consequences to keep in mind:

* **Always quote text labels** (`"CPU Temp"`), otherwise a multi-word label
  becomes a broken string and stray defines.
* **Lines are split on the first `=`.** Don't put a bare `=` inside a value.
* **No trailing inline comments.** `config.sh` takes everything after the first
  `=` as the value, so `CONFIG_HWM_VOLTAGE_1_LSB=13 # step` becomes the string
  `"13 # step"` instead of the integer `13`. Put every comment on its **own
  line** (a line starting with `#`). This is why the real board `.conf` files
  never annotate a value line inline.
* A line whose key isn't `_MAP`/`_CONFIG` and whose value is a bare word gets
  auto-quoted — that's why `CONFIG_WDT_CHIPSET=ite` safely becomes `"ite"`.
* Full-line comments (`# ...`) and blank lines are ignored, so annotate freely
  **on their own lines**.

---

## 4. Agent workflow — adding a brand-new board

When the user asks for a new target SBC, follow these steps:

1. **Confirm the board identity.** Ask for / determine the exact
   `/sys/class/dmi/id/board_name` string. The `.conf` file name and
   `CONFIG_BOARD_NAME` must match it.
2. **Confirm the hardware inventory.** Which subsystems exist and on which chip?
   * Super-I/O (Fintek F81966 or Nuvoton NCT61x6D) → `sio` device.
   * Embedded Controller (e.g. ITE) → `ec` device.
   * SMBus GPIO expander (e.g. NCT5655 via i801) → `smb` device.
   Gather per-channel details: which voltage rails/temps/fans/PWM are wired, their
   labels, register offsets, divider resistors, and LSB/step; GPIO line count and
   bit offsets; MISC register/mask for each configurable function.
3. **Pick the closest existing `.conf` as a template** from the same chip family
   (see the matrix in §2) and copy it to `configs/boards/<BOARD_NAME>.conf`.
4. **Edit the values.** Update `CONFIG_BOARD_NAME`, chip IDs, enabled channels,
   labels, registers, resistors, GPIO offsets, MISC entries. Keep every `_NUM`
   fixed to the chip's real channel count and keep each `_MAP` listing all
   channels in order. Remove whole subsystem blocks the board doesn't have.
5. **Verify only referenced register macros exist** in the matching HAL C file
   (`grep` for the macro name in `src/hal/<device>/<chipset>_*.c`). If a needed
   macro/HAL is missing, that's a driver-code change, not a config change — flag
   it.
6. **Generate and build-test** (see §5). Fix any `config.sh`/compiler errors.
7. **Run the functional tests** on the target hardware (see §6).

---

## 5. Testing the config: generation + build

These checks need no target hardware and should always be run after writing a
`.conf`.

**a) Header generation** — confirm `config.sh` produces a clean header:

```bash
scripts/config.sh configs/boards/<BOARD_NAME>.conf /tmp/board.h
cat /tmp/board.h        # eyeball: labels quoted, macros unquoted, no "MAKE_*"
```

Every enabled channel should appear as a sensible `#define`. Watch for two common
mistakes:

* an unquoted multi-word value, e.g. `#define CONFIG_..._LABEL CPU Temp` — a
  label that was not wrapped in `"..."`;
* a **quoted number**, e.g. `#define CONFIG_CHIPID "0x5782 # note"` — caused
  by a trailing inline comment on the value line (see §3). Chip IDs, registers,
  and resistors must come out as bare numbers.

A quick sanity grep:

```bash
grep -E 'CHIPID|_R1|_R2|_LSB|_NUM' /tmp/board.h   # these must all be bare numbers, never quoted
```

**b) Compile** — build all modules for the board (requires kernel headers, see the
top-level [`README.md`](../README.md)):

```bash
make BOARD_NAME=<BOARD_NAME>            # or run natively on the board with plain `make`
make BOARD_NAME=<BOARD_NAME> watchdog  # build a single subsystem
make BOARD_NAME=<BOARD_NAME> clean     # remove board.h and objects between tries
```

A successful build emits `wdt.ko` / `gpio.ko` / `hwm.ko` / `misc.ko` for exactly
the subsystems whose block sets both `MAKE_*_DEVICE` and `MAKE_*_CHIPSET`. A
block that names a device but leaves the chipset empty is skipped with a warning
naming the board and the key. `Kbuild` compiles with `-Wall -Wextra -Werror`, so
a malformed `board.h` fails loudly.

---

## 6. Testing the hardware

After `sudo make BOARD_NAME=<BOARD_NAME> install` loads the modules:

**GPIO** — an automated loopback test lives at [`../test/test-gpio.sh`](../test/test-gpio.sh).
It finds the chip by its `[gpio]` label, drives the first half of the lines and
reads them back on the second half (and vice-versa), so it requires the pins to
be **physically shorted in pairs** (`pin0↔pin4`, `pin1↔pin5`, … for an 8-line
part; `pin0↔pin8`, … for 16 lines):

```bash
sudo ./test/test-gpio.sh
```

Manual GPIO checks with `libgpiod` (`gpiodetect`, `gpioinfo`, `gpioget`,
`gpioset`) are documented in [`../GPIO.md`](../docs/GPIO.md).

**Watchdog** — see [`../WDT.md`](../docs/WDT.md) for how to arm/refresh and confirm the
reboot behaviour.

**Hardware Monitor** — once `hwm.ko` is loaded, the sensors appear under
the board's HWM chip name (`ite`, `nct61x6d` or `f81966` — e.g. `sensors`
from `lm-sensors`, or `/sys/class/hwmon/hwmonX/`).
Check that each enabled channel's label matches the `.conf` and that readings are
physically plausible (correct rails, temperatures, fan RPM). Wrong `R1/R2` or
`LSB` shows up as a voltage that is off by a constant factor.

**MISC** — exercise each enabled ioctl (e.g. switch a COM port between
RS-232/422/485) and confirm the register read-back matches.

---

## 7. Quick reference — minimal skeleton

A minimal single-subsystem (GPIO-only) board file:

```ini
# ================================
# XYZ-123 Board Configuration File
# ================================
CONFIG_BOARD_NAME="XYZ-123"

# ==============================
# ===== GPIO Configuration =====
# ==============================
MAKE_GPIO_DEVICE=smb
MAKE_GPIO_PROTOCOL=i801
MAKE_GPIO_CHIPSET=nct5655

CONFIG_SMBUS_BASE_PORT=0x0000
CONFIG_SMBUS_SLAVE_ADDR1=0x40
CONFIG_SMBUS_SLAVE_ADDR2=0x40

CONFIG_GPIO_PIN0_OFFSET=0
CONFIG_GPIO_PIN1_OFFSET=1
CONFIG_GPIO_PIN2_OFFSET=2
CONFIG_GPIO_PIN3_OFFSET=3
CONFIG_GPIO_PIN4_OFFSET=8
CONFIG_GPIO_PIN5_OFFSET=9
CONFIG_GPIO_PIN6_OFFSET=10
CONFIG_GPIO_PIN7_OFFSET=11
CONFIG_GPIO_PIN_NUM=8
CONFIG_GPIO_PIN_MAP={ CONFIG_GPIO_PIN0_OFFSET, CONFIG_GPIO_PIN1_OFFSET, CONFIG_GPIO_PIN2_OFFSET, CONFIG_GPIO_PIN3_OFFSET, CONFIG_GPIO_PIN4_OFFSET, CONFIG_GPIO_PIN5_OFFSET, CONFIG_GPIO_PIN6_OFFSET, CONFIG_GPIO_PIN7_OFFSET }
```

For a full multi-subsystem example, read
[`boards/ECM-ASL.conf`](boards/ECM-ASL.conf) (ITE-EC: WDT + HWM + GPIO + MISC),
[`boards/EAX-Q170KP.conf`](boards/EAX-Q170KP.conf) (Nuvoton super-I/O: WDT + HWM
+ GPIO, three fans) or
[`boards/MX610H.conf`](boards/MX610H.conf) (Fintek super-I/O: rich HWM).

---

*Copyright © Avalue Technology Co., Ltd. All Rights Reserved.*
