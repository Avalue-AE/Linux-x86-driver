#!/bin/bash
# Proves scripts/config.sh's undivided-input-rail report (issue #75) three
# ways, on a synthetic minimal board fixture written inline here -- never
# reads or copies anything from configs/boards/, so this file needs no
# hardware and does not join test/README.md's internal-only file list. See
# test/README.md's "Board file sweep" section.
#
# This never touches the committed tree: each case writes its own scratch
# .conf and hands it to the real scripts/config.sh, which writes its
# generated header into the same mktemp -d scratch directory removed when
# the run ends.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_SH="$REPO_ROOT/scripts/config.sh"

SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

failures=0
checked=0

# write_fixture <path> <label> <r1> <r2> <annotate: yes|no>
# Base fixture: passes every one of scripts/config.sh's other guards
# (require_group_keys' hwm _NUM/_MAP pairs, require_chip_id_key via
# MAKE_HWM_DEVICE=ec/MAKE_HWM_CHIPSET=ite+CONFIG_CHIPID); MAKE_GPIO_DEVICE
# and MAKE_MISC_DEVICE are left unset so require_ite_gpio_keys and
# require_smb_addr_keys no-op. Only LABEL/R1/R2 on CONFIG_HWM_VOLTAGE_0 and
# the annotation comment vary per case.
write_fixture() {
    local path="$1" label="$2" r1="$3" r2="$4" annotate="$5" enable_line
    enable_line="CONFIG_HWM_VOLTAGE_0_ENABLE=1"
    if [ "$annotate" = "yes" ]; then
        enable_line="# UNDIVIDED-ON-PURPOSE: test fixture, measured directly
$enable_line"
    fi
    cat > "$path" <<EOF
CONFIG_BOARD_NAME="TESTBOARD"

MAKE_HWM_DEVICE=ec
MAKE_HWM_CHIPSET=ite
CONFIG_HWM_CHIPSET=ite
CONFIG_CHIPID=0x1234

$enable_line
CONFIG_HWM_VOLTAGE_0_REG=0x30
CONFIG_HWM_VOLTAGE_0_LABEL="$label"
CONFIG_HWM_VOLTAGE_0_LSB=12
CONFIG_HWM_VOLTAGE_0_R1=$r1
CONFIG_HWM_VOLTAGE_0_R2=$r2

CONFIG_HWM_VOLTAGE_NUM=1
CONFIG_HWM_VOLTAGE_MAP={ CONFIG_HWM_VOLTAGE_0_ENABLE }

CONFIG_HWM_TEMPERATURE_NUM=0
CONFIG_HWM_TEMPERATURE_MAP={ }

CONFIG_HWM_FAN_NUM=0
CONFIG_HWM_FAN_MAP={ }

CONFIG_HWM_PWM_NUM=0
CONFIG_HWM_PWM_MAP={ }
EOF
}

# run_case <name> <label> <r1> <r2> <annotate> <expect: report|quiet>
run_case() {
    local name="$1" label="$2" r1="$3" r2="$4" annotate="$5" expect="$6"
    local conf="$SCRATCH/${name}.conf" out="$SCRATCH/${name}.h" log rc has_report
    write_fixture "$conf" "$label" "$r1" "$r2" "$annotate"
    log=$("$CONFIG_SH" "$conf" "$out" 2>&1)
    rc=$?
    checked=$((checked + 1))
    if [ "$rc" -ne 0 ]; then
        echo "[UNDIVIDED-VIN]: FAIL $name -- scripts/config.sh exited $rc, expected 0. Output:"
        echo "$log"
        failures=$((failures + 1))
        return
    fi
    if echo "$log" | grep -q "^\[CONFIG\]: Report: $conf channel 0 "; then
        has_report=yes
    else
        has_report=no
    fi
    if [ "$expect" = "report" ] && [ "$has_report" = "yes" ]; then
        echo "[UNDIVIDED-VIN]: PASS $name -- Report line present naming channel 0, as expected."
    elif [ "$expect" = "quiet" ] && [ "$has_report" = "no" ]; then
        echo "[UNDIVIDED-VIN]: PASS $name -- no Report line, as expected."
    else
        echo "[UNDIVIDED-VIN]: FAIL $name -- expected $expect, got Report=$has_report. Output:"
        echo "$log"
        failures=$((failures + 1))
    fi
}

# Case 1: a divided channel (real R1/R2) stays quiet.
run_case "case1-divided" "VIN" 200000 20000 no quiet

# Case 2: an undivided input-rail channel is reported by name.
run_case "case2-undivided-vin" "VIN" 0 0 no report

# Case 3: same as case 2, but annotated -- goes quiet again.
run_case "case3-annotated" "VIN" 0 0 yes quiet

# Case 4: a non-input-rail label (DIMM) with R1=R2=0 is never reported --
# proves the label-set restriction.
run_case "case4-non-input-rail" "DIMM" 0 0 no quiet

if [ "$failures" -gt 0 ]; then
    echo "[UNDIVIDED-VIN]: FAILED: $failures of $checked case(s) failed."
    exit 1
fi

echo "[UNDIVIDED-VIN]: PASS: all $checked case(s) confirmed the undivided-input-rail report."
exit 0
