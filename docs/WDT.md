
# Watchdog Service Guide

This document describes how to set up the **Userspace Watchdog Daemon** to interact with the Avalue Kernel Driver. The service is responsible for "kicking" (feeding) the watchdog timer to prevent the system from resetting.

## Prerequisites

Before setting up the service, ensure the kernel driver is loaded:

```bash
# Verify the driver is loaded
lsmod | grep wdt

# Verify the device node exists
ls -l /dev/watchdog*
```

---

## Using Standard Linux Watchdog Daemon

For production environments, it is recommended to use the standard Linux `watchdog` package. It provides robust monitoring features (CPU load, memory usage, network status, etc.).

### Hardware timeout ranges

The driver advertises the timeout range implemented by the selected watchdog
chipset. ITE EC watchdogs use the two-byte counter at EC registers `0x48` and
`0x49`, providing a range of 1 to 65,535 seconds. The F81966 and NCT61x6D SIO
watchdogs retain their one-byte range of 1 to 255 seconds.

A `WDIOC_SETTIMEOUT` of `0` disables the watchdog timer; `1` second is the
shortest timeout that keeps it running.

### Once the watchdog starts, it cannot be stopped (unless you ask it to allow that)

**This section describes the default, `nowayout=1`.** See "Turning it off:
`nowayout=0`" below for the other setting.

**The `nowayout` module parameter defaults to on.** A customer who does
nothing sees exactly today's behaviour: once the watchdog is running,
nothing in software stops it -- not `WDIOC_SETOPTIONS`, and not closing
`/dev/watchdog0`.

So if you stop the service (`sudo systemctl stop watchdog`) and nothing else
reopens `/dev/watchdog0`, the board resets about one timeout period later.
The kernel prints `watchdog0: watchdog did not stop!` in `dmesg` and feeds
the watchdog one last time; nothing feeds it after that. This is expected --
it is what the reset test below relies on. Plan for it before you stop the
service on a running system.

**On Debian/Ubuntu, something else usually reopens it.** The `watchdog`
package ships with `run_wd_keepalive=1` in `/etc/default/watchdog` as its
own default answer
([Debian source package `watchdog` 5.16-1, `debian/postinst`, line 53](https://sources.debian.org/src/watchdog/5.16-1/debian/postinst/#L53);
the sysvinit path sets the same default in
[`debian/init`, line 33](https://sources.debian.org/src/watchdog/5.16-1/debian/init/#L33)),
which arms `watchdog.service`'s own `OnFailure=`/`ExecStopPost=` trick
([`debian/watchdog.service`](https://sources.debian.org/src/watchdog/5.16-1/debian/watchdog.service/))
to start a second daemon, `wd_keepalive`, right after `watchdog.service`
stops. `wd_keepalive` opens `/dev/watchdog0` on its own and feeds it in a
loop, so on a default Debian/Ubuntu install `sudo systemctl stop watchdog`
by itself does **not** reset the board -- `wd_keepalive` is now the one
feeding it. RHEL/CentOS ships no such handoff: upstream's own `redhat/`
init script's `stop()` just sends `SIGTERM` and cleans up, nothing else
([same package, `redhat/watchdog.init`](https://sources.debian.org/src/watchdog/5.16-1/redhat/watchdog.init/)),
so there `systemctl stop watchdog` behaves exactly as described above.

**`sudo systemctl stop watchdog wd_keepalive` does not reach that state on
Debian/Ubuntu.** `wd_keepalive` is not running yet at the moment you type
that command -- it only starts *after* `watchdog.service` stops and enters
the `failed` state, which is what `OnFailure=` above is waiting for. So
naming it in the same command stops nothing, and it comes up moments later
holding `/dev/watchdog0` again. Use the package's own switch instead:

```bash
sudo sed -i 's/^run_wd_keepalive=.*/run_wd_keepalive=0/' /etc/default/watchdog
sudo systemctl stop watchdog
```

With `run_wd_keepalive=0`, `ExecStopPost=` succeeds instead of failing,
`watchdog.service` never enters `failed`, and `wd_keepalive` never starts.
Confirm both are stopped:

```bash
systemctl is-active watchdog wd_keepalive
```

Both should print `inactive`. If you already tried the broken command above
first, `wd_keepalive` can still be `active` here: `stop watchdog` on an
already-stopped service is a no-op that exits `0`, and `wd_keepalive`'s own
unit only reacts to `watchdog.service` *starting*, not stopping
([`debian/wd_keepalive.service`, `Conflicts=`](https://sources.debian.org/src/watchdog/5.16-1/debian/wd_keepalive.service/)).
Stop it directly: `sudo systemctl stop wd_keepalive`.

Once both print `inactive`, nothing is feeding the watchdog anymore. On this
section's default (`nowayout=1`), that is the point of doing this: the board
resets about one timeout period later, the same reset described at the top
of this section. With `nowayout=0` loaded first (see below), it does not
reset -- the watchdog is genuinely off.

(This is why the reset test below kills both `watchdog` and `wd_keepalive`
directly, not just stops `watchdog`.)

#### Turning it off: `nowayout=0`

Load the module with the parameter off, one-shot:

```bash
sudo modprobe wdt nowayout=0
```

To make it stick across reboots, create `/etc/modprobe.d/avalue-wdt.conf`
containing:

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

With `nowayout=0` loaded, two things really stop the watchdog: `WDIOS_DISABLECARD`
via `WDIOC_SETOPTIONS` now reaches the chip and really stops it, and the
magic-close sequence -- write the single byte `V` to `/dev/watchdog0`, then
close it:

```bash
printf 'V' > /dev/watchdog0
```

Bash opens the file descriptor, writes the single byte `V` with no trailing
newline, and closes it when the command completes -- that open-write-close
*is* the magic-close sequence the kernel checks for.

**`/dev/watchdog0` is single-open, so this only works when nothing else
already has it open.** A second `open()` while the file is already open
returns `-EBUSY`, so the command above fails with `Device or resource busy`
if the `watchdog` service, or a customer's own daemon, is running and
feeding the board -- which is the ordinary case this section is about.
There, the magic close has to come from *that* process, on its own file
descriptor: it writes `V` and then closes, the same way stopping the
service does in the section above rather than a shell command run against
a file another process holds (`sudo lsof /dev/watchdog0` below shows who
holds it). The command above only does something useful on its own when
nothing holds the file yet (a no-op round trip: it opens, starts the
watchdog, writes `V`, and closes it again) or when the watchdog is running
with nobody holding the file (someone closed it earlier without writing
`V`, and it is counting down to a reset) -- there it genuinely stops the
countdown.

**Confirmed: the standard `watchdog` daemon really does write `V` on a clean
stop.** Its own shutdown path, `close_watchdog()` in `src/keep_alive.c`,
always writes the byte before closing the device it holds open
([Debian source package `watchdog` 5.16-1](https://sources.debian.org/src/watchdog/5.16-1/src/keep_alive.c/),
line 284; `sudo systemctl stop watchdog` sends the `SIGTERM` that
reaches this path). So on RHEL/CentOS, `sudo systemctl stop watchdog` with
`nowayout=0` loaded really does leave the watchdog off: no reset, and
nothing feeds it again until you start the service. **On Debian/Ubuntu**,
remember the caveat above: the package restarts `wd_keepalive` right after,
and `wd_keepalive` reopens the device and starts feeding it again -- so the
watchdog does not stay off unless you also turn off `run_wd_keepalive`
first, the way the Debian/Ubuntu note above describes.

**What does not change with `nowayout=0`:** closing `/dev/watchdog0`
*without* first writing `V` still does not stop the watchdog, and still does
not keep it alive either -- the kernel still prints `watchdog0: watchdog did
not stop!`, feeds it one last time, then stops feeding it, and the board
still resets about one timeout period later, exactly as described above.
Only a deliberate `V`-then-close actually stops it when `nowayout=0`.

### 1. Installation

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install watchdog
```

**CentOS/RHEL:**

```bash
sudo yum install watchdog
```

### 2. Configuration

Edit the configuration file `/etc/watchdog.conf`:

```bash
sudo nano /etc/watchdog.conf
```

Uncomment and modify the following lines to match the hardware driver:

```ini
# The device node created by this driver
watchdog-device = /dev/watchdog # or /dev/watchdog0

# Interval between heartbeats (seconds)
# Must be smaller than the hardware timeout (default is usually 60s)
interval = 10
```

### 3. Service Management

Enable and start the service:

```bash
sudo systemctl enable watchdog
sudo systemctl start watchdog
```

Check status:

```bash
sudo systemctl status watchdog
```
---

## ⚠️ Verification & Testing

**WARNING: The following tests will cause a system reboot.**

To verify that the hardware watchdog is actually working (i.e., it reboots the system when not fed), perform the following test:

**This test works the same whether `nowayout` is `0` or `1`.** `kill -9`
sends `SIGKILL`, which the process cannot catch or run any shutdown code
for, so it never reaches the daemon's own `close_watchdog()` write of `V`
(see "Turning it off: `nowayout=0`" above). Killing both `watchdog` and
`wd_keepalive` this way, as Step 2 does, closes `/dev/watchdog0` with no
magic-close either way, and closing without `V` does not stop the watchdog
under either setting -- once Step 3 confirms nothing else holds
`/dev/watchdog0`, the board still resets. If you stop the daemons the
clean way instead, with `nowayout=0` loaded, the board will **not** reset --
see "Turning it off: `nowayout=0`" above for the command and why.

### Step 1: Check Timeout

Check the current timeout setting

```bash
cat /sys/class/watchdog/watchdog0/timeleft
```

### Step 2: Stop All Watchdog Services

Stop all watchdog-related services to simulate a system freeze.

```bash
sudo killall -9 watchdog
sudo killall -9 wd_keepalive
```

**On Debian/Ubuntu, systemd starts `wd_keepalive` again moments after
this.** `watchdog.service` is `Type=forking`, so killing its main process
this way still runs `ExecStopPost=`, which fails on purpose while
`run_wd_keepalive=1` and puts the unit in `failed`;
`OnFailure=wd_keepalive.service` then starts the second daemon, which
reopens `/dev/watchdog0` and feeds it
([`debian/watchdog.service`](https://sources.debian.org/src/watchdog/5.16-1/debian/watchdog.service/)).
The two `killall` commands above run one right after the other, so this
respawn usually lands only after both have already been typed. RHEL/CentOS
ships no such handoff, so there this step is the whole story.

### Step 3: Verify Watchdog Device is Released

Ensure no process is holding the `/dev/watchdog0` device:

```bash
sudo lsof /dev/watchdog0
```

If any process is still holding it, kill it explicitly:

```bash
sudo kill -9 <pid>  # e.g., sudo kill -9 234
```

**On Debian/Ubuntu, this normally finds `wd_keepalive` holding the
device -- that is the previous step's respawn, not a leftover.** Killing
it here is a required part of the test, not a formality: skip it and
`wd_keepalive` keeps feeding the watchdog, so the board never resets. Once
killed it stays dead -- `debian/wd_keepalive.service` carries no
`OnFailure=` and no `Restart=`
([`debian/wd_keepalive.service`](https://sources.debian.org/src/watchdog/5.16-1/debian/wd_keepalive.service/)),
so there is no loop to repeat.

### Step 4: Wait for Reboot

Wait for the timeout period (e.g., 60 seconds). The system should automatically reset.
