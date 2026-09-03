#!/bin/bash

CONF_FILE=$1
OUT_FILE=$2
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

# Full clones resolve the version from git tags. A shipped single-board
# tarball has no .git, so fall back to a VERSION file (written at packaging
# time); if neither is available, say so explicitly rather than emitting an
# empty #define that reads as "no version" in support logs.
VERSION=`git -C "$SCRIPT_DIR" describe --tags 2>/dev/null`
if [ -z "$VERSION" ] && [ -f "$SCRIPT_DIR/../VERSION" ]; then
    VERSION=$(cat "$SCRIPT_DIR/../VERSION")
fi
VERSION=${VERSION:-unknown}

DATE=$(date +"%Y-%m-%d")

if [ -z "$CONF_FILE" ] || [ -z "$OUT_FILE" ]; then
    echo "[CONFIG]: Usage: $0 <path_to_conf> <path_to_header>"
    exit 1
fi

if [ ! -f "$CONF_FILE" ]; then
    echo "[CONFIG]: Error: Config file $CONF_FILE not found."
    exit 1
fi

# A subsystem's whole-group _NUM / _MAP keys (as opposed to a per-channel
# _ENABLE key) are not optional: the kernel build reads them straight into a
# C array initializer, so a missing one fails 200 lines into the compiler
# with "excess elements in array initializer" instead of naming the board
# file and the key. Catch it here, before any compiler runs, but only for a
# subsystem the board really builds -- both its DEVICE and CHIPSET keys
# non-empty, the same pair the Makefile's own guard tests.
strip_trailing_comment() {
    # A comment may follow a value on the same line ("value  # note").
    # Cut at the first unquoted '#'; a '#' inside a double-quoted string
    # (a LABEL value) or a single-quoted char literal ('#') is data, not a
    # comment, so it survives.
    local v="$1" out="" in_dq=0 in_sq=0 ch i
    for ((i = 0; i < ${#v}; i++)); do
        ch="${v:$i:1}"
        if [ "$ch" = '"' ] && [ "$in_sq" -eq 0 ]; then
            in_dq=$((1 - in_dq))
        elif [ "$ch" = "'" ] && [ "$in_dq" -eq 0 ]; then
            in_sq=$((1 - in_sq))
        elif [ "$ch" = '#' ] && [ "$in_dq" -eq 0 ] && [ "$in_sq" -eq 0 ]; then
            break
        fi
        out+="$ch"
    done
    # printf, not echo: an unquoted value that is exactly "-n", "-e" or
    # "-E" is a valid config value, not an echo option, and plain `echo`
    # cannot tell the difference -- it swallows the value and this
    # function's caller captures an empty string instead.
    printf '%s\n' "$out"
}

# Leading/trailing whitespace trim with no external process: a bash-only
# equivalent of `sed 's/^[[:space:]]*//;s/[[:space:]]*$//'`.
trim_ws() {
    local v="$1"
    v="${v#"${v%%[![:space:]]*}"}"
    v="${v%"${v##*[![:space:]]}"}"
    printf '%s' "$v"
}

declare -A CONF_VALUE
while IFS='=' read -r ckey cvalue; do
    [[ "$ckey" =~ ^#.*$ ]] && continue
    [[ -z "$ckey" ]] && continue
    ckey=$(trim_ws "$ckey")
    cvalue=$(strip_trailing_comment "$cvalue")
    cvalue=$(trim_ws "$cvalue")
    CONF_VALUE["$ckey"]="$cvalue"
done < "$CONF_FILE"

count_map_elements() {
    # Top-level element count of a MAP's brace body: commas at brace/bracket/
    # paren depth 0, plus one; an empty body counts 0. A macro call with its
    # own commas inside parentheses (F81966_HWM_REG_VOL(0, 1)) is one element.
    local body="$1" depth=0 count=0 ch i
    # trim_ws, not a piped sed: a piped external command's own exit status
    # went unchecked here, so a failing sed silently made this function
    # return 0 as if the body were empty -- and the caller reports that as
    # "$MAP has 0 element(s)", blaming a board file that is correct
    # trim_ws is a bash builtin with no external
    # process to fail.
    body=$(trim_ws "$body")
    if [ -z "$body" ]; then
        echo 0
        return
    fi
    # A trailing comma before the closing brace ({ A, B, }) is ordinary,
    # legal C and names no extra element.
    [[ "$body" == *, ]] && body="${body%,}"
    body=$(trim_ws "$body")
    if [ -z "$body" ]; then
        echo 0
        return
    fi
    count=1
    for ((i = 0; i < ${#body}; i++)); do
        ch="${body:$i:1}"
        case "$ch" in
        '{' | '[' | '(') depth=$((depth + 1)) ;;  # unchecked-ok: '|' here separates case patterns, not a shell pipe -- no command runs on this line
        '}' | ']' | ')') depth=$((depth - 1)) ;;  # unchecked-ok: same case-pattern alternation as the line above, not a pipe
        ',') [ "$depth" -eq 0 ] && count=$((count + 1)) ;;
        esac
    done
    echo "$count"
}

require_group_keys() {
    local subsys="$1" device_key="$2" chipset_key="$3"
    shift 3
    [ -z "${CONF_VALUE[$device_key]}" ] && return 0
    [ -z "${CONF_VALUE[$chipset_key]}" ] && return 0
    local key
    for key in "$@"; do
        if [ -z "${CONF_VALUE[$key]}" ]; then
            echo "[CONFIG]: Error: $CONF_FILE builds '$subsys' ($device_key=${CONF_VALUE[$device_key]}, $chipset_key=${CONF_VALUE[$chipset_key]}) but does not set $key."
            echo "[CONFIG]:        Every subsystem a board builds must set its _NUM and _MAP keys."
            exit 1
        fi
    done
    # _NUM / _MAP count check: for each _NUM key in this call whose _MAP
    # partner is also in this call (both already confirmed present above),
    # the declared count must equal the map's own top-level element count --
    # a mismatch compiles today and fails 200 lines in with "excess elements
    # in array initializer". A _MAP value that does not parse as { ... } is
    # named and counted as not gradable rather than silently skipped or
    # made to crash the script -- an unforeseen form must never go quiet.
    local pairs_checked=0 not_gradable=0
    for key in "$@"; do
        [[ "$key" == *_NUM ]] || continue
        local map_key="${key%_NUM}_MAP" has_map=0 k2
        for k2 in "$@"; do [ "$k2" = "$map_key" ] && has_map=1; done
        [ "$has_map" -eq 1 ] || continue
        local map_value="${CONF_VALUE[$map_key]}"
        if ! [[ "$map_value" =~ ^\{.*\}$ ]]; then
            echo "[CONFIG]: Note: $CONF_FILE sets $map_key=$map_value, which this guard cannot read as a { ... } map; not gradable, skipping the count check for $key/$map_key."
            not_gradable=$((not_gradable + 1))
            continue
        fi
        local body="${map_value#\{}"
        body="${body%\}}"
        local declared="${CONF_VALUE[$key]}" counted
        counted=$(count_map_elements "$body")
        pairs_checked=$((pairs_checked + 1))
        if ! [[ "$declared" =~ ^[0-9]+$ ]] || [ "$declared" -ne "$counted" ]; then
            echo "[CONFIG]: Error: $CONF_FILE sets $key=$declared but $map_key has $counted element(s)."
            echo "[CONFIG]:        The declared count and the map's element count must match, or the compiler fails 200 lines in with 'excess elements in array initializer'."
            exit 1
        fi
    done
    echo "[CONFIG]: Verified $pairs_checked _NUM/_MAP pair(s) for '$subsys' ($not_gradable not gradable)."
}

require_group_keys hwm MAKE_HWM_DEVICE MAKE_HWM_CHIPSET \
    CONFIG_HWM_CHIPSET \
    CONFIG_HWM_VOLTAGE_NUM CONFIG_HWM_VOLTAGE_MAP \
    CONFIG_HWM_TEMPERATURE_NUM CONFIG_HWM_TEMPERATURE_MAP \
    CONFIG_HWM_FAN_NUM CONFIG_HWM_FAN_MAP \
    CONFIG_HWM_PWM_NUM CONFIG_HWM_PWM_MAP

require_group_keys gpio MAKE_GPIO_DEVICE MAKE_GPIO_CHIPSET \
    CONFIG_GPIO_PIN_NUM CONFIG_GPIO_PIN_MAP

require_group_keys misc MAKE_MISC_DEVICE MAKE_MISC_CHIPSET \
    CONFIG_MISC_NUM CONFIG_MISC_MAP

# The ITE EC GPIO HAL reads its register addresses straight from CONFIG_GPIO_*
# keys with no safe fallback: unlike a _NUM/_MAP omission, a missing register
# key still compiles, and the driver then reads or writes whichever address
# src/hal/ec/ite.h used to default to -- on real hardware, not this board's own
# EC. So the board file must name every register CONFIG_GPIO_DIR_FIXED's mode
# uses, and this stops the build before any compiler runs.
require_ite_gpio_keys() {
    [ "${CONF_VALUE[MAKE_GPIO_DEVICE]}" != "ec" ] && return 0
    [ "${CONF_VALUE[MAKE_GPIO_CHIPSET]}" != "ite" ] && return 0
    local dir_fixed="${CONF_VALUE[CONFIG_GPIO_DIR_FIXED]}"
    if [ -z "$dir_fixed" ]; then
        echo "[CONFIG]: Error: $CONF_FILE builds 'gpio' (MAKE_GPIO_DEVICE=ec, MAKE_GPIO_CHIPSET=ite) but does not set CONFIG_GPIO_DIR_FIXED."
        echo "[CONFIG]:        Every ITE EC GPIO board must set CONFIG_GPIO_DIR_FIXED (0 or 1) to pick its register layout."
        exit 1
    fi
    if [ "$dir_fixed" != "0" ] && [ "$dir_fixed" != "1" ]; then
        echo "[CONFIG]: Error: $CONF_FILE sets CONFIG_GPIO_DIR_FIXED=$dir_fixed, but this key must be 0 or 1."
        echo "[CONFIG]:        0 selects the configurable-direction model, 1 the fixed split-bank model."
        exit 1
    fi
    local key
    for key in CONFIG_GPIO_REG_INPUT CONFIG_GPIO_REG_OUTPUT; do
        if [ -z "${CONF_VALUE[$key]}" ]; then
            echo "[CONFIG]: Error: $CONF_FILE builds 'gpio' (MAKE_GPIO_DEVICE=ec, MAKE_GPIO_CHIPSET=ite) but does not set $key."
            echo "[CONFIG]:        Every ITE EC GPIO board must set its own register addresses; there is no safe default."
            exit 1
        fi
    done
    if [ "$dir_fixed" = "1" ]; then
        key=CONFIG_GPIO_DIR_MASK
    else
        key=CONFIG_GPIO_REG_DIRECTION
    fi
    if [ -z "${CONF_VALUE[$key]}" ]; then
        echo "[CONFIG]: Error: $CONF_FILE builds 'gpio' (MAKE_GPIO_DEVICE=ec, MAKE_GPIO_CHIPSET=ite, CONFIG_GPIO_DIR_FIXED=$dir_fixed) but does not set $key."
        echo "[CONFIG]:        Every ITE EC GPIO board must set the register its own CONFIG_GPIO_DIR_FIXED mode uses."
        exit 1
    fi
}

require_ite_gpio_keys

# The SMBus host protocol reads its bus address straight from CONFIG_SMBUS_*
# keys with no safe fallback: a missing key still compiles, and the driver
# then talks to whichever address src/hal/smb/smb.h used to default to -- on
# real hardware, not this board's own bus. So any board that builds a
# subsystem over the smb HAL must name its own base port and slave addresses,
# and this stops the build before any compiler runs.
require_smb_addr_keys() {
    local subsys hit=""
    for subsys in GPIO HWM MISC; do
        local device_key="MAKE_${subsys}_DEVICE"
        local chipset_key="MAKE_${subsys}_CHIPSET"
        if [ "${CONF_VALUE[$device_key]}" = "smb" ] && [ -n "${CONF_VALUE[$chipset_key]}" ]; then
            hit="$subsys ($device_key=smb, $chipset_key=${CONF_VALUE[$chipset_key]})"
            break
        fi
    done
    [ -z "$hit" ] && return 0
    local key
    for key in CONFIG_SMBUS_BASE_PORT CONFIG_SMBUS_SLAVE_ADDR1 CONFIG_SMBUS_SLAVE_ADDR2; do
        if [ -z "${CONF_VALUE[$key]}" ]; then
            echo "[CONFIG]: Error: $CONF_FILE builds '$hit' over the smb HAL but does not set $key."
            echo "[CONFIG]:        Every board on the SMBus host protocol must name its own bus address; there is no safe default."
            exit 1
        fi
    done
}

require_smb_addr_keys

# The three chip HALs (src/hal/ec/ite.c, src/hal/sio/f81966.c,
# src/hal/sio/nct61x6d.c) each read the board's declared chip id from
# CONFIG_CHIPID with no safe fallback: a missing key still compiles, and the
# HAL's own build-time check then has nothing to compare the probed id
# against. So any board that builds a subsystem over one of these three chip
# HALs must name its own chip id, and this stops the build before any
# compiler runs.
require_chip_id_key() {
    local subsys hit=""
    for subsys in WDT HWM GPIO MISC; do
        local device_key="MAKE_${subsys}_DEVICE"
        local chipset_key="MAKE_${subsys}_CHIPSET"
        local device="${CONF_VALUE[$device_key]}"
        local chipset="${CONF_VALUE[$chipset_key]}"
        if [ "$device" = "ec" ] && [ "$chipset" = "ite" ]; then
            hit="$subsys ($device_key=ec, $chipset_key=ite)"
            break
        fi
        if [ "$device" = "sio" ] && [ "$chipset" = "f81966" ]; then
            hit="$subsys ($device_key=sio, $chipset_key=f81966)"
            break
        fi
        if [ "$device" = "sio" ] && [ "$chipset" = "nct61x6d" ]; then
            hit="$subsys ($device_key=sio, $chipset_key=nct61x6d)"
            break
        fi
    done
    [ -z "$hit" ] && return 0
    if [ -z "${CONF_VALUE[CONFIG_CHIPID]}" ] || ! [[ "${CONF_VALUE[CONFIG_CHIPID]}" =~ ^0x[0-9A-Fa-f]+$ ]]; then
        echo "[CONFIG]: Error: $CONF_FILE builds '$hit' over a chip HAL but does not set CONFIG_CHIPID to a hex literal (0x????)."
        echo "[CONFIG]:        Every board on the ite / f81966 / nct61x6d chip HALs must name its own chip id as 0x<hex>; there is no safe default."
        exit 1
    fi
}

require_chip_id_key

# An earlier finding: a voltage channel can be labelled as a board input rail (VIN /
# VIN_L / DCIN) while its divider resistors are both left at 0. With
# R1=R2=0, src/hal/ec/ite_hwm.c skips the divider math entirely and reports
# the raw ADC counts as if they were the finished voltage -- exactly what
# happened on ECM-WHL/EPC-WHL: bench VIN read 1.12V on a 12V board. This
# is a REPORT, never a build failure: DIMM/Threm_*/5V/VRTC/etc
# rails legitimately carry R1=R2=0 too, so the check is scoped to labels
# that specifically claim to be a board input rail, and a board file can
# silence one channel deliberately (see undivided_channel_is_annotated()).
undivided_channel_is_annotated() {
    local n="$1" enable_key="CONFIG_HWM_VOLTAGE_${n}_ENABLE=" lineno i line
    lineno=$(grep -n "^${enable_key}" "$CONF_FILE" | head -1 | cut -d: -f1)  # unchecked-ok: a grep no-match here is the legitimate "this board has no such _ENABLE key" signal, and the very next line's [ -z "$lineno" ] && return 1 already treats any failure (grep, head or cut) the same conservative way
    [ -z "$lineno" ] && return 1
    i=$((lineno - 1))
    while [ "$i" -ge 1 ]; do
        line=$(sed -n "${i}p" "$CONF_FILE")
        if [[ "$line" =~ ^[[:space:]]*# ]]; then
            [[ "$line" == *UNDIVIDED-ON-PURPOSE* ]] && return 0
            i=$((i - 1))
            continue
        fi
        break
    done
    return 1
}

report_undivided_input_rail() {
    [ -z "${CONF_VALUE[MAKE_HWM_DEVICE]}" ] && return 0
    [ -z "${CONF_VALUE[MAKE_HWM_CHIPSET]}" ] && return 0
    local key n channels=()
    for key in "${!CONF_VALUE[@]}"; do
        [[ "$key" =~ ^CONFIG_HWM_VOLTAGE_([0-9]+)_LABEL$ ]] && channels+=("${BASH_REMATCH[1]}")
    done
    [ "${#channels[@]}" -eq 0 ] && return 0
    local sorted label label_upper r1 r2
    sorted=$(printf '%s\n' "${channels[@]}" | sort -n) || {
        echo "[CONFIG]: Error: $CONF_FILE: failed to sort voltage channel numbers."
        exit 1
    }
    for n in $sorted; do
        label="${CONF_VALUE[CONFIG_HWM_VOLTAGE_${n}_LABEL]}"
        # CONF_VALUE for a _LABEL key still carries its surrounding double
        # quotes literally; strip them before comparing.
        label="${label%\"}"
        label="${label#\"}"
        label_upper="${label^^}"
        case "$label_upper" in
        VIN | VIN_L | DCIN) ;;  # unchecked-ok: '|' separates case patterns naming the input-rail labels, not a shell pipe -- no command runs on this line
        *) continue ;;
        esac
        r1="${CONF_VALUE[CONFIG_HWM_VOLTAGE_${n}_R1]}"
        r2="${CONF_VALUE[CONFIG_HWM_VOLTAGE_${n}_R2]}"
        [ "$r1" = "0" ] && [ "$r2" = "0" ] || continue
        undivided_channel_is_annotated "$n" && continue
        echo "[CONFIG]: Report: $CONF_FILE channel $n label \"$label\" has R1=0 and R2=0 -- reads the raw ADC value as the voltage on a rail whose label names a board input (VIN/VIN_L/DCIN). Set real R1/R2 from the schematic, or add a \"# UNDIVIDED-ON-PURPOSE: <why>\" comment line immediately above its _ENABLE key if this is deliberate."
    done
}

report_undivided_input_rail

mkdir -p "$(dirname "$OUT_FILE")" || {
    echo "[CONFIG]: Error: could not create the output directory for $OUT_FILE."
    exit 1
}

echo "[CONFIG]: Generating $OUT_FILE from $CONF_FILE ..."


cat <<EOF > "$OUT_FILE"
/** 
 * ==============================
 * Auto-generated from $CONF_FILE
 * DO NOT EDIT DIRECTLY
 * Generated at $DATE Driver v$VERSION
 * ==============================
 */
 
#ifndef __CONFIGS_BOARD_CONFIG_H__
#define __CONFIGS_BOARD_CONFIG_H__

#define CONFIG_DRIVER_VERSION "$VERSION"

EOF

while IFS='=' read -r key value; do
    # Skip comments and empty lines
    [[ "$key" =~ ^#.*$ ]] && continue
    [[ -z "$key" ]] && continue
    
    # Skip MAKE_* variables (they are only for Makefile, not for C headers)
    [[ "$key" =~ ^MAKE_ ]] && continue
    
    # Trim whitespace
    key=$(trim_ws "$key")
    value=$(strip_trailing_comment "$value")
    value=$(trim_ws "$value")
    
    # Check if value is a single-quoted character literal (e.g., 'A')
    _re="^'.'$"
    if [[ "$value" =~ $_re ]]; then
        echo "#define $key $value" >> "$OUT_FILE"

    # Check if value is already double-quoted (e.g., "ECM-ASL") — emit as-is
    elif [[ "$value" =~ ^\".*\"$ ]]; then
        echo "#define $key $value" >> "$OUT_FILE"

    # Check if value is a hex number
    elif [[ "$value" =~ ^0x[0-9A-Fa-f]+$ ]]; then
        echo "#define $key $value" >> "$OUT_FILE"

    # Check if value is an integer
    elif [[ "$value" =~ ^[0-9]+$ ]]; then
        echo "#define $key $value" >> "$OUT_FILE"

    # Check if value is a C macro/function call with parentheses (e.g., F81966_HWM_REG_VOL(0))
    elif [[ "$value" =~ ^[A-Z_][A-Z0-9_]*\(.*\)$ ]]; then
        echo "#define $key $value" >> "$OUT_FILE"

    # Check if value is an array (starts with { and ends with })
    elif [[ "$value" =~ ^\{.*\}$ ]]; then
        echo "#define $key $value" >> "$OUT_FILE"
    
    # Check if key ends with _MAP or _CONFIG (array-like, no quotes)
    elif [[ "$key" =~ _(MAP|CONFIG)$ ]]; then  # unchecked-ok: '|' is inside a quoted =~ regex group (MAP|CONFIG), not a shell pipe -- [[ ]] has no pipe stage to check
        echo "#define $key $value" >> "$OUT_FILE"
    
    # Otherwise, treat as string (add quotes)
    else
        echo "#define $key \"$value\"" >> "$OUT_FILE"
    fi
done < "$CONF_FILE"

echo -e "\n#endif /** __CONFIGS_BOARD_CONFIG_H__ */" >> "$OUT_FILE"

echo "[CONFIG]: Done."