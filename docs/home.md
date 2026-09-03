# Avalue Linux Driver Suite 4.0

Linux kernel drivers for Avalue industrial motherboards and embedded SBCs.
Driver 4.0 exposes four standard kernel interfaces — a `gpiochip` character
device, a `hwmon` class device, `/dev/watchdog`, and `/dev/misc` for the
board's EC registers — so you drive it with ordinary userspace tools
(`libgpiod`, `lm-sensors`, the `watchdog` daemon, sysfs) rather than with
anything Avalue-specific.

Everything here is built from one per-board config file
(`configs/boards/<BOARD_NAME>.conf`); the build turns it into a header and
compiles only the subsystems that board actually declares. Nothing
board-specific is hard-coded in the drivers.

## Pages

Start with the API guide. The four subsystem pages under it are the
reference for one interface each, and go further than the guide does.

| Page | What it covers |
|---|---|
| [Linux X86 API](Linux-X86-API.md) | The complete user-facing guide: installing, GPIO, hardware monitor, watchdog, misc/EC channels, code samples, and the `make` interface. **Start here.** |
| [Supported boards](Supported-Boards.md) | Every board this repo carries a config for, which subsystems that board's own config builds, and whether it has been bench-verified. |
| [GPIO](GPIO.md) | The `gpiochip` character device: line numbering, direction, and driving lines with `libgpiod`. |
| [Hardware monitor](HWM.md) | The `hwmon` class device: which voltage, temperature, fan and PWM channels a board publishes, their labels, and how raw readings are scaled. |
| [Watchdog](WDT.md) | `/dev/watchdog`: arming, pinging, the timeout range, and what `nowayout` means for stopping it. |
| [Misc / EC](MISC.md) | `/dev/misc`: reading and writing the board's EC registers, and which boards declare the device. |

## Source

The drivers live in the `avalue-driver-4.0` repository. `README.md` there
covers prerequisites and the kernel versions the suite is known to build
on; `configs/README.md` is the guide for adding a board or a new chip HAL.
Every page above is a file in that repository's `docs/` directory, so the
same text reads as a wiki here and as a directory of Markdown there.

## About these pages

These pages are **published from the driver repository, not edited here.**
Each one is authored and reviewed under [`docs/`](.) in the source
tree, as an ordinary change — with a test grading the page's claims against
that tree — and only then copied onto this wiki. An edit made directly
here is lost the next time a page is published, so corrections belong in a
merge request against `docs/`.
