# Tests you can run on the board

Two tests here run against real hardware, and both are meant to be run by
whoever has the board in front of them. They discover what to test from the
running system rather than from a configuration file, so they say what this
board actually does -- not what a build was told it would do.

| Test | Needs | Answers |
|---|---|---|
| [`board-acceptance.sh`](board-acceptance.sh) | root, the drivers loaded | Does every subsystem this board presents behave? Writes a Markdown report and ends on RELEASE or DEBUG. |
| [`test-gpio.sh`](test-gpio.sh) | root, `gpiod` tools, loopback wiring | Does every GPIO line drive and read back, in both directions? |

Start with `board-acceptance.sh`: it covers all four subsystems and needs no
wiring. `test-gpio.sh` goes further on GPIO alone, and is the one to reach
for when acceptance reports a GPIO fault and you want to know which line.

Both are non-destructive unless you tell them otherwise. Where a check has
to change hardware state -- driving a line, writing a PWM duty, arming the
watchdog -- it is a question asked before the run starts.

## GPIO loopback hardware test

Run the real GPIO loopback test from the repository root:

```sh
sudo ./test/test-gpio.sh
```

This needs the GPIO driver loaded, root access, and the `gpiodetect`,
`gpioget`, and `gpioset` tools. Wire each line in the first half of the
board's GPIO lines to the matching line in the second half before running
it. The test drives both directions and both values across every pair.

**Every pair is tested, and a failing one does not end the run**, because which
*other* pairs responded is the only thing that separates the two findings a
failure here can be. Three outcomes, and they are different findings:

| outcome | exit | what it means |
|---|---|---|
| every pair followed | `0` | `All <N> GPIO line pair tests passed.` |
| some followed, some did not | `1` | the wiring is demonstrably present, so the pairs that did not follow are the board |
| **no pair followed, either direction** | `2` | either no loopback wiring is fitted or the driver's output never reaches the pins — this test cannot tell those apart, and says so rather than picking one |

Stopping at the first failing pair, as this used to, reports one pair as though
it were the finding and throws away the evidence that would have told those
apart.

**`gpioset` must hold the line while `gpioget` reads it, and which libgpiod is
installed decides whether it does so on its own.** v1 defaults to
`--mode=exit` — *"set values and exit immediately"* — which releases the line
before the read, so the input reads its pull-up: a `1` where a `0` was driven,
which is exactly what a broken loopback pair looks like. v1 is therefore asked
for `--mode=signal`, *"set values and wait for SIGINT or SIGTERM"*, which is
what the test sends after each read. v2 holds until interrupted and has no
`--mode` option at all, so it is passed nothing. The test settles this by asking
`gpioset` for its own options rather than by reading a version string, and
prints which it chose.

## Whole-board acceptance test (hardware)

Run it on the target board, as root, from the repository root:

```sh
sudo ./test/board-acceptance.sh            # ask before each hardware-touching check
sudo ./test/board-acceptance.sh --dry-run  # ask nothing, confirm the drivers only
sudo ./test/board-acceptance.sh --all      # ask nothing, run everything
```

Three checks change what the board is doing, and the plain run **asks about each
one before it starts** — three answers, then the rest of the run is unattended.
`--dry-run` asks nothing and takes every default, leaving a run that confirms
the drivers are loaded, are ours, and read back what they registered, without
changing any hardware state. `--all` asks nothing and answers yes to everything,
which is the unattended full pass — and, on a board whose `nowayout` is set,
the run that reboots it. Asking for both at once is refused rather than resolved
by which flag came last.

With no terminal to answer on — a run under `cron`, a pipe, a CI job — the plain
run behaves as `--dry-run` and records that it did, because a prompt nobody can
see is a hang rather than a default.

It exercises every Avalue driver subsystem the machine actually presents,
writes a Markdown report, and ends on one verdict: **RELEASE** when every check
that ran passed, **DEBUG** when any did not. Exit status is 0 for RELEASE, 1 for
DEBUG, 2 when it could not run at all. The report path defaults to
`./avalue-acceptance-<board>-<timestamp>.md`; `--report <path>` moves it.

Subsystems run in the order **hwm, gpio, misc, and the watchdog last**. That
order is a safety property, not a preference: opening the watchdog device arms
the timer, and while `nowayout` is set nothing short of a reset stops it, so a
board that fails the stop check reboots and takes any not-yet-run check with it.
Last means a reset costs nothing already measured — and the report is written to
disk *before* the device is opened, so a board that goes down still leaves
behind everything measured up to that moment, saying so in the report.

It discovers what to test from the running system rather than from a board
configuration file, so it ships with the driver and a customer can run it on
the board in front of them. What it finds is the expectation: the hwmon device
the driver registered, the channels it published, the gpiochip it added, the
watchdog it registered. Missing dependencies are installed automatically —
`gpiod` for the GPIO checks, through whichever of apt/dnf/yum/zypper/pacman the
distribution has — and `--no-install` turns that off, skipping what it needed.

**A driver this board carries but has not loaded is loaded, not skipped.** From
`/sys/module` alone, "this board has no such subsystem" and "the module simply
is not loaded" look identical, and reporting both as a skip is how a real fault
hides inside a RELEASE verdict — the first hardware run of this test reported
`wdt`, `gpio` and `misc` as plain skips on a board whose configuration builds
two of them. So `modprobe` is asked to resolve each name first, and one it finds
but cannot load is a **failure**. `--no-load` turns the loading off.

A name `modprobe` cannot resolve is reported as **exactly that** — no module of
that name is installed on this machine — and recorded as a **skip**, missing
coverage. It is deliberately not called "this board has no such subsystem":
`modprobe` resolves against *installed* modules, so a board that builds the
driver and was never `make install`ed lands in the same branch, and the stronger
wording would hide the very gap this test exists to find. When the built `.ko`
is sitting in the repository the test was run from, the skip says so and names
the install step.

What it checks, per area:

- **modules** — every driver this board carries is loaded, and the module
  actually holding each name **is** an Avalue driver. `hwm`, `gpio`, `wdt` and
  `misc` are generic enough that something else could answer to them, and every
  later check trusts the name. A `/sys/module` entry with no `initstate` means
  nothing was inserted under that name — what a built-in looks like to a plain
  directory test — and that is a failure on its own.

  **Identity is settled by `srcversion`, not by looking the name up.** `modinfo`
  can only be asked about a *name*, and it answers from `/lib/modules`, so it
  describes whatever is *installed* under that name — which need not be what is
  resident. A driver inserted straight from its build tree with `insmod` is not
  installed at all, and a name that a mainline module also uses resolves to that
  one. Both are ordinary bench situations, and in both the answer is about a
  different file. `srcversion` is what settles it: modpost computes it from the
  module's own source and the kernel publishes the resident module's under
  `/sys/module/<m>/srcversion`, so a module and a file carrying the same one
  were built from the same source. The resident module is matched first against
  `<m>.ko` in the tree this test was run from, then against the installed module
  of that name; whichever matches supplies the `MODULE_DESCRIPTION` that decides
  the verdict.

  Only a *positive* match can convict. When the installed module is confirmed
  resident and does not say "for Avalue boards", that is a failure and the
  report says whether a mainline module holds the name. When nothing matches,
  there is no evidence either way — that is recorded as a **skip**, missing
  coverage, and the subsystem is left untested rather than trusted or condemned.

  **A module this test loaded and then could not confirm is unloaded again** —
  inserting an unrelated driver into a board under test is a side effect nobody
  asked for. A module that was **already resident** when the run started is left
  where it is, since this run did not put it there; either way its subsystem
  goes into the report's "subsystems not exercised" list, which is the one place
  an operator reads to find out what this run did not cover.

  This is not hypothetical. On a real EMX-W880P, `modprobe gpio` succeeded and
  loaded `/lib/modules/<ver>/kernel/drivers/mtd/nand/raw/gpio.ko` — the
  mainline **GPIO NAND driver** — because the Avalue module is also called
  `gpio` and the kernel's own module wins the name. Without this check that
  reads as `PASS ... gpio loads`, and every GPIO check after it is looking for
  a chip a driver that was never loaded would have registered.
- **hwm** — the hwmon device exists; every channel reads a plausible value for
  its quantity; **both sysfs layouts name the same channels**, so a write to
  `pwm1` reaches the channel `pwm/pwm1` describes; every PWM channel says
  what it drives.
- **gpio** — a gpiochip labelled `gpio` with an even, readable line count; every
  line is read and the ones that refuse are **named**, not just counted; the
  line map and the values at rest go into the report, so a loopback failure can
  be read against them. When no chip is found, the report records what
  `gpiodetect` *did* list, which character devices exist, what label each
  gpiochip carries in sysfs, and what the driver said in `dmesg` — because a
  driver that registered nothing, a chip under a different label, and output
  this does not parse are three different faults and the report has to tell them
  apart.
- **misc** — `/dev/misc` is a character device and every published channel is
  read out. A channel that answers `EACCES` is write-only by design and is
  recorded as such; any other read error is a failure.
- **wdt** — a watchdog registered under this board's identity, with a timeout;
  state, bootstatus and nowayout recorded. Every watchdog device present is
  listed, since a board commonly carries a platform one from the chipset
  alongside this driver's. **The board name in DMI and the one the driver
  publishes are not the same string**: DMI reports what the BIOS carries and can
  append a BIOS identifier (`EMX-W880P(EMX-W880P_0B)` is a measured example),
  while the identity is built from the name in the board's `.conf`
  (`EMX-W880P ite Watchdog`). The match is on the leading token both agree on,
  anchored at the start of the identity and followed by a space so it can only
  match the identity's own board field. Matching the whole DMI string reported a
  registered watchdog as missing and took the subsystem with it.

Three checks touch hardware state, and each one is a question rather than a
flag:

- **Write a PWM duty?** Writes a duty to a fan-labelled PWM channel, reads it
  back through both sysfs paths, watches the tachometer, and restores the
  original value. Backlight channels are never written.
- **Drive the GPIO lines?** Runs `test/test-gpio.sh`, which needs pin-to-pin
  loopback wiring. The question asks the operator to answer yes **only if that
  wiring is fitted**, because that is the one fact the test cannot measure for
  itself — and it is what decides how a total failure reads. A failure carries
  **everything** that test printed, not only its error line: the hold mode, and
  which pairs responded, are what separate a dead line from an unwired bench
  from a `gpioset` that released the line before it could be read. The hold mode
  is recorded on a pass too — a loopback that passed says which libgpiod it
  passed with.
- **Arm the watchdog?** Checks that it counts down, that a ping refreshes the
  countdown, and that a magic close does what `nowayout` says it should.

All three are asked **before any check runs**, so an operator answers three times
and can then leave a run that takes minutes alone. Asking up front costs one
thing worth naming: a question put before discovery cannot say *which* fan
channel or *which* watchdog it is about. The one answer where that detail decides
the outcome is therefore confirmed a second time, at the point of use, where the
value actually read can be quoted back — see below.

**Arming can reset the board, and the test says so before it does.** The
magic-close stop only works when `nowayout` is clear; with `nowayout` set,
arming is a decision to reboot the machine. So a yes to the watchdog question is
followed by a second question that quotes the `nowayout` value just read, and a
no there is recorded with that value rather than passing silently. `--all`
answers that second question yes as well: an unattended full pass on a
`nowayout` board is a reboot, by design and in the open.

**`nowayout` decides what the magic close is supposed to do, and the verdict
follows it.** With `nowayout` clear, a magic close must stop the timer. With it
set, the driver's contract is that software *cannot* stop the timer once running
— it says so at initialisation — so a watchdog still counting after a magic close
is that promise being kept, and one that stopped is the defect: a timer that can
be talked out of firing is no protection, and the board's operator was told it
could not be. Judging both cases by the `nowayout=0` rule is how a board doing
exactly what it documents gets failed for it, which is what a real `nowayout=1`
board did on the bench. The board still goes down either way, and the run says
so on the console whichever verdict it records.

Every answer is written into the report, under **Mode** and **Answers**, so a
check that was declined and a check that was never asked about do not read alike
months later.

**A skip is missing coverage, not a pass.** The report lists every skipped check
with what would let it run, under the verdict as well as in the table, because a
RELEASE verdict over a page of skips is not a tested board.
