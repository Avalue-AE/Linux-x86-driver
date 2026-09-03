# Avalue Linux Driver Suite

**Version:** 4.0.0

**Manufacturer:** Avalue Technology Co., Ltd.

## Overview

This repository contains the Linux kernel drivers for Avalue industrial motherboards and embedded systems.

It provides onboard features including:

* [Watch dog](docs/WDT.md)
* [GPIO](docs/GPIO.md)
* [Hardware Monitor](docs/HWM.md)
* [Misc](docs/MISC.md)

---

## Prerequisites

Before building the drivers, ensure your system has the necessary build tools and kernel headers installed.

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

### RHEL/CentOS/Fedora

```bash
sudo yum install make gcc kernel-devel-$(uname -r)
```

`kernel-devel` (or `linux-headers` on Debian/Ubuntu) must match the kernel you
are **running**, not just installed -- check with `uname -r` first. A mismatch
here is the most common first failure, and it looks like a driver problem when
it is not one.

---

## Supported kernels

The oldest kernel driver 4.0 is known to build on is **4.15** (one subsystem
at a time -- see below); the oldest on which a plain `make` builds everything
in one run is **5.4.302**. The newest is **6.14.0**. A kernel version number
is not always what it looks like: enterprise
distributions (RHEL, AlmaLinux, Rocky, SLES) backport years of newer kernel code
under an older-looking version string, so a matching number is not by itself a
guarantee.

Kernel versions older than 4.15 are unsupported. On a working 4.15 tree the
subsystems build, one at a time (`make watchdog`, `make gpio`, `make hwmon`,
`make misc`) -- the table below is that measurement, not a claim. A plain
`make`, which builds every enabled subsystem in one run, fails on 4.15 for a
reason in that kernel's own build system. On 4.15, build one subsystem at a
time.

On 4.15 the hwmon module registers through plain sysfs attribute groups,
because 4.15's headers do not carry the newer chip_info API. Its sensor files
appear in `in/`, `temp/`, `fan/` and `pwm/` subdirectories under
`/sys/class/hwmon/hwmonN/`, not as the flat `inN_input` files that kernel 5.4
and newer get, so a tool expecting the standard layout (`sensors`) will not
find them there. This path was built and linked against a real 4.15 tree with
its own `Module.symvers` (the `4.15.0-101` rows below), and modpost resolved
every symbol the module imports -- but it has not been run on 4.15 hardware.

One row per kernel line we have actually built against, except the two
`4.15.0-101` rows: that tree is graded per board, because the two boards
declare a different set of subsystems and the grading counts them (see the
Note column). A line not in this table is simply untested, not unsupported.

| Distribution | Kernel version | Build status | Note |
|---|---|---|---|
| Ubuntu (mainline-style) | 4.15.18 | Does not build | Our build host's tree for this line stops before the compiler runs: a plain `make` ends at `No rule to make target '.../src/hal/ec/ite.o'` with zero `CC [M]` lines. That tree is also missing `scripts/mod/modpost` and `tools/objtool/objtool`, which the six other `/kernels` trees in this table have -- but the build stops earlier than either of those is needed, so this is our tree being incomplete, not the 4.15 kernel line. On a good 4.15 tree the driver does build, one subsystem at a time (see the two rows below). |
| Ubuntu (fetched `linux-headers` package, not under `/kernels`) | 4.15.0-101 -- EPC-WHL | Builds clean | A real, complete 4.15 tree ([recipe](test/README.md)), built one subsystem at a time: this board declares watchdog, gpio and hwmon (no misc). All 3 produced a `.ko`, `make` exited 0 for each, and every `.ko`'s `modinfo -F vermagic` reads `4.15.0-101-generic SMP mod_unload`. |
| Ubuntu (fetched `linux-headers` package, not under `/kernels`) | 4.15.0-101 -- ESM-KX60G | Builds clean | Same tree and method as the row above; this board declares all four subsystems. All 4 produced a `.ko`, `make` exited 0 for each, same `vermagic`. |
| Ubuntu (mainline-style) | 5.4.302 | Builds clean | |
| Ubuntu (mainline-style) | 5.15.211 | Builds clean | Needs the `-Wno-error=type-limits` line already in `Kbuild` (the kernel's own `bits.h` GENMASK trips this warning) |
| Ubuntu (mainline-style) | 6.6.89 | Builds clean | |
| Ubuntu (mainline-style) | 6.8.2 | Builds clean | |
| Ubuntu (mainline-style) | 6.12.27 | Builds clean | |
| Ubuntu (mainline-style) | 6.14.0 | Builds clean | |
| AlmaLinux 9.8 (EL9) | 5.14.0-687.29.1.el9\_8 | Builds clean | Real `kernel-devel` tree, not a vanilla kernel.org 5.14 |

"Builds clean" above means a plain `make` -- no subsystem named, so it builds
every subsystem the board's `.conf` declares in one run -- for every row
except the two `4.15.0-101` ones. Kernel 4.15's own build system rejects a
bare `make` (see the paragraph above), so those two rows come from the
per-subsystem targets (`make watchdog`, `make gpio`, `make hwmon`,
`make misc`) instead. The `4.15.18` row is still a plain `make`; it is the one
that fails before the compiler runs, on our own incomplete host tree.

Most rows above are graded on what the build printed rather than on its exit
code, because most of the kernel trees behind them are development trees with
no `Module.symvers`. Without that file modpost cannot resolve the kernel
symbols an out-of-tree module imports, and each kernel line answers that
differently: the 6.x trees turn it into `ERROR: modpost: "..." undefined!` and
exit 2 even when every object compiled cleanly, 5.15 reports the same symbols
as `WARNING:` and exits 0, and 5.4 does not report them at all. So "Builds
clean" above means the compiler ran and printed no `error:` line.

**On a normal installation none of that arises.** The `linux-headers` (or
`kernel-devel`) package for your running kernel ships its own
`Module.symvers`, modpost resolves every symbol, and the modules link
completely -- which is what the two `4.15.0-101` rows and the AlmaLinux row
were built against.

The two `4.15.0-101` rows are graded differently again: that kernel's Kbuild
prints no `CC [M]` lines even on a build that fully succeeds, so those rows
were graded on whether every subsystem the board declares produced its `.ko`
-- 3 of 3 and 4 of 4 -- with no `error:` line anywhere in the output. The
`4.15.18` row is a different case from both: on the tree it was built against
the build stops before the compiler runs at all, which is that particular
tree being incomplete rather than the 4.15 kernel line being unsupported --
the two rows below it are the same kernel line building clean.

Build status was measured on two boards, `EPC-WHL` (watchdog, gpio, hwmon)
and `ESM-KX60G` (all four), chosen because between them they exercise every
subsystem. It is not a per-board measurement: a board not named here is
untested on these kernels rather than unsupported.

---

## Board Configuration

The build system dynamically selects the target board configuration. It prioritizes the configuration in the following order:

1. **Command Line Argument:** `BOARD_NAME=xxx` passed to `make`.
2. **Auto-Detection:** Reads `/sys/class/dmi/id/board_name` from the host system.

Configuration files are located in `configs/boards/`.

Each board has its own file. If yours is missing, create it at `configs/boards/<BOARD_NAME>.conf`; `scripts/config.sh` then turns it into `src/configs/board.h`, which the build includes.

### Checking Your Board Name

To see what board name the system detects:

```bash
cat /sys/class/dmi/id/board_name
```

---

## Building the Drivers

### 1. Standard Build (Auto-Detect)

If you are compiling natively on the target Avalue board:

```bash
make
```

*This will auto-detect the board name and build all enabled modules defined in the `.conf` file.*

### 2. Manual Selection

If you are building for a different board or the auto-detection is incorrect:

```bash
make BOARD_NAME=ECM-APL
```

### 3. Building Specific Modules

You can force the build of specific modules, overriding the board configuration defaults:

* **Watchdog:**
```bash
make watchdog
```


* **GPIO:**
```bash
make gpio
```


* **Hardware Monitor:**
```bash
make hwmon
```


* **Misc:**
```bash
make misc
```

### 4. Debug Mode

To build the Watchdog driver with debug symbols and extra logging enabled:

* **Watchdog:**
```bash
make watchdog-debug
```

* **GPIO:**
```bash
make gpio-debug
```

* **Hardware Monitor:**
```bash
make hwmon-debug
```

* **Misc:**
```bash
make misc-debug
```

---

## Installation

The installation process copies the driver modules to a board-specific directory (e.g., `/lib/modules/.../extra/$(BOARD_NAME)/`) and configures the system to load them automatically at boot.

```bash
sudo make install
```

**Note:** If you only built specific modules (e.g., `make watchdog`), `make install` will detect this and **only install the compiled modules**. It will not install uncompiled drivers.

---

## Uninstallation

To remove the installed drivers from the system:

```bash
sudo make uninstall
```

This command will:

1. Stop and unload active modules from kernel memory.
2. Remove the driver files from `/lib/modules/$(uname -r)/extra/$(BOARD_NAME)/`.
3. Remove the auto-load configuration from `/etc/modules-load.d/`.
4. Update the module dependency map (`depmod`).

---

#### Contact

Please feel free to report any questions or issues to the support team.

**Copyright © Avalue Technology Co., Ltd. All Rights Reserved.**