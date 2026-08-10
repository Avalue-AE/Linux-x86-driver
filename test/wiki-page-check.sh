#!/bin/bash
# Reads docs/wiki/Linux-X86-API.md (or the page given as $1) and checks
# every claim the PAGE'S OWN TEXT makes -- every `/dev/<node>` path, every
# `/sys/class/<...>/` path, and every `make <target>` command it names --
# against what this driver and its Makefile really provide. Claims are
# extracted from the page's own text with sed/awk, not copied by hand into
# this script, so a future edit to the page is what a future run grades.
# See test/README.md.
#
# Ground truth comes from the source tree at run time -- a grep against the
# driver's own registration calls, and the Makefile's own `DRIVERS :=`
# line -- not from a hard-coded list of "expected paths".
#
# `make <target>` claims are read only from the page's own code: fenced
# ``` blocks and inline `...` spans. A plain-English sentence that happens
# to contain the word "make" (e.g. "a plain make, driven by...", "make
# sure...") is not a command the page is telling a customer to run, so it
# is not a claim -- only text the page itself formats as code is.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAGE="${1:-$REPO_ROOT/docs/wiki/Linux-X86-API.md}"
MAKEFILE="$REPO_ROOT/Makefile"
SRC_GPIO="$REPO_ROOT/src/drivers/gpio.c"
SRC_WDT="$REPO_ROOT/src/drivers/watchdog.c"
SRC_MISC="$REPO_ROOT/src/drivers/misc.c"
SRC_HWMON="$REPO_ROOT/src/drivers/hwmon.c"

if [ ! -f "$PAGE" ]; then
    echo "[WIKI-CHECK]: FAILED: page not found: $PAGE"
    exit 1
fi

SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

# ------------------------------------------------------------------------
# Ground truth: is a given interface really provided by this source tree?
# ------------------------------------------------------------------------
gpiochip_real=0
grep -q devm_gpiochip_add_data "$SRC_GPIO" 2>/dev/null && gpiochip_real=1

watchdog_real=0
grep -q watchdog_register_device "$SRC_WDT" 2>/dev/null && watchdog_real=1

misc_dev_real=0
grep -q '"misc"' "$SRC_MISC" 2>/dev/null && misc_dev_real=1

hwmon_real=0
grep -qE 'hwmon_device_register_with_(info|groups)' "$SRC_HWMON" 2>/dev/null && hwmon_real=1

misc_sysfs_real=0
grep -q sysfs_create_group "$SRC_MISC" 2>/dev/null && misc_sysfs_real=1

# Valid make targets: the Makefile's own `DRIVERS :=` line names the
# subsystems -- not hard-coded here -- plus each subsystem's -debug
# variant, plus the fixed lifecycle targets.
drivers_line=$(grep -m1 '^DRIVERS[[:space:]]*:=' "$MAKEFILE" 2>/dev/null)
subsystems=$(printf '%s\n' "$drivers_line" | sed -E 's/^DRIVERS[[:space:]]*:=[[:space:]]*//')

: > "$SCRATCH/valid_targets"
for s in $subsystems; do
    printf '%s\n' "$s" >> "$SCRATCH/valid_targets"
    printf '%s-debug\n' "$s" >> "$SCRATCH/valid_targets"
done
for t in all modules clean install uninstall help config; do
    printf '%s\n' "$t" >> "$SCRATCH/valid_targets"
done
sort -u -o "$SCRATCH/valid_targets" "$SCRATCH/valid_targets"

is_valid_target() {
    grep -qxF "$1" "$SCRATCH/valid_targets"
}

# ------------------------------------------------------------------------
# Extract claims from the page's own text.
# ------------------------------------------------------------------------

# 1. Device paths: every /dev/<word> substring, deduped.
grep -oE '/dev/[A-Za-z0-9_]+' "$PAGE" | sort -u > "$SCRATCH/dev_claims"

# 2. sysfs class paths: every /sys/class/<word>/ substring, deduped. `misc`
# is graded separately below at full path precision instead of here --
# unlike hwmon/watchdog, whose device number is assigned at runtime with no
# compile-time string to check, misc has a real compile-time constant (the
# miscdevice's own name) worth checking down to the path segment.
grep -oE '/sys/class/(hwmon|watchdog)/' "$PAGE" | sort -u > "$SCRATCH/sysfs_claims"

# 2b. sysfs misc paths, at full path precision: every /sys/class/misc/<seg>
# substring, checked against the one real compile-time constant this driver
# has for it -- the miscdevice's own name, "misc" (src/drivers/misc.c).
grep -oE '/sys/class/misc/[A-Za-z0-9_-]+' "$PAGE" | sed -E 's#^/sys/class/misc/##' \
  | sort -u > "$SCRATCH/misc_segment_claims"

# 3. make targets: pull the page's own code text apart first -- fenced
# ``` blocks verbatim, and the interior of every inline `...` span -- so
# prose that merely mentions the word "make" is never looked at.
awk '
    /^```/ { infence = !infence; next }
    infence { print; next }
    {
        line = $0
        while (match(line, /`[^`]*`/)) {
            print substr(line, RSTART + 1, RLENGTH - 2)
            line = substr(line, RSTART + RLENGTH)
        }
    }
' "$PAGE" > "$SCRATCH/code_text"

# Within that code text, for every "make" token: skip any VAR=value
# tokens that follow it (e.g. BOARD_NAME=ESM-KX60G), then the next token,
# if it looks like a bare identifier, is the target claim; no such token
# (end of line, or it doesn't look like an identifier) means the implicit
# "all" target.
awk '
    {
        n = split($0, toks, /[ \t]+/)
        for (i = 1; i <= n; i++) {
            if (toks[i] == "make") {
                j = i + 1
                while (j <= n && toks[j] ~ /^[A-Za-z_][A-Za-z0-9_]*=/) j++
                if (j <= n && toks[j] ~ /^[A-Za-z0-9_-]+$/) {
                    print toks[j]
                } else {
                    print "all"
                }
            }
        }
    }
' "$SCRATCH/code_text" | sort -u > "$SCRATCH/make_claims"

# ------------------------------------------------------------------------
# Grade every claim. Every failure is printed by name and kind; nothing
# stops at the first one.
# ------------------------------------------------------------------------
failures=0
checked=0

while IFS= read -r claim; do
    [ -z "$claim" ] && continue
    checked=$((checked + 1))
    ok=0
    if [[ "$claim" =~ ^/dev/gpiochip[0-9]*$ ]]; then
        [ "$gpiochip_real" -eq 1 ] && ok=1
    elif [[ "$claim" =~ ^/dev/watchdog[0-9]*$ ]]; then
        [ "$watchdog_real" -eq 1 ] && ok=1
    elif [ "$claim" = "/dev/misc" ]; then
        [ "$misc_dev_real" -eq 1 ] && ok=1
    fi
    if [ "$ok" -ne 1 ]; then
        echo "[WIKI-CHECK]: FAILED: device path claim not real: $claim"
        failures=$((failures + 1))
    fi
done < "$SCRATCH/dev_claims"

while IFS= read -r claim; do
    [ -z "$claim" ] && continue
    checked=$((checked + 1))
    ok=0
    case "$claim" in
        /sys/class/hwmon/*)    [ "$hwmon_real" -eq 1 ] && ok=1 ;;
        /sys/class/watchdog/*) [ "$watchdog_real" -eq 1 ] && ok=1 ;;
    esac
    if [ "$ok" -ne 1 ]; then
        echo "[WIKI-CHECK]: FAILED: sysfs class path claim not real: $claim"
        failures=$((failures + 1))
    fi
done < "$SCRATCH/sysfs_claims"

while IFS= read -r seg; do
    [ -z "$seg" ] && continue
    checked=$((checked + 1))
    if [ "$seg" != "misc" ] || [ "$misc_sysfs_real" -ne 1 ]; then
        echo "[WIKI-CHECK]: FAILED: sysfs misc path claim not real: /sys/class/misc/$seg/"
        failures=$((failures + 1))
    fi
done < "$SCRATCH/misc_segment_claims"

while IFS= read -r claim; do
    [ -z "$claim" ] && continue
    checked=$((checked + 1))
    if ! is_valid_target "$claim"; then
        echo "[WIKI-CHECK]: FAILED: make target claim not real: make $claim"
        failures=$((failures + 1))
    fi
done < "$SCRATCH/make_claims"

if [ "$checked" -eq 0 ]; then
    echo "[WIKI-CHECK]: FAILED: zero claims found in $PAGE -- a check that verifies nothing is not a pass."
    exit 1
fi

if [ "$failures" -gt 0 ]; then
    echo "[WIKI-CHECK]: FAILED: $failures of $checked claim(s) in $PAGE do not hold."
    exit 1
fi

echo "[WIKI-CHECK]: PASS: all $checked claim(s) in $PAGE hold."
exit 0
