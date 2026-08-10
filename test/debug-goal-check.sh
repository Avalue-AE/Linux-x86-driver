#!/bin/bash
# Proves a "<subsystem>-debug" goal (e.g. `make hwmon-debug`) selects exactly
# the one subsystem its plain counterpart does, with CONFIG_DEBUG=y the only
# difference -- and that a board unable to build that subsystem stops the
# debug goal the same way it stops the plain one. See test/README.md and
# issue #71.
#
# This never touches a real kernel tree or the committed source tree: the
# whole repo (Makefile, configs/, scripts/, src/) is copied into a scratch
# directory once, and every `make` invocation below runs against that copy,
# with KERNEL_SOURCE pointed at a scratch stand-in whose `modules` target
# only echoes back the MAKE_* variables (and CONFIG_DEBUG) it was handed --
# no compiler involved, so the echoed line alone tells us which subsystems a
# goal actually turned on.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARDS_DIR="$REPO_ROOT/configs/boards"

SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

WORK="$SCRATCH/repo"
FAKE_KERNEL="$SCRATCH/fake-kernel"
mkdir -p "$WORK" "$FAKE_KERNEL"
cp -a "$REPO_ROOT/Makefile" "$REPO_ROOT/configs" "$REPO_ROOT/scripts" "$REPO_ROOT/src" "$WORK/"

# Same fixture the issue's own repro used -- a stand-in KERNEL_SOURCE whose
# `modules` target only echoes the MAKE_* vars and CONFIG_DEBUG it receives.
cat > "$FAKE_KERNEL/Makefile" <<'EOF'
modules:
	@echo "MAKE_WDT=[$(MAKE_WDT)] MAKE_GPIO=[$(MAKE_GPIO)] MAKE_HWM=[$(MAKE_HWM)] MAKE_MISC=[$(MAKE_MISC)] CONFIG_DEBUG=[$(CONFIG_DEBUG)]"
clean:
	@true
EOF

DRIVERS="watchdog gpio hwmon misc"
ECHO_RE='MAKE_WDT=\[[^]]*\] MAKE_GPIO=\[[^]]*\] MAKE_HWM=\[[^]]*\] MAKE_MISC=\[[^]]*\] CONFIG_DEBUG=\[[^]]*\]'

LAST_RC=0
LAST_OUT=""
run_goal() {
    # run_goal <board> <goal> -- sets LAST_RC / LAST_OUT. Runs in a subshell
    # (via `cd ... &&` inside command substitution) so this script's own
    # cwd, and the real checkout, are never touched.
    local board="$1" goal="$2"
    LAST_OUT=$(cd "$WORK" && make BOARD_NAME="$board" KERNEL_SOURCE="$FAKE_KERNEL" "$goal" 2>&1)
    LAST_RC=$?
}

declare -A goal_pass
declare -A goal_total
for d in $DRIVERS; do
    goal_pass["$d"]=0;       goal_total["$d"]=0
    goal_pass["$d-debug"]=0; goal_total["$d-debug"]=0
done

failures=()
boards=0

for conf in "$BOARDS_DIR"/*.conf; do
    boards=$((boards + 1))
    board=$(basename "$conf" .conf)

    for drv in $DRIVERS; do
        run_goal "$board" "$drv"
        plain_rc=$LAST_RC
        plain_out="$LAST_OUT"

        run_goal "$board" "${drv}-debug"
        debug_rc=$LAST_RC
        debug_out="$LAST_OUT"

        goal_total["$drv"]=$((goal_total["$drv"] + 1))
        goal_total["$drv-debug"]=$((goal_total["$drv-debug"] + 1))

        ok=1
        reason=""

        if [ "$debug_rc" -ne "$plain_rc" ]; then
            ok=0
            reason="exit code differs: plain=$plain_rc debug=$debug_rc"
        elif [ "$plain_rc" -eq 0 ]; then
            plain_line=$(echo "$plain_out" | grep -oE "$ECHO_RE")
            debug_line=$(echo "$debug_out" | grep -oE "$ECHO_RE")
            if [ -z "$plain_line" ] || [ -z "$debug_line" ]; then
                ok=0
                reason="missing MAKE_* echo line (plain='$plain_line' debug='$debug_line')"
            else
                plain_makes=${plain_line% CONFIG_DEBUG=*}
                debug_makes=${debug_line% CONFIG_DEBUG=*}
                plain_debug=${plain_line##*CONFIG_DEBUG=}
                debug_debug=${debug_line##*CONFIG_DEBUG=}
                if [ "$plain_makes" != "$debug_makes" ]; then
                    ok=0
                    reason="MAKE_* set differs: plain='$plain_makes' debug='$debug_makes'"
                elif [ "$plain_debug" != "[]" ] || [ "$debug_debug" != "[y]" ]; then
                    ok=0
                    reason="CONFIG_DEBUG did not go []->[y]: plain=$plain_debug debug=$debug_debug"
                fi
            fi
        else
            if [ "$plain_out" != "$debug_out" ]; then
                ok=0
                reason="error text differs between plain and debug run (rc=$plain_rc both)"
            fi
        fi

        if [ "$ok" -eq 1 ]; then
            goal_pass["$drv"]=$((goal_pass["$drv"] + 1))
            goal_pass["$drv-debug"]=$((goal_pass["$drv-debug"] + 1))
        else
            failures+=("$board/$drv-debug: $reason")
        fi
    done
done

echo "[DEBUG-GOAL]: swept $boards board(s) x 4 subsystem(s), plain and -debug each."
for d in $DRIVERS; do
    echo "[DEBUG-GOAL]: $d: ${goal_pass[$d]}/${goal_total[$d]}"
    echo "[DEBUG-GOAL]: $d-debug: ${goal_pass[$d-debug]}/${goal_total[$d-debug]}"
done

if [ "${#failures[@]}" -gt 0 ]; then
    echo "[DEBUG-GOAL]: FAILED: ${#failures[@]} board/goal combination(s):"
    for f in "${failures[@]}"; do
        echo "[DEBUG-GOAL]:   $f"
    done
fi

# Separate, one-off check (issue box 4): an unknown "<name>-debug" goal must
# be refused by name -- naming all four valid subsystems -- rather than
# silently falling through to "build everything the board declares".
bad_ok=1
bad_reason=""
run_goal "ECM-WHL" "wibble-debug"
bad_rc=$LAST_RC
bad_out="$LAST_OUT"
if [ "$bad_rc" -eq 0 ]; then
    bad_ok=0
    bad_reason="wibble-debug exited 0 (should be rejected)"
else
    for want in watchdog gpio hwmon misc; do
        if ! echo "$bad_out" | grep -q "$want"; then
            bad_ok=0
            bad_reason="rejection message does not name '$want': $bad_out"
        fi
    done
fi

if [ "$bad_ok" -eq 1 ]; then
    echo "[DEBUG-GOAL]: unknown-goal rejection (wibble-debug): PASS"
else
    echo "[DEBUG-GOAL]: unknown-goal rejection (wibble-debug): FAIL: $bad_reason"
fi

if [ "${#failures[@]}" -gt 0 ] || [ "$bad_ok" -ne 1 ]; then
    echo "[DEBUG-GOAL]: FAIL"
    exit 1
fi

echo "[DEBUG-GOAL]: PASS"
exit 0
