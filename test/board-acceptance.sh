#!/bin/bash
# mirror-run-exempt: runs against a live board -- needs root, the loaded Avalue drivers, and real hardware to read.
#
# Whole-board acceptance test. Exercises every Avalue driver subsystem this
# machine actually presents, writes a Markdown test report, and ends on one
# verdict: RELEASE when every executed check passed, DEBUG when any did not.
#
# It discovers what to test from the running system rather than from a board
# configuration file, so it ships with the driver and a customer can run it on
# the board in front of them. What it finds IS the expectation: the hwmon
# device the driver registered, the channels it published, the gpiochip it
# added, the watchdog it registered.
#
# Non-destructive by default. Three things touch hardware state -- writing a PWM
# duty, driving the GPIO lines, and arming the watchdog -- and each is a question
# asked before the run starts rather than a flag remembered from the last time.
# --dry-run answers all three with their defaults and only confirms the drivers;
# --all answers yes to every one of them and runs unattended.
#
# Subsystems run in the order hwm, gpio, misc, and the WATCHDOG LAST. That order
# is a safety property, not a preference: opening the watchdog device arms the
# timer, and while nowayout is set nothing short of a reset stops it, so a board
# that fails the stop check reboots and takes any not-yet-run check with it.
# Last means a reset costs nothing already measured, and the report is written
# to disk before the device is opened.
#
# A driver this board carries but has not loaded is loaded with modprobe rather
# than skipped: "not loaded" and "this board has no such subsystem" look
# identical from /sys/module, and reporting both as a skip is how a real fault
# hides inside a RELEASE verdict.
#
# Usage:
#   sudo ./test/board-acceptance.sh [options]
#
#   (no options)      ask before each check that changes hardware state
#   --dry-run         ask nothing, take every default -- confirm the drivers only
#   --all             ask nothing, answer yes to everything, including a watchdog
#                     arm that resets a board whose nowayout is set
#   --report <path>   where to write the report
#                     (default: ./avalue-acceptance-<board>-<timestamp>.md)
#   --no-install      do not install missing dependencies, skip what needs them
#   -h, --help        this message
#
# Exit status: 0 = RELEASE, 1 = DEBUG, 2 = could not run.
#
# AVALUE_ACCEPTANCE_SYSFS_ROOT prefixes every /sys path this script reads, so
# the whole run can be driven against a scratch tree of planted faults rather
# than a board. Setting it also waives the root requirement, because nothing
# real is being read or written. Leave it unset on a real board.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

REPORT=""
DO_INSTALL=1
DO_LOAD=1

# What the three hardware-touching checks were answered. Set from the questions
# below, never from the command line: an answer belongs to the run that gave it.
DO_PWM_WRITE=0
DO_GPIO_LOOPBACK=0
DO_WDT_ARM=0

# ask   -- put each question to the operator
# dry   -- ask nothing, take every default (no is the default for all three)
# all   -- ask nothing, answer yes to everything
RUN_MODE="ask"
RUN_MODE_LABEL="interactive"
SAW_DRY_RUN=0
SAW_ALL=0

usage() {
	cat <<'USAGE'
Whole-board acceptance test for the Avalue drivers. Exercises every subsystem
this machine presents, writes a Markdown report, and ends on one verdict:
RELEASE when every executed check passed, DEBUG when any did not.

Three checks change what the hardware is doing: writing a PWM duty, driving the
GPIO lines, and arming the watchdog. Run with no options and the test asks about
each one before it starts, so three answers are all it wants from you and the
rest of the run is unattended.

Usage:
  sudo ./test/board-acceptance.sh [options]

  (no options)        ask about each hardware-touching check, then run
  --dry-run           ask nothing and take every default: confirm that the
                      drivers are loaded, are ours, and read back what they
                      registered, without changing any hardware state
  --all               ask nothing and answer yes to every question, including
                      the one that accepts a watchdog arm resetting this board
  --report <path>     where to write the report
                      (default: ./avalue-acceptance-<board>-<timestamp>.md)
  --no-install        do not install missing dependencies, skip what needs them
  --no-load           do not modprobe a driver this board carries but has not
                      loaded; skip that subsystem instead
  -h, --help          this message

--all can reset this board. Arming a watchdog whose nowayout is set cannot be
undone by software, so the run that answers yes to everything is also the run
that reboots a board configured that way. Interactive mode asks that one twice,
naming the nowayout value it read.

Subsystems are tested in the order hwm, gpio, misc, and the watchdog LAST,
because a real watchdog test can end the machine -- the report is written to
disk before the timer is ever armed, so a board that resets still leaves
everything measured up to that point behind.

With no terminal to ask on, a run with no options behaves as --dry-run and
records that it did, because a prompt nobody can see is a hang, not a default.

Exit status is 0 for RELEASE, 1 for DEBUG, 2 when the test could not run at all.
USAGE
}

while [ $# -gt 0 ]; do
	case "$1" in
	--report)
		if [ $# -lt 2 ]; then
			echo "error: --report needs a path" >&2
			exit 2
		fi
		REPORT="$2"
		shift 2
		;;
	--no-install)      DO_INSTALL=0; shift ;;
	--no-load)         DO_LOAD=0; shift ;;
	--dry-run)         SAW_DRY_RUN=1; shift ;;
	--all)             SAW_ALL=1; shift ;;
	-h|--help)       usage; exit 0 ;;  # unchecked-ok: a case-pattern alternation, not a pipeline -- the scanner's top-level '|' heuristic cannot tell them apart
	--pwm-write|--gpio-loopback|--wdt-arm|--wdt-allow-reset)  # unchecked-ok: a case-pattern alternation, not a pipeline -- same as the option parser above
		echo "error: '$1' is no longer an option -- the test asks about that check instead." >&2
		echo "       Run it with no options to be asked, --all to answer yes to everything," >&2
		echo "       or --dry-run to answer nothing and only confirm the drivers." >&2
		exit 2
		;;
	*)
		echo "error: unknown option '$1' -- see --help" >&2
		exit 2
		;;
	esac
done

if [ "$SAW_DRY_RUN" -eq 1 ] && [ "$SAW_ALL" -eq 1 ]; then
	echo "error: --dry-run and --all ask for opposite runs -- pick one" >&2
	exit 2
fi
if [ "$SAW_ALL" -eq 1 ]; then
	RUN_MODE="all"
	RUN_MODE_LABEL="all (--all)"
elif [ "$SAW_DRY_RUN" -eq 1 ]; then
	RUN_MODE="dry"
	RUN_MODE_LABEL="dry-run (--dry-run)"
fi

SYSROOT="${AVALUE_ACCEPTANCE_SYSFS_ROOT:-}"

if [ -z "$SYSROOT" ] && [ "${EUID:-$(id -u)}" -ne 0 ]; then
	echo "error: this test reads and writes device nodes -- run it as root." >&2
	exit 2
fi

# ---------------------------------------------------------------- results ---
# One row per check: STATUS<TAB>AREA<TAB>WHAT<TAB>OBSERVED. STATUS is one of
# PASS, FAIL, SKIP or INFO; only FAIL moves the verdict.
RESULTS=()
FAIL_COUNT=0
PASS_COUNT=0
SKIP_COUNT=0

record() {
	# args: status, area, what, observed
	RESULTS+=("$1	$2	$3	$4")
	case "$1" in
	PASS) PASS_COUNT=$((PASS_COUNT + 1)) ;;
	FAIL) FAIL_COUNT=$((FAIL_COUNT + 1)) ;;
	SKIP) SKIP_COUNT=$((SKIP_COUNT + 1)) ;;
	esac
	printf '%-4s [%s] %s -- %s\n' "$1" "$2" "$3" "$4"
}

# ----------------------------------------------------------------- asking ---
# Three checks change what the hardware is doing: writing a PWM duty, driving
# the GPIO lines, and arming the watchdog. Each is a question rather than a
# flag, and all three are asked here, before any check runs, so an operator
# answers three times and can then leave a run that takes minutes alone.
#
# Asking up front costs one thing worth naming: a question put before discovery
# cannot say which fan channel or which watchdog it is about. The one answer
# where that detail decides the outcome -- arming a watchdog whose nowayout is
# set reboots the board -- is therefore confirmed a second time at the point of
# use, where the value that was actually read can be quoted back.
ANSWERS=""

ask_yn() {
	# args: key, question, default answer ("yes" or "no") for a bare Enter.
	# Returns 0 for yes. Every answer is remembered under its key so the report
	# can say what this run was allowed to do.
	local key="$1" question="$2" default="$3" reply="" hint="[y/N]"

	if [ "$RUN_MODE" = "all" ]; then
		ANSWERS="${ANSWERS:+$ANSWERS }$key=yes"
		printf '%s yes (--all)\n' "$question"
		return 0
	fi

	if [ "$RUN_MODE" = "dry" ]; then
		ANSWERS="${ANSWERS:+$ANSWERS }$key=$default"
		if [ "$default" = "yes" ]; then
			return 0
		fi
		return 1
	fi

	if [ "$default" = "yes" ]; then
		hint="[Y/n]"
	fi
	printf '%s %s ' "$question" "$hint"
	if ! IFS= read -r reply; then
		# End of input: there is nobody on the other end after all, so the
		# default stands and the line the operator never typed is closed.
		printf '\n'
		reply=""
	elif [ ! -t 0 ]; then
		# A terminal echoes what was typed; a pipe or a file does not, and
		# without this the next question lands on the same line as this one
		# and the transcript never shows what was answered.
		printf '%s\n' "$reply"
	fi
	case "${reply:-$default}" in
	y|Y|yes|Yes|YES)  # unchecked-ok: a case-pattern alternation, not a pipeline
		ANSWERS="${ANSWERS:+$ANSWERS }$key=yes"
		return 0
		;;
	esac
	ANSWERS="${ANSWERS:+$ANSWERS }$key=no"
	return 1
}

check() {
	# args: condition-already-evaluated (0 ok), area, what, observed
	if [ "$1" -eq 0 ]; then
		record PASS "$2" "$3" "$4"
	else
		record FAIL "$2" "$3" "$4"
	fi
}

# Reads a sysfs file, printing its contents on one line, or the empty string.
read1() {
	[ -r "$1" ] || return 1
	tr -d '\n' < "$1"
}

# ----------------------------------------------------------- dependencies ---
PKG_MANAGER=""
INSTALL_CMD=""
INSTALLED_PKGS=""

detect_pkg_manager() {
	if command -v apt-get >/dev/null 2>&1; then
		PKG_MANAGER="apt-get"
		INSTALL_CMD="apt-get install -y"
	elif command -v dnf >/dev/null 2>&1; then
		PKG_MANAGER="dnf"
		INSTALL_CMD="dnf install -y"
	elif command -v yum >/dev/null 2>&1; then
		PKG_MANAGER="yum"
		INSTALL_CMD="yum install -y"
	elif command -v zypper >/dev/null 2>&1; then
		PKG_MANAGER="zypper"
		INSTALL_CMD="zypper --non-interactive install"
	elif command -v pacman >/dev/null 2>&1; then
		PKG_MANAGER="pacman"
		INSTALL_CMD="pacman -S --noconfirm"
	fi
}

# The package that carries a tool differs per distribution family.
package_for() {
	# args: tool
	case "$1:$PKG_MANAGER" in
	gpiodetect:apt-get)                     echo "gpiod" ;;
	gpiodetect:dnf|gpiodetect:yum)          echo "libgpiod-utils" ;;  # unchecked-ok: a case-pattern alternation, not a pipeline -- same as the option parser above
	gpiodetect:zypper)                      echo "libgpiod-utils" ;;
	gpiodetect:pacman)                      echo "libgpiod" ;;
	sensors:apt-get)                        echo "lm-sensors" ;;
	sensors:dnf|sensors:yum|sensors:zypper) echo "lm_sensors" ;;  # unchecked-ok: a case-pattern alternation, not a pipeline -- same as the option parser above
	sensors:pacman)                         echo "lm_sensors" ;;
	*)                                      echo "" ;;
	esac
}

ensure_tool() {
	# args: tool, what it unlocks. Returns 0 when the tool is usable.
	local tool="$1" purpose="$2" pkg apt_updated=0

	if command -v "$tool" >/dev/null 2>&1; then
		return 0
	fi

	if [ "$DO_INSTALL" -eq 0 ]; then
		record SKIP "deps" "$tool for $purpose" "missing, and --no-install was given"
		return 1
	fi

	if [ -z "$PKG_MANAGER" ]; then
		record SKIP "deps" "$tool for $purpose" "missing, and no supported package manager was found"
		return 1
	fi

	pkg=$(package_for "$tool")
	if [ -z "$pkg" ]; then
		record SKIP "deps" "$tool for $purpose" "missing, and no package is known for it on $PKG_MANAGER"
		return 1
	fi

	echo "installing $pkg (for $tool, needed by $purpose) with $PKG_MANAGER..."
	if [ "$PKG_MANAGER" = "apt-get" ] && [ "$apt_updated" -eq 0 ]; then
		apt-get update >/dev/null 2>&1 # unchecked-ok: a stale index only matters if the install below then fails, and that failure is what this function reports
		apt_updated=1
	fi

	if ! $INSTALL_CMD "$pkg" >/dev/null 2>&1; then
		record SKIP "deps" "$tool for $purpose" "install of $pkg failed under $PKG_MANAGER"
		return 1
	fi

	if ! command -v "$tool" >/dev/null 2>&1; then
		record SKIP "deps" "$tool for $purpose" "$pkg installed but $tool is still not on PATH"
		return 1
	fi

	INSTALLED_PKGS="${INSTALLED_PKGS:+$INSTALLED_PKGS, }$pkg"
	record INFO "deps" "$tool for $purpose" "installed $pkg"
	return 0
}

# ------------------------------------------------------------ environment ---
BOARD_NAME=$(read1 "$SYSROOT"/sys/class/dmi/id/board_name || echo "unknown")
BOARD_VENDOR=$(read1 "$SYSROOT"/sys/class/dmi/id/board_vendor || echo "unknown")
BIOS_VERSION=$(read1 "$SYSROOT"/sys/class/dmi/id/bios_version || echo "unknown")
KERNEL_RELEASE=$(uname -r)
STAMP=$(date '+%Y-%m-%d %H:%M:%S %Z')
STAMP_FILE=$(date '+%Y%m%d-%H%M%S')

if [ -z "$REPORT" ]; then
	# A real board name can carry spaces and parentheses -- EMX-W880P(EMX-W880P_0B)
	# is a measured example -- and those make the report awkward to pass to any
	# later command. Keep only characters that need no quoting.
	safe_board=$(printf '%s' "$BOARD_NAME" | tr -c 'A-Za-z0-9._-' '_')  # unchecked-ok: tr cannot fail on a here-string of an already-read value, and an empty result still yields a usable default name
	REPORT="./avalue-acceptance-${safe_board}-${STAMP_FILE}.md"
fi

echo "Avalue board acceptance test"
echo "  board  : $BOARD_VENDOR $BOARD_NAME"
echo "  kernel : $KERNEL_RELEASE"
echo "  report : $REPORT"
echo
# A run with no terminal cannot be asked anything, and a prompt nobody can see
# is a hang rather than a default -- so it takes the defaults and says it did.
# Under a fixture tree the questions stay live: that is how the self-test drives
# an answer through without a terminal of its own.
if [ "$RUN_MODE" = "ask" ] && [ ! -t 0 ] && [ -z "$SYSROOT" ]; then
	RUN_MODE="dry"
	RUN_MODE_LABEL="dry-run (no terminal to ask on)"
	record INFO "run" "questions were not asked" "stdin is not a terminal, so every question took its default and nothing touched hardware state -- the same run as --dry-run. Use --all to answer yes to everything without asking."
fi

if [ "$RUN_MODE" = "ask" ]; then
	echo "Three checks change what this board is doing. Answer them now and the rest"
	echo "of the run needs nothing from you. Enter alone takes the default in [ ]."
	echo "(--dry-run takes every default without asking; --all answers yes to all.)"
	echo
fi

if ask_yn pwm-write "Write a duty to a fan PWM channel, check a fan follows it, then restore it?" no; then
	DO_PWM_WRITE=1
fi
if ask_yn gpio-loopback "Drive the GPIO lines and read them back? Answer yes only if every line is wired to its pair -- that wiring is the one fact this test cannot measure for itself." no; then
	DO_GPIO_LOOPBACK=1
fi
if ask_yn wdt-arm "Arm the watchdog and check it counts down, refreshes on a ping, and stops?" no; then
	DO_WDT_ARM=1
fi

if [ "$RUN_MODE" = "ask" ]; then
	echo
fi

detect_pkg_manager
record INFO "deps" "package manager" "${PKG_MANAGER:-none found}"
# --------------------------------------------------------------- modules ---
# A subsystem this board does not build and a subsystem whose module simply is
# not loaded look identical from /sys/module, and reporting both as a skip is
# how a real fault hides inside a RELEASE verdict. Ask modprobe to resolve the
# module first: a name it cannot find is a subsystem this board does not carry,
# and a name it finds but cannot load is a fault worth failing over.
LOADED_MODULES=""
ABSENT_MODULES=""

# Driving a fixture tree, modprobe is taken from inside that tree, so a
# self-test can script what this board carries and what refuses to load without
# any chance of loading a real module on the machine running the test. A fixture
# with no modprobe of its own carries no drivers at all.
MODPROBE="modprobe"
MODINFO="modinfo"
# Where a locally built <module>.ko would sit. Normally the repository this
# script was run from; under a fixture, a scratch dir the self-test fills.
BUILD_DIR="$REPO_ROOT"
if [ -n "$SYSROOT" ]; then
	MODPROBE="$SYSROOT/bin/modprobe"
	MODINFO="$SYSROOT/bin/modinfo"
	BUILD_DIR="$SYSROOT/build"
fi

module_available() {
	# args: module name. True when this board carries the module at all.
	[ -n "$SYSROOT" ] && [ ! -x "$MODPROBE" ] && return 1
	"$MODPROBE" --dry-run "$1" >/dev/null 2>&1
}

# hwm, gpio, wdt and misc are generic enough that something else could answer
# to them, and every check below trusts the name. Confirm the module actually
# holding it is one of ours, and say what it is when it is not. Each driver
# declares its own MODULE_DESCRIPTION ("... for Avalue boards") and a
# MODULE_VERSION, and a loaded module -- as opposed to a built-in that merely
# has a /sys/module entry -- has an initstate.
confirm_module_identity() {
	# args: module name
	local m="$1" state ver src desc path detail
	local built built_src built_desc name_src

	state=$(read1 "$SYSROOT/sys/module/$m/initstate" || echo "")
	ver=$(read1 "$SYSROOT/sys/module/$m/version" || echo "")
	src=$(read1 "$SYSROOT/sys/module/$m/srcversion" || echo "")
	desc=$("$MODINFO" -F description "$m" 2>/dev/null)
	path=$("$MODINFO" -n "$m" 2>/dev/null)
	detail="initstate=${state:-none} version=${ver:-none} srcversion=${src:-none} description=${desc:-none} path=${path:-unknown}"

	if [ -z "$state" ]; then
		# No initstate means nothing was inserted under this name: a built-in
		# with a /sys/module entry looks exactly like a loaded module to a
		# plain directory test, and every check after this would trust it.
		record FAIL "modules" "$m is a loaded module" "/sys/module/$m has no initstate, so nothing was inserted under this name -- $detail"
		return 1
	fi

	if [ -n "$SYSROOT" ] && [ ! -x "$MODINFO" ]; then
		record INFO "modules" "$m identity" "no modinfo available to describe it -- $detail"
		return 0
	fi

	# modinfo can only be asked about a NAME, and it answers from
	# /lib/modules -- so it describes whatever is installed under that name,
	# which need not be what is actually resident. A module inserted straight
	# from a build tree with insmod is not installed at all, and one whose
	# name is also used by a module that ships with the kernel resolves to
	# that one. Either way the answer is about a different file, and treating
	# it as a statement about the resident module is how this check produced
	# confident wrong verdicts.
	#
	# srcversion is what settles it. modpost computes it from the module's
	# own source and the kernel writes it into /sys/module/<m>/srcversion, so
	# two things carrying the same srcversion were built from the same
	# source. Comparing the resident module's against a candidate FILE
	# identifies it outright, with no reliance on where it was installed.

	# The build tree this test was run from is the first candidate: a driver
	# insmod-ed from the repository is exactly the bench workflow.
	built="$BUILD_DIR/$m.ko"
	if [ -n "$src" ] && [ -f "$built" ]; then
		built_src=$("$MODINFO" -F srcversion "$built" 2>/dev/null)
		if [ -n "$built_src" ] && [ "$built_src" = "$src" ]; then
			built_desc=$("$MODINFO" -F description "$built" 2>/dev/null)
			case "$built_desc" in
			*"for Avalue boards"*)
				record PASS "modules" "$m is the Avalue driver" "the resident module was built from $built -- srcversion=$src version=${ver:-none} description=$built_desc"
				return 0
				;;
			esac
		fi
	fi

	# Otherwise the installed module of that name is a candidate, but only
	# once its srcversion says the resident module really is that file.
	name_src=$("$MODINFO" -F srcversion "$m" 2>/dev/null)
	if [ -n "$src" ] && [ -n "$name_src" ] && [ "$name_src" = "$src" ]; then
		case "$desc" in
		*"for Avalue boards"*)
			record PASS "modules" "$m is the Avalue driver" "$detail"
			return 0
			;;
		esac
		case "$path" in
		*/kernel/*)
			# The name is taken by a module shipped with the kernel itself, so
			# modprobe resolves it long before anything of ours is considered.
			record FAIL "modules" "$m is the Avalue driver" "the name '$m' is taken by a module that ships with the kernel, and that is the module now resident -- $detail"
			;;
		*)
			record FAIL "modules" "$m is the Avalue driver" "the module resident as '$m' is not an Avalue driver -- $detail"
			;;
		esac
		return 1
	fi

	# Nothing on this machine can speak for the resident module. That is not
	# evidence it is the wrong one -- it is the absence of evidence either
	# way, so the honest report is missing coverage rather than a fault, and
	# the subsystem below is left untested rather than trusted.
	if [ -z "$src" ]; then
		record SKIP "modules" "$m identity is confirmed" "/sys/module/$m has no srcversion, so the resident module cannot be matched to any file -- $detail"
	elif [ -f "$built" ]; then
		record SKIP "modules" "$m identity is confirmed" "a build of $m.ko exists in $BUILD_DIR but was built from different source than the resident module -- rebuild and reinsert it, or install it -- $detail"
	else
		record SKIP "modules" "$m identity is confirmed" "the resident $m module is not installed under /lib/modules and no $m.ko was found in $BUILD_DIR, so nothing here can say what it is -- run this test from the tree it was built in, or 'sudo make install' -- $detail"
	fi
	return 1
}

ensure_module() {
	# args: module name. Returns 0 when the module is loaded and usable.
	local m="$1" err

	if [ -d "$SYSROOT/sys/module/$m" ]; then
		# A resident module that turns out not to be ours leaves this
		# subsystem untested, so it belongs in the not-exercised list just
		# like one that could not be loaded at all. Without this the
		# subsystem drops out of the report's own summary of what was
		# missed, which is the one place an operator looks to find out
		# what this run did not cover.
		if ! confirm_module_identity "$m"; then
			ABSENT_MODULES="${ABSENT_MODULES:+$ABSENT_MODULES }$m"
			return 1
		fi
		LOADED_MODULES="${LOADED_MODULES:+$LOADED_MODULES }$m"
		return 0
	fi

	if ! module_available "$m"; then
		# What was actually observed is narrower than "this board has no such
		# subsystem": modprobe resolves against the INSTALLED modules, so all
		# this says is that no $m module is installed here. A board whose
		# configuration builds $m but which was never `make install`ed lands
		# in exactly this branch, and calling that "not carried" would hide
		# the very gap this test exists to find. So report the observation,
		# and record it as a skip -- missing coverage -- not as information.
		if [ -f "$REPO_ROOT/$m.ko" ]; then
			record SKIP "modules" "$m is available to load" "$m.ko is built in $REPO_ROOT but not installed -- run 'sudo make install' so this subsystem can be tested"
		else
			record SKIP "modules" "$m is available to load" "no $m module is installed on this machine, so modprobe cannot resolve it -- this board may not build $m, or it may not have been installed"
		fi
		ABSENT_MODULES="${ABSENT_MODULES:+$ABSENT_MODULES }$m"
		return 1
	fi

	if [ "$DO_LOAD" -eq 0 ]; then
		record SKIP "modules" "$m is loaded" "this board carries $m but it is not loaded, and --no-load was given"
		ABSENT_MODULES="${ABSENT_MODULES:+$ABSENT_MODULES }$m"
		return 1
	fi

	echo "loading the $m driver..."
	if err=$("$MODPROBE" "$m" 2>&1); then
		if confirm_module_identity "$m"; then
			record PASS "modules" "$m loads" "the Avalue $m driver was not resident and loaded on request"
			LOADED_MODULES="${LOADED_MODULES:+$LOADED_MODULES }$m"
			return 0
		fi

		# This test inserted a module, and it is not ours. Leaving an
		# unrelated driver resident on a board under test is a side effect
		# nobody asked for, so put the machine back the way it was found.
		if "$MODPROBE" -r "$m" >/dev/null 2>&1; then
			record INFO "modules" "$m removed again" "this test loaded it, it was not confirmed to be an Avalue driver, so it was unloaded again"
		else
			record FAIL "modules" "$m removed again" "this test loaded a module it could not confirm as an Avalue driver and could not unload it -- it is still resident, run 'modprobe -r $m'"
		fi
		ABSENT_MODULES="${ABSENT_MODULES:+$ABSENT_MODULES }$m"
		return 1
	fi

	# The module exists for this board and refused to load. That is a fault,
	# not missing coverage.
	record FAIL "modules" "$m loads" "this board carries $m but modprobe refused it: ${err:-no message}"
	ABSENT_MODULES="${ABSENT_MODULES:+$ABSENT_MODULES }$m"
	return 1
}

HAVE_HWM=0; ensure_module hwm && HAVE_HWM=1
HAVE_GPIO=0; ensure_module gpio && HAVE_GPIO=1
HAVE_MISC=0; ensure_module misc && HAVE_MISC=1
# wdt is loaded here with the rest, but is exercised last -- see the WDT
# section at the end of this file for why.
HAVE_WDT=0; ensure_module wdt && HAVE_WDT=1

if [ -z "$LOADED_MODULES" ]; then
	record FAIL "modules" "at least one Avalue driver is loaded" "none of hwm, gpio, wdt or misc is loaded, and none could be loaded"
else
	record PASS "modules" "loaded Avalue drivers" "$LOADED_MODULES"
fi
[ -n "$ABSENT_MODULES" ] && record INFO "modules" "subsystems not exercised" "$ABSENT_MODULES -- see the skips above for why each one could not be"

# ------------------------------------------------------------------- HWM ---
# This driver is the only hwmon device that carries in/, temp/, fan/ and pwm/
# subdirectory groups, so those identify it without knowing the chipset name.
HWMON_DIR=""
for d in "$SYSROOT"/sys/class/hwmon/hwmon*; do
	[ -d "$d/pwm" ] || [ -d "$d/temp" ] || [ -d "$d/fan" ] || [ -d "$d/in" ] || continue
	HWMON_DIR="$d"
	break
done

is_int() {
	printf '%s' "${1:-}" | grep -Eq '^-?[0-9]+$'  # unchecked-ok: this pipeline's exit status IS is_int()'s return value, and every caller tests it; printf writing to a pipe cannot meaningfully fail
}

# A raw hwmon number is millivolts, millidegrees, RPM or percent depending on
# the channel it came from. Print it as the quantity it is, so the report can
# be read without the units table.
human_value() {
	# args: type, raw value
	local kind="$1" v="$2" whole frac sign=""
	case "$kind" in
	in)
		[ "$v" -lt 0 ] && sign="-" && v=$((-v))
		whole=$((v / 1000))
		frac=$(printf '%03d' $((v % 1000)))
		printf '%s mV (%s%s.%s V)' "$2" "$sign" "$whole" "$frac"
		;;
	temp)
		[ "$v" -lt 0 ] && sign="-" && v=$((-v))
		whole=$((v / 1000))
		frac=$(((v % 1000) / 100))
		printf '%s m°C (%s%s.%s °C)' "$2" "$sign" "$whole" "$frac"
		;;
	fan)  printf '%s RPM' "$2" ;;
	pwm)  printf '%s%% duty' "$2" ;;
	*)    printf '%s' "$2" ;;
	esac
}

hwm_value_ok() {
	# args: type, raw value. Plausibility only -- a reading far outside these
	# bounds means the register or its scaling is wrong, not that the board is.
	local kind="$1" v="$2"
	case "$kind" in
	in)   [ "$v" -ge 0 ] && [ "$v" -le 30000 ] ;;   # mV
	temp) [ "$v" -ge -40000 ] && [ "$v" -le 125000 ] ;;  # m degC
	fan)  [ "$v" -ge 0 ] && [ "$v" -le 30000 ] ;;   # RPM
	pwm)  [ "$v" -ge 0 ] && [ "$v" -le 100 ] ;;     # percent, this driver's range
	*)    return 1 ;;
	esac
}

if [ -z "$HWMON_DIR" ]; then
	# HAVE_HWM is what the module step already settled -- including whether the
	# module answering to that name really is ours. Re-deriving it from a path
	# spelled out again here means two places have to agree about one name, and
	# they did not: a rename left this branch probing a name nothing used, so it
	# reported "not loaded" about a driver the same run had confirmed one line
	# earlier. One place decides.
	if [ "$HAVE_HWM" -ne 0 ]; then
		record FAIL "hwm" "hwmon device registered" "the hwm module is loaded but no hwmon device carries this driver's subdirectory groups"
	else
		record SKIP "hwm" "hwmon device" "the hwm module is not loaded on this board"
	fi
else
	HWM_CHIP=$(read1 "$HWMON_DIR/name" || echo "?")
	record PASS "hwm" "hwmon device registered" "$HWMON_DIR (name=$HWM_CHIP)"

	# Every channel the driver published, read through the flat path.
	for kind in in temp fan pwm; do
		found=0
		for f in "$HWMON_DIR/$kind"[0-9]*; do
			[ -e "$f" ] || continue
			base=$(basename "$f")
			case "$base" in
			*_label|*_alarm|*_min|*_max|*_crit) continue ;;  # unchecked-ok: a case-pattern alternation, not a pipeline
			esac
			found=$((found + 1))
			if ! raw=$(read1 "$f"); then
				record FAIL "hwm" "$base is readable" "read failed"
				continue
			fi
			if ! printf '%s' "$raw" | grep -Eq '^-?[0-9]+$'; then
				record FAIL "hwm" "$base reads a number" "got '$raw'"
				continue
			fi
			if hwm_value_ok "$kind" "$raw"; then
				record PASS "hwm" "$base reads a plausible value" "$(human_value "$kind" "$raw")"
			else
				record FAIL "hwm" "$base reads a plausible value" "$(human_value "$kind" "$raw") is outside the sane range for a $kind channel"
			fi
		done
		[ "$found" -eq 0 ] && record INFO "hwm" "$kind channels" "none published on this board"
	done

	# The two layouts have to name the same channels. A subdirectory file with
	# no flat file of that name means the numbering has drifted apart again.
	for kind in in temp fan pwm; do
		[ -d "$HWMON_DIR/$kind" ] || continue
		for f in "$HWMON_DIR/$kind"/*; do
			[ -e "$f" ] || continue
			base=$(basename "$f")
			case "$base" in
			*_label)
				sub_label=$(read1 "$f")
				if [ ! -e "$HWMON_DIR/$base" ]; then
					# PWM labels exist only in the subdirectory: the kernel's
					# hwmon core has no PWM label attribute at all.
					if [ "$kind" = "pwm" ]; then
						record INFO "hwm" "$kind/$base has no flat twin" "expected -- the hwmon core has no PWM label attribute"
					else
						record FAIL "hwm" "$kind/$base has a flat twin" "no $HWMON_DIR/$base"
					fi
					continue
				fi
				flat_label=$(read1 "$HWMON_DIR/$base")
				if [ "$sub_label" = "$flat_label" ]; then
					record PASS "hwm" "$kind/$base and $base name one channel" "$sub_label"
				else
					record FAIL "hwm" "$kind/$base and $base name one channel" "subdirectory says '$sub_label', flat says '$flat_label'"
				fi
				;;
			*)
				if [ ! -e "$HWMON_DIR/$base" ]; then
					record FAIL "hwm" "$kind/$base has a flat twin" "no $HWMON_DIR/$base -- the two layouts number channels differently"
				fi
				;;
			esac
		done
	done

	# PWM channels, and what each one drives.
	PWM_FAN_FILE=""
	PWM_FAN_LABEL=""
	for f in "$HWMON_DIR/pwm"/pwm[0-9]*; do
		[ -e "$f" ] || continue
		base=$(basename "$f")
		case "$base" in *_label) continue ;; esac
		label=$(read1 "$HWMON_DIR/pwm/${base}_label" || echo "")
		if [ -z "$label" ]; then
			record FAIL "hwm" "$base says what it drives" "no readable ${base}_label"
			continue
		fi
		record PASS "hwm" "$base says what it drives" "$label"
		# Remember one fan channel for the optional write test. Backlight
		# channels are left alone: writing one dims the panel.
		if [ -z "$PWM_FAN_FILE" ] && printf '%s' "$label" | grep -qi 'fan'; then  # unchecked-ok: the 'if' reads the pipeline's last stage, grep, which is exactly the test being made; printf into a pipe cannot meaningfully fail
			PWM_FAN_FILE="$HWMON_DIR/$base"
			PWM_FAN_LABEL="$label"
		fi
	done

	if [ "$DO_PWM_WRITE" -eq 0 ]; then
		record SKIP "hwm" "PWM write and read-back" "not run -- the PWM write question was answered no, so no duty was written"
	elif [ -z "$PWM_FAN_FILE" ]; then
		record SKIP "hwm" "PWM write and read-back" "no PWM channel is labelled as a fan on this board"
	else
		orig=$(read1 "$PWM_FAN_FILE")
		target=60
		if is_int "$orig"; then
			[ "$orig" -ge 55 ] && [ "$orig" -le 65 ] && target=40
		else
			record FAIL "hwm" "$PWM_FAN_LABEL current duty is readable" "reads '$orig', not a number -- restoring it after the write is not possible"
			orig=""
		fi

		fan_before=""
		for f in "$HWMON_DIR"/fan[0-9]*_input; do
			[ -e "$f" ] && fan_before=$(read1 "$f") && FAN_INPUT="$f" && break
		done

		if printf '%s' "$target" > "$PWM_FAN_FILE" 2>/dev/null; then
			sleep 3
			back=$(read1 "$PWM_FAN_FILE")
			if ! is_int "$back"; then
				record FAIL "hwm" "$PWM_FAN_LABEL write/read-back" "wrote $target, and the read back is not a number: '$back'"
			else
				# 0-100 percent is stored as a 0-255 byte, so a round trip
				# loses up to one percent.
				delta=$((back - target))
				[ "$delta" -lt 0 ] && delta=$((-delta))
				if [ "$delta" -le 1 ]; then
					record PASS "hwm" "$PWM_FAN_LABEL write/read-back" "wrote $target%, read $back%"
				else
					record FAIL "hwm" "$PWM_FAN_LABEL write/read-back" "wrote $target%, read $back% -- the write did not land where the read looks"
				fi
			fi

			# The same channel through the other path has to accept the same
			# write -- that is the whole point of the two layouts agreeing.
			sub_twin="$HWMON_DIR/pwm/$(basename "$PWM_FAN_FILE")"
			if [ -w "$sub_twin" ] && printf '%s' "$target" > "$sub_twin" 2>/dev/null; then
				sleep 1
				back2=$(read1 "$PWM_FAN_FILE")
				if ! is_int "$back2"; then
					record FAIL "hwm" "$PWM_FAN_LABEL is one channel from both paths" "wrote $target% to the subdirectory path, and the flat file does not read a number: '$back2'"
				else
					delta2=$((back2 - target))
					[ "$delta2" -lt 0 ] && delta2=$((-delta2))
					if [ "$delta2" -le 1 ]; then
						record PASS "hwm" "$PWM_FAN_LABEL is one channel from both paths" "wrote $target% to pwm/$(basename "$PWM_FAN_FILE"), the flat file reads $back2%"
					else
						record FAIL "hwm" "$PWM_FAN_LABEL is one channel from both paths" "wrote $target% to the subdirectory path, the flat file reads $back2% -- the two paths are not one channel"
					fi
				fi
			else
				record SKIP "hwm" "$PWM_FAN_LABEL is one channel from both paths" "no writable subdirectory twin"
			fi

			if is_int "$fan_before"; then
				sleep 5
				fan_after=$(read1 "$FAN_INPUT")
				if ! is_int "$fan_after"; then
					record FAIL "hwm" "fan speed follows duty" "the tachometer stopped reading a number: '$fan_after'"
					fan_after="$fan_before"
				fi
				if [ "$fan_before" -eq 0 ] && [ "$fan_after" -eq 0 ]; then
					record INFO "hwm" "fan speed follows duty" "tachometer read 0 RPM before and after -- no fan connected, or it does not report"
				elif [ "$fan_before" -eq "$fan_after" ]; then
					record INFO "hwm" "fan speed follows duty" "$fan_before RPM unchanged across a duty change of $((target - orig)) points -- may be a 3-wire fan with no speed control"
				else
					record PASS "hwm" "fan speed follows duty" "$fan_before RPM at ${orig}%, $fan_after RPM at ${target}%"
				fi
			else
				record SKIP "hwm" "fan speed follows duty" "no fan tachometer channel on this board"
			fi

			if [ -z "$orig" ]; then
				record FAIL "hwm" "$PWM_FAN_LABEL restored" "the original duty was never readable, so the channel is left at ${target}%"
			elif printf '%s' "$orig" > "$PWM_FAN_FILE" 2>/dev/null; then
				record INFO "hwm" "$PWM_FAN_LABEL restored" "back to ${orig}%"
			else
				record FAIL "hwm" "$PWM_FAN_LABEL restored" "could not write the original ${orig}% back -- the channel is left at ${target}%"
			fi
		else
			record FAIL "hwm" "$PWM_FAN_LABEL write/read-back" "write of $target to $PWM_FAN_FILE failed"
		fi
	fi
fi
# ------------------------------------------------------------------ GPIO ---
if [ "$HAVE_GPIO" -eq 0 ]; then
	record SKIP "gpio" "gpiochip" "the gpio driver is not loaded on this board"
elif ! ensure_tool gpiodetect "the GPIO checks"; then
	record SKIP "gpio" "gpiochip" "gpiodetect is unavailable"
else
	GPIO_DETECT_ALL=$(gpiodetect 2>&1)
	GPIO_LINE=$(printf '%s\n' "$GPIO_DETECT_ALL" | grep '\[gpio\]' | head -1)  # unchecked-ok: an empty result is caught by the "[ -z ]" test on the next line, which records the failure
	if [ -z "$GPIO_LINE" ]; then
		# "no chip labelled gpio" leaves the next question unanswered: what IS
		# there? A driver that loaded and registered nothing, a chip under a
		# different label, and a gpiodetect whose output this does not parse
		# are three different faults and the report has to tell them apart.
		record FAIL "gpio" "a gpiochip labelled 'gpio' is present" "the gpio driver is loaded but no chip in gpiodetect's output is labelled [gpio]"
		record INFO "gpio" "gpiodetect listed" "$(printf '%s' "${GPIO_DETECT_ALL:-nothing}" | tr '\n' ';')"  # unchecked-ok: formats already-captured output for one report cell; the capture above is what this reports on
		gpio_devnodes=$(ls -1 /dev/gpiochip* 2>/dev/null | tr '\n' ' ')  # unchecked-ok: an absent glob is itself the finding, recorded as "none" on the next line
		record INFO "gpio" "character devices present" "${gpio_devnodes:-none under /dev}"
		# Whatever the kernel exposes about each chip, read out as found --
		# no assumption about which of these paths this kernel provides.
		gpio_labels=""
		for gd in /sys/bus/gpio/devices/* /sys/class/gpio/gpiochip*; do
			[ -e "$gd" ] || continue
			gl=$(cat "$gd/label" 2>/dev/null || echo "?")
			gpio_labels="${gpio_labels:+$gpio_labels }$(basename "$gd")=$gl"
		done
		record INFO "gpio" "chip labels in sysfs" "${gpio_labels:-no gpiochip found under /sys/bus/gpio/devices or /sys/class/gpio}"
		# log_info() is an unconditional pr_info(), so the driver's own
		# initialisation line reaches dmesg on a plain build as well as a
		# debug one -- its absence is evidence, not a build difference.
		gpio_dmesg=$(dmesg 2>/dev/null | grep -iE "GPIO driver initial|GPIO chip|avalue" | tail -3 | tr '\n' ';')  # unchecked-ok: an empty result records as "nothing" on the next line, which is itself the finding
		record INFO "gpio" "what the driver said in dmesg" "${gpio_dmesg:-nothing from this driver -- and log_info is an unconditional pr_info, so silence here means the code path did not run (or the ring buffer has wrapped)}"
	else
		GPIO_CHIP=$(printf '%s' "$GPIO_LINE" | grep -oE 'gpiochip[0-9]+')  # unchecked-ok: feeds the gpioget loop below, whose own failure is recorded per line
		GPIO_LINES=$(printf '%s' "$GPIO_LINE" | grep -oE '[0-9]+ lines' | grep -oE '^[0-9]+')  # unchecked-ok: validated by the numeric test immediately below
		record PASS "gpio" "a gpiochip labelled 'gpio' is present" "$GPIO_CHIP with ${GPIO_LINES:-?} lines"

		if ! is_int "$GPIO_LINES" || [ "$GPIO_LINES" -le 0 ]; then
			record FAIL "gpio" "the line count is readable" "gpiodetect reported '${GPIO_LINES:-}'"
		else
			if [ $((GPIO_LINES % 2)) -eq 0 ]; then
				record PASS "gpio" "the line count is even" "$GPIO_LINES lines pair up for a loopback test"
			else
				record FAIL "gpio" "the line count is even" "$GPIO_LINES lines cannot be paired for a loopback test"
			fi

			# Read every line, and name the ones that refuse rather than only
			# counting them -- a single stuck line is what this finds.
			unreadable=""
			gpio_values=""
			for ((i = 0; i < GPIO_LINES; i++)); do
				if v=$(gpioget "$GPIO_CHIP" "$i" 2>/dev/null); then
					gpio_values="${gpio_values:+$gpio_values }$i=$v"
				else
					unreadable="${unreadable:+$unreadable }$i"
				fi
			done
			if [ -z "$unreadable" ]; then
				record PASS "gpio" "every line reads back" "all $GPIO_LINES lines readable"
			else
				record FAIL "gpio" "every line reads back" "line(s) $unreadable refused a read"
			fi
			record INFO "gpio" "line values at rest" "${gpio_values:-none read}"

			# The pin map belongs in the report: a loopback failure is read
			# against it, and it says which line is which on this board.
			if command -v gpioinfo >/dev/null 2>&1; then
				gpio_map=$(gpioinfo "$GPIO_CHIP" 2>/dev/null | tr '\n' ';' | tr -s ' ')  # unchecked-ok: informational only -- an empty map records as "unavailable" on the next line
				record INFO "gpio" "line map" "${gpio_map:-gpioinfo returned nothing}"
			fi
		fi

		if [ "$DO_GPIO_LOOPBACK" -eq 0 ]; then
			record SKIP "gpio" "loopback drive test" "not run -- the GPIO loopback question was answered no, so the lines were read but never driven"
		elif [ -n "$SYSROOT" ]; then
			# The loopback shells out to a script that drives real lines. A
			# fixture tree has none, and running it here would reach past the
			# fixture to whatever chip this machine actually has.
			record SKIP "gpio" "loopback drive test" "not run against a fixture tree -- driving lines needs the real chip"
		elif [ ! -f "$REPO_ROOT/test/test-gpio.sh" ]; then
			record SKIP "gpio" "loopback drive test" "test/test-gpio.sh is not in this tree"
		else
			# Keep the failing pair, not just the failure: test-gpio.sh names
			# the pin it could not drive, and that line is the whole finding.
			gpio_out=$(bash "$REPO_ROOT/test/test-gpio.sh" 2>&1)
			gpio_rc=$?
			if [ "$gpio_rc" -eq 0 ]; then
				record PASS "gpio" "loopback drive test" "every line pair drove and read both values"
			elif [ "$gpio_rc" -eq 2 ]; then
				# Not one pair followed, in either direction. Naming a single
				# failing pair here would report the first symptom as though it
				# were the finding; the pattern across ALL of them is the
				# finding, and it narrows to one of two things. The operator
				# supplied the one this test cannot measure when they answered
				# the wiring question, so say what their answer implies rather
				# than leaving the reader to work it out.
				record FAIL "gpio" "loopback drive test" "no line pair responded, in either direction -- with the loopback wiring fitted, as the question above was answered, that leaves the driver's output path not reaching the pins; if the wiring is in fact absent, that is the explanation instead and this run cannot tell which"
			else
				gpio_why=$(printf '%s' "$gpio_out" | grep -iE 'mismatch|error|fail' | head -1)  # unchecked-ok: an empty extraction falls back to the generic message on the next line
				record FAIL "gpio" "loopback drive test" "${gpio_why:-test/test-gpio.sh failed; run it directly for the failing pair}"
			fi

			if [ "$gpio_rc" -ne 0 ]; then
				# Which pairs passed, which failed, and what each one actually
				# read is the whole basis for the verdict above -- and the hold
				# mode the loopback test reports rules out a third explanation
				# that used to look identical. Keeping only a one-line summary
				# leaves the reader unable to check any of it.
				record INFO "gpio" "what the loopback test reported" "$(printf '%s' "$gpio_out" | tr '\n' ';')"  # unchecked-ok: reformats already-captured output for one report cell; the capture above is what this reports on
			fi

			# Which libgpiod is installed decides whether gpioset holds a line
			# while it is read, so it belongs in the report on a pass as well --
			# a loopback that passed says which tooling it passed with.
			gpio_hold=$(printf '%s' "$gpio_out" | grep -i 'gpioset hold' | head -1)  # unchecked-ok: an empty extraction is skipped by the "[ -n ]" test on the next line
			if [ -n "$gpio_hold" ]; then
				record INFO "gpio" "gpioset hold mode" "$gpio_hold"
			fi
		fi
	fi
fi

# ------------------------------------------------------------------ MISC ---
MISC_DIR="$SYSROOT/sys/class/misc/misc"
if [ "$HAVE_MISC" -eq 0 ]; then
	record SKIP "misc" "misc channels" "the misc driver is not loaded on this board"
elif [ ! -d "$MISC_DIR" ]; then
	record FAIL "misc" "the misc device is registered" "the misc driver is loaded but $MISC_DIR does not exist"
else
	record PASS "misc" "the misc device is registered" "$MISC_DIR"

	# misc_register() creates the character node the ioctl interface is
	# reached through; the sysfs group beside it is the per-channel view.
	if [ -n "$SYSROOT" ]; then
		record INFO "misc" "/dev/misc" "not checked against a fixture tree"
	elif [ -c /dev/misc ]; then
		record PASS "misc" "/dev/misc is a character device" "$(ls -l /dev/misc 2>/dev/null | tr -s ' ' | cut -d' ' -f1,5,6)"  # unchecked-ok: cosmetic detail for the report; the [ -c ] test above is what decides this check
	else
		record FAIL "misc" "/dev/misc is a character device" "the misc driver is loaded but /dev/misc is missing or not a character device"
	fi

	misc_found=0
	for f in "$MISC_DIR"/*; do
		[ -f "$f" ] || continue
		base=$(basename "$f")
		case "$base" in dev|uevent|power|subsystem) continue ;; esac  # unchecked-ok: a case-pattern alternation, not a pipeline
		misc_found=$((misc_found + 1))

		if v=$(cat "$f" 2>/dev/null); then
			record PASS "misc" "$base reads" "$(printf '%s' "$v" | tr '\n' ' ')"  # unchecked-ok: formats an already-captured value for one report cell; the read itself was checked by the "if" above
			continue
		fi

		# A write-only channel answers EACCES by design -- the driver returns
		# it when the channel declares no read. Anything else is a fault.
		why=$(cat "$f" 2>&1 >/dev/null)  # unchecked-ok: this read is expected to fail; its stderr text is the finding, and the classification below is what acts on it
		case "$why" in
		*"Permission denied"*)
			record INFO "misc" "$base" "write-only channel -- reads answer EACCES by design"
			;;
		*)
			record FAIL "misc" "$base reads" "read failed: ${why:-no message}"
			;;
		esac
	done
	[ "$misc_found" -eq 0 ] && record INFO "misc" "misc channels" "the driver is loaded but published no channels"
fi

# ---------------------------------------------------------------- report ---
# Writing the report is a function, not the last thing that happens, because
# the watchdog section below can end the machine. It is called once before the
# watchdog is armed -- so a board that resets still leaves a report behind
# carrying everything measured up to that moment -- and once at the end.
write_report() {
	# args: [a note to append, e.g. what was about to happen]
	verdict="RELEASE"
	[ "$FAIL_COUNT" -gt 0 ] && verdict="DEBUG"

	{
		echo "# Avalue board acceptance report"
		echo
		echo "| | |"
		echo "|---|---|"
		echo "| Board | $BOARD_VENDOR $BOARD_NAME |"
		echo "| BIOS | $BIOS_VERSION |"
		echo "| Kernel | $KERNEL_RELEASE |"
		echo "| Drivers loaded | ${LOADED_MODULES:-none} |"
		echo "| Run at | $STAMP |"
		echo "| Mode | $RUN_MODE_LABEL |"
		echo "| Answers | ${ANSWERS:-nothing was asked} |"
		echo "| Options | load-modules=$DO_LOAD install-deps=$DO_INSTALL |"
		echo "| Packages installed | ${INSTALLED_PKGS:-none} |"
		echo
		echo "## Verdict: $verdict"
		echo
		if [ "$verdict" = "RELEASE" ]; then
			echo "Every check that ran passed. $PASS_COUNT passed, $SKIP_COUNT skipped."
			if [ "$SKIP_COUNT" -gt 0 ]; then
				echo
				echo "**A skip is missing coverage, not a pass.** These did not run — read them"
				echo "before signing the board off:"
				echo
				for row in "${RESULTS[@]}"; do
					IFS=$'\t' read -r st area what observed <<<"$row"
					[ "$st" = "SKIP" ] || continue
					echo "- **[$area] $what** — $observed"
				done
			fi
		else
			echo "**$FAIL_COUNT check(s) failed.** $PASS_COUNT passed, $SKIP_COUNT skipped."
			echo
			echo "Failing checks, each with what was observed:"
			echo
			for row in "${RESULTS[@]}"; do
				IFS=$'\t' read -r st area what observed <<<"$row"
				[ "$st" = "FAIL" ] || continue
				echo "- **[$area] $what** — $observed"
			done
		fi
		echo
		echo "## Checks"
		echo
		echo "| Status | Area | Check | Observed |"
		echo "|---|---|---|---|"
		for row in "${RESULTS[@]}"; do
			IFS=$'\t' read -r st area what observed <<<"$row"
			echo "| $st | $area | $what | $observed |"
		done
		echo
		echo "## Reading this report"
		echo
		echo "- **PASS** — the check ran against the hardware and held."
		echo "- **FAIL** — the check ran and did not hold. Every one is listed under the verdict."
		echo "- **SKIP** — the check did not run. The observed column says what would let it."
		echo "- **INFO** — a reading recorded for the record; it does not move the verdict."
		echo
		echo "The watchdog is never armed by this test: opening \`/dev/watchdog\` starts the"
		echo "timer, and with \`nowayout\` set nothing short of a reset stops it. Everything"
		echo "reported about it comes from sysfs."
		[ -n "${1:-}" ] && printf '%s\n' "" "$1"
	} > "$REPORT"
}

# ------------------------------------------------- WDT (deliberately last) ---
# The watchdog is exercised after everything else because a real test of one
# can end the machine. Opening the character device ARMS the timer, and while
# `nowayout` is set nothing short of a reset stops it again -- so a board that
# fails the stop check reboots, taking any check that had not run yet with it.
# Running it last means a reset costs nothing already measured, and the report
# written just above is on disk before the device is ever opened.
# Two different board names meet here. DMI reports the one the BIOS carries,
# which can append a BIOS identifier -- "EMX-W880P(EMX-W880P_0B)" is a measured
# example -- while the driver builds its watchdog identity from the name in the
# board's own .conf, giving "EMX-W880P ite Watchdog". Looking for the whole DMI
# string inside that identity therefore finds nothing on any board whose DMI
# name carries a suffix, and reports a registered watchdog as missing. Match on
# the leading token instead, which is the name both agree on, anchored at the
# start and followed by a space so it can only match the identity's own board
# field and never some other driver's text.
BOARD_TOKEN=${BOARD_NAME%%(*}
BOARD_TOKEN=${BOARD_TOKEN%% *}

WDT_DIR=""
WDT_TOKEN_DIR=""
WDT_SEEN=""
for d in "$SYSROOT"/sys/class/watchdog/watchdog*; do
	[ -d "$d" ] || continue
	ident=$(read1 "$d/identity" || echo "")
	WDT_SEEN="${WDT_SEEN:+$WDT_SEEN; }$(basename "$d")=${ident:-no identity}"
	case "$ident" in
	*"$BOARD_NAME"*) [ -z "$WDT_DIR" ] && WDT_DIR="$d" ;;
	esac
	if [ -n "$BOARD_TOKEN" ] && [ -z "$WDT_TOKEN_DIR" ]; then
		case "$ident" in
		"$BOARD_TOKEN "*) WDT_TOKEN_DIR="$d" ;;
		esac
	fi
done
[ -z "$WDT_DIR" ] && WDT_DIR="$WDT_TOKEN_DIR"

# A board carries more than one watchdog often enough -- a platform one from
# the chipset alongside this driver's -- that which devices exist is worth
# reporting either way, not only when the search comes up empty.
[ -n "$WDT_SEEN" ] && record INFO "wdt" "watchdog devices present" "$WDT_SEEN"

if [ "$HAVE_WDT" -eq 0 ]; then
	record SKIP "wdt" "watchdog" "the wdt driver is not loaded on this board"
elif [ -z "$WDT_DIR" ]; then
	record FAIL "wdt" "the watchdog is registered" "the wdt driver is loaded but no watchdog identity under /sys/class/watchdog names '$BOARD_NAME' or begins with '$BOARD_TOKEN' -- found: ${WDT_SEEN:-no watchdog device at all}"
else
	record PASS "wdt" "the watchdog is registered" "$WDT_DIR (identity=$(read1 "$WDT_DIR/identity"))"
	for attr in state timeout timeleft bootstatus nowayout pretimeout; do
		[ -e "$WDT_DIR/$attr" ] || continue
		v=$(read1 "$WDT_DIR/$attr" || echo "unreadable")
		record INFO "wdt" "$attr" "$v"
	done

	wdt_timeout=$(read1 "$WDT_DIR/timeout" || echo "")
	if is_int "$wdt_timeout" && [ "$wdt_timeout" -ge 1 ]; then
		record PASS "wdt" "a timeout is configured" "$wdt_timeout s"
	else
		record FAIL "wdt" "a timeout is configured" "timeout reads '$wdt_timeout'"
	fi

	WDT_NOWAYOUT=$(read1 "$WDT_DIR/nowayout" || echo "")
	WDT_DEV="/dev/$(basename "$WDT_DIR")"

	if [ "$DO_WDT_ARM" -eq 0 ]; then
		record SKIP "wdt" "arming, countdown, ping and stop" "not run -- the watchdog question was answered no, so the timer was never started"
	elif [ "$WDT_NOWAYOUT" != "0" ] && ! ask_yn wdt-reset "nowayout reads '${WDT_NOWAYOUT:-unreadable}', so the magic-close stop cannot work and arming WILL reset this board. Arm it anyway?" no; then
		# With nowayout set, the magic-close stop below cannot work, so arming
		# is a decision to reboot this machine. That is the one answer the
		# up-front question could not put properly, because the value it turns
		# on is only known after discovery -- so it is put here, quoting what
		# was actually read. It is judged before anything touches the device,
		# so the reason a run declined to arm is the same whether or not a
		# device was there to arm.
		record SKIP "wdt" "arming, countdown, ping and stop" "refused: nowayout reads '${WDT_NOWAYOUT:-unreadable}', so arming WILL reset this board, and that was declined"
	elif [ -n "$SYSROOT" ]; then
		record SKIP "wdt" "arming, countdown, ping and stop" "not run against a fixture tree -- arming needs the real device"
	elif [ ! -c "$WDT_DEV" ]; then
		record FAIL "wdt" "the watchdog character device exists" "$WDT_DEV is missing or not a character device, so the timer cannot be exercised"
	else
		echo
		echo "*** arming the watchdog on $WDT_DEV (timeout ${wdt_timeout}s) ***"
		if [ "$WDT_NOWAYOUT" != "0" ]; then
			echo "*** nowayout is set: THIS BOARD WILL RESET when the timer expires ***"
		fi
		write_report "**A watchdog arm was in progress when this report was written.** If the board reset, everything above was measured before it; the watchdog section is incomplete by design, not missing."

		if ! exec 3>"$WDT_DEV"; then
			record FAIL "wdt" "the watchdog arms" "could not open $WDT_DEV"
		else
			sleep 1
			wdt_state=$(read1 "$WDT_DIR/state" || echo "")
			if [ "$wdt_state" = "active" ]; then
				record PASS "wdt" "the watchdog arms on open" "state=$wdt_state"
			else
				record FAIL "wdt" "the watchdog arms on open" "$WDT_DEV was opened but state reads '$wdt_state'"
			fi

			t1=$(read1 "$WDT_DIR/timeleft" || echo "")
			sleep 3
			t2=$(read1 "$WDT_DIR/timeleft" || echo "")
			if ! is_int "$t1" || ! is_int "$t2"; then
				record FAIL "wdt" "the countdown runs" "timeleft read '$t1' then '$t2'"
			elif [ "$t2" -lt "$t1" ]; then
				record PASS "wdt" "the countdown runs" "timeleft went $t1 s -> $t2 s across 3 s"
			else
				record FAIL "wdt" "the countdown runs" "timeleft went $t1 s -> $t2 s -- the timer is not counting down"
			fi

			# Any character other than the magic 'V' is a keepalive.
			printf 'A' >&3
			sleep 1
			t3=$(read1 "$WDT_DIR/timeleft" || echo "")
			if ! is_int "$t3"; then
				record FAIL "wdt" "a ping refreshes the countdown" "timeleft read '$t3' after the ping"
			elif [ "$t3" -gt "$t2" ]; then
				record PASS "wdt" "a ping refreshes the countdown" "timeleft went back up to $t3 s from $t2 s"
			else
				record FAIL "wdt" "a ping refreshes the countdown" "timeleft is $t3 s after a ping, no higher than the $t2 s before it"
			fi

			# The magic close: 'V' then close is the only stop the driver
			# advertises (WDIOF_MAGICCLOSE). What it is SUPPOSED to do here is
			# decided by nowayout, and judging both cases by the nowayout=0 rule
			# is how a board doing exactly what it documents gets failed for it.
			#
			# With nowayout set, the driver's own contract is that software
			# cannot stop the timer once it is running -- it says so at
			# initialisation. A watchdog still counting after a magic close is
			# that promise being kept, and one that stopped is the defect: a
			# timer that can be talked out of firing is no protection, and the
			# board's operator was told it could not be.
			#
			# The board still goes down either way, and the operator agreed to
			# that before arming, so the banner is printed whatever the verdict.
			printf 'V' >&3
			exec 3>&-
			sleep 2
			wdt_state=$(read1 "$WDT_DIR/state" || echo "")
			t4=$(read1 "$WDT_DIR/timeleft" || echo "?")
			if [ "$WDT_NOWAYOUT" != "0" ]; then
				if [ "$wdt_state" = "inactive" ]; then
					record FAIL "wdt" "nowayout keeps the watchdog running" "nowayout reads '$WDT_NOWAYOUT', so a magic close must NOT stop the timer -- but state reads 'inactive', so software stopped a watchdog that says it cannot be stopped"
				else
					record PASS "wdt" "nowayout keeps the watchdog running" "state=$wdt_state after a magic close, which is what nowayout='$WDT_NOWAYOUT' promises -- and it is why this board resets in about $t4 s"
				fi
				echo
				echo "*** nowayout is set, so the watchdog did not stop: this board resets in about $t4 s ***"
			elif [ "$wdt_state" = "inactive" ]; then
				record PASS "wdt" "a magic close stops the watchdog" "state=$wdt_state -- the board is not going to reset"
			else
				record FAIL "wdt" "a magic close stops the watchdog" "state reads '$wdt_state' after a magic close -- THIS BOARD RESETS IN ABOUT $t4 s"
				echo
				echo "*** the watchdog did not stop: this board resets in about $t4 s ***"
			fi
		fi
	fi
fi

write_report ""
echo
echo "report written to $REPORT"
echo "verdict: $verdict ($PASS_COUNT passed, $FAIL_COUNT failed, $SKIP_COUNT skipped)"

if [ "$verdict" = "RELEASE" ]; then
	echo "[BOARD-ACCEPTANCE]: RELEASE: every check that ran passed on $BOARD_NAME."
	exit 0
fi

echo "[BOARD-ACCEPTANCE]: DEBUG: $FAIL_COUNT check(s) failed on $BOARD_NAME -- see $REPORT."
exit 1
