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

| Page | What it covers |
|---|---|
| [Linux X86 API](Linux-X86-API) | The complete user-facing guide: installing, GPIO, hardware monitor, watchdog, misc/EC channels, code samples, and the `make` interface. Start here. |
| [Supported boards](Supported-Boards) | Every board this repo carries a config for, which subsystems that board's own config builds, and whether it has been bench-verified. |

## Source

The drivers live at
[internal/avalue-driver-4.0](http://192.168.100.17/internal/avalue-driver-4.0).
`README.md` there covers prerequisites and the kernel versions the suite is
known to build on; `configs/README.md` is the guide for adding a board or a
new chip HAL.

## About these pages

These pages are **published from the driver repository, not edited here.**
Each one is authored and reviewed under
[`docs/wiki/`](http://192.168.100.17/internal/avalue-driver-4.0/-/tree/master/docs/wiki)
as an ordinary merge request — with a test grading the page's claims against
the source tree — and only then copied onto this wiki. An edit made directly
here is lost the next time a page is published, so corrections belong in a
merge request against `docs/wiki/`.
