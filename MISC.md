# Linux Kernel Misc Driver User Guide

This driver exposes a handful of board-specific EC registers — things like a
COM port's operating mode or the EC firmware version — through a single
ioctl character device, `/dev/misc`, plus one plain sysfs file per register
for quick read/write access without writing any C code at all.

Commands below that read `configs/boards/*.conf` (Section 5's board list, the `CONFIG_MISC_0_IOW` example in Section 3.3) assume the internal tree. Board configuration files are supplied with each board rather than bundled with every copy of this source — the public GitHub mirror, for example, ships without `configs/boards/`.

## 1. Prerequisites

Before interacting with the misc driver, ensure the following requirements
are met:

* **Driver Loaded:** The kernel module `misc.ko` must be successfully
  loaded. Check with `lsmod | grep misc`.
* **Device Node:** A character device node `/dev/misc` must exist.
* **Board Support:** Not every board builds this driver. Only a board whose
  `.conf` sets `MAKE_MISC_DEVICE=ec` compiles `misc.ko` and creates
  `/dev/misc`; a board that does not set it has neither. See
  [Section 5](#5-which-boards-have-this-device) for the current list.
* **Root Privileges:** The device node itself is created world read/write
  (mode `0666`), but the sysfs files it exposes are more restrictive — see
  Section 2.2 below.

---

## 2. What This Driver Exposes

### 2.1 The Device Node — `/dev/misc`

The driver registers a `struct miscdevice` with `.name = "misc"`:

```
$ grep -n '"misc"' src/drivers/misc.c
233:	.name = "misc",
```

That name is what puts the node at **`/dev/misc`** once `misc_register()`
runs in the module's init function (`src/drivers/misc.c`,
`struct miscdevice miscdev` around line 230, `misc_register(&miscdev)`
around line 247). The node's `file_operations` only wire up
`.open`, `.release` and `.unlocked_ioctl` — there is **no `.read`/`.write`**
on `/dev/misc` itself. In other words, `/dev/misc` exists purely so you can
`open()` it and issue `ioctl()` calls (Section 4); it is not a file you
`cat` or `echo` into directly.

### 2.2 The sysfs Attribute Files

For everyday use you usually don't need `ioctl()` at all. Every channel the
board `.conf` enables also gets a plain sysfs attribute file, created on the
same device the class registered:

```
/sys/class/misc/misc/<LABEL>
```

`<LABEL>` is that channel's `CONFIG_MISC_<n>_LABEL` from the board `.conf`
(e.g. `COM1_Mode`). The file is created mode `0644`, and reading it calls
into the same HAL code the ioctl path uses — `cat` triggers a driver read,
`echo N >` triggers a driver write.

> **⚠️ Mode `0644` does not mean every file is writable.** A channel with no
> `_IOW` macro in its board `.conf` — the driver checks this at runtime — has
> no write handler on the hardware side, and writing to its sysfs file
> returns `-EACCES` even though the file's own permission bits say `0644`.
> The `EC_FW_Version` channel on `ESM-KX60G` is exactly this case: it is
> read-only in practice, not just in convention. See Section 3.3.

> The `/dev/misc` ioctl path gives the same guarantee. A command number no
> channel defines — including `0`, which every board reserves as "no
> command here" — is rejected with `-ENOTTY` and never reaches the EC.
> `ioctl()` does not raise `-EACCES` the way the sysfs write does; it
> raises `-ENOTTY` instead, since from the ioctl path's point of view an
> unsupported command and an unrecognized one look the same.

---

## 3. Reading and Writing Channels via sysfs

### 3.1 Reading a Channel

```bash
cat /sys/class/misc/misc/COM1_Mode
```

This prints the current register value as a decimal number.

### 3.2 Writing a Channel

```bash
echo 1 | sudo tee /sys/class/misc/misc/COM1_Mode
```

The driver parses the string as an unsigned 32-bit integer (`kstrtou32()`,
any base) and writes it to the hardware register.

### 3.3 Read-only Channels — `EC_FW_Version`

On `ESM-KX60G`, the single enabled channel is `EC_FW_Version`
(register `0x4B`). Its board `.conf` sets `CONFIG_MISC_0_IOW=0` — there is no
write ioctl for it at all:

```
$ grep -n 'CONFIG_MISC_0_IOW' configs/boards/ESM-KX60G.conf
203:CONFIG_MISC_0_IOW=0
```

Reading it works as normal (`cat /sys/class/misc/misc/EC_FW_Version`), but
writing it fails plainly:

```bash
$ echo 1 | sudo tee /sys/class/misc/misc/EC_FW_Version
tee: /sys/class/misc/misc/EC_FW_Version: Permission denied
```

Do not treat the `0644` mode on this file as a promise that it is writable —
on this board, it is not.

The `/dev/misc` ioctl path gives the same guarantee. `CONFIG_MISC_0_IOW=0`
means there is no write command to build for this channel (Section 4.1) —
send `ioctl(fd, 0, &value)` anyway and the driver rejects it with
`-ENOTTY` before it ever touches EC register `0x4B`. Reading through
`ioctl()` still works, the same as through sysfs.

---

## 4. Reading a Channel via `ioctl()`

If you are writing your own C program instead of shelling out to sysfs, you
talk to `/dev/misc` directly with `ioctl()`.

### 4.1 How the ioctl Number Is Built

Each channel's ioctl numbers are generated by the board `.conf`, not
hard-coded in the driver:

```
CONFIG_MISC_<n>_IOR = _IOR(CONFIG_MISC_IOCTL_BASE, CONFIG_MISC_<n>_REG, u32)
CONFIG_MISC_<n>_IOW = _IOW(CONFIG_MISC_IOCTL_BASE, CONFIG_MISC_<n>_REG, u32)
```

`CONFIG_MISC_IOCTL_BASE` is `'A'` on every board that has this device, and
`CONFIG_MISC_<n>_REG` is that channel's hardware register (e.g. `0x20` for
`COM1_Mode`). Build these two macros yourself from the board's `.conf` before
writing an ioctl call — do not guess a magic number.

A `.conf` value of `0` for a channel's `_IOW` (as `ESM-KX60G` sets for its
one channel, Section 3.3) means that channel has no write command at all —
there is nothing to build. Sending command `0` itself is always rejected
with `-ENOTTY`: the driver never dispatches a command number no channel
defines, so a mistake here fails loud instead of touching hardware.

### 4.2 Worked Example — `COM1_Mode` on a board with the channel enabled

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

This program has been **compiled** (`gcc -Wall -Wextra -c`, zero warnings)
but not run — actually exercising the `ioctl()` call needs a real board with
`/dev/misc` present. Treat the register value and IOR macro as read off the
source and the board `.conf`, and the compile step as the only part verified
in this guide.

---

## 5. Which Boards Have This Device

`misc.ko` is only built for a board whose `.conf` sets
`MAKE_MISC_DEVICE=ec`. As of this writing that is five boards out of the
full board list:

```
$ grep -l '^MAKE_MISC_DEVICE=ec' configs/boards/*.conf
configs/boards/ECM-ASL.conf
configs/boards/EMS-ARH.conf
configs/boards/ESM-KX60G.conf
configs/boards/HID-2340.conf
configs/boards/NUC-RPU.conf
```

| Board | Channel(s) | Register | sysfs label | Writable? |
|---|---|---|---|---|
| ECM-ASL | 0 | `0x20` | `COM1_Mode` | yes |
| EMS-ARH | 0, 1 | `0x20`, `0x21` | `COM1_Mode`, `COM2_Mode` | yes, yes |
| HID-2340 | 0 | `0x20` | `COM1_Mode` | yes |
| NUC-RPU | 0, 1 | `0x20`, `0x21` | `COM1_Mode`, `COM2_Mode` | yes, yes |
| ESM-KX60G | 0 | `0x4B` | `EC_FW_Version` | **no** — see Section 3.3 |

A board not in this table (any board that does not set `MAKE_MISC_DEVICE`)
builds no `misc.ko`, and has neither `/dev/misc` nor any of the sysfs files
above.
