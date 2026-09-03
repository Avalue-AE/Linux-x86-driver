#!/bin/bash
# mirror-run-exempt: requires root access, gpiod tools, a loaded driver, physical GPIO, and loopback wiring.

# Require root privileges to access GPIO lines
if [ "$EUID" -ne 0 ]; then
	echo "Error: This script must be run as root to access GPIO lines."
	exit 1
fi

# Require installed gpiod
# apt install gpiod
if [ ! -x "$(command -v gpiodetect)" ]; then
	echo "Error: gpiod tools are not installed. Please install them to run this test."
	exit 1
fi

# For testing the GPIO functionality, we will use the gpiod tools to read and write to GPIO lines.
# Please ensure you have shorted the GPIO pins

# e.g., The target board has 8-bit GPIO lines
# pin0 <--> pin4
# pin1 <--> pin5
# pin2 <--> pin6
# pin3 <--> pin7

# e.g., Other target board has 16-bit GPIO lines
# pin0 <--> pin8
# pin1 <--> pin9
# pin2 <--> pin10
# pin3 <--> pin11
# ... and so on

# Identify the GPIO chip and number, by default it is named as "gpio"
echo "Identifying GPIO chip and lines"

GPIO_CHIP=$(gpiodetect | grep '\[gpio\]' | grep -oP 'gpiochip\d+')  # unchecked-ok: the next line's "[ -z "$GPIO_CHIP" ]" check already catches an empty extraction and exits 1
if [ -z "$GPIO_CHIP" ]; then
	echo "No GPIO chip found. Please check your GPIO setup."
	exit 1
fi
echo "Found GPIO chip: $GPIO_CHIP"

GPIO_LINES=$(gpiodetect | grep '\[gpio\]' | grep -oP 'gpiochip\d+.*\(\d+ lines\)' | grep -oP '\d+(?= lines)')  # unchecked-ok: the two checks right below (empty via "[ -z ]", zero via "[ -eq 0 ]") already tell an empty extraction apart from a real "0 lines" reading
if [ -z "$GPIO_LINES" ]; then
	echo "Error: could not read a GPIO line count from gpiodetect's output. Please check your GPIO setup."
	exit 1
fi
if [ "$GPIO_LINES" -eq 0 ]; then
	echo "No GPIO lines found. Please check your GPIO setup."
	exit 1
fi
echo "Found GPIO lines: $GPIO_LINES"

# gpioset has to HOLD the line while gpioget reads it back, and whether it does
# so on its own depends on which libgpiod is installed. v1 defaults to
# --mode=exit, "set values and exit immediately", so the line is released before
# the read and the input reads whatever its pull-up gives it -- a 1 where a 0 was
# driven, which is indistinguishable from a board whose loopback pair is broken.
# v1 therefore needs --mode=signal, "set values and wait for SIGINT or SIGTERM",
# which is exactly what the kill after each read sends. v2 holds the line until
# it is interrupted and has no --mode option at all, so it must be passed
# nothing. This variable was referenced by the four gpioset calls below from the
# day this script was written and never once assigned, so every run so far has
# used v1's default and reported a wiring fault it could not have measured.
#
# Asking gpioset for its own options settles which one is installed, rather than
# reading a version string: the option exists exactly where it is needed.
GPIOSET_HOLD=""
if gpioset --help 2>&1 | grep -q -- '--mode'; then  # unchecked-ok: the 'if' reads the pipeline's last stage, grep, which is exactly the test being made -- "does this gpioset have --mode"; a gpioset that cannot run at all answers no here and then fails loudly in the drive loop below
	GPIOSET_HOLD="--mode=signal"
fi
echo "Using gpioset hold: ${GPIOSET_HOLD:-none needed, this gpioset holds until interrupted}"


# Test each GPIO line pair in both directions: first half driving second half,
# then the reverse. gpioset is run in the background to hold the driven value
# while gpioget reads it, then killed to release the line before the next read.
#
# A pair that does not follow is RECORDED rather than ending the run. Which
# other pairs did is the only evidence that separates the two things a failure
# here can mean: with some pairs following and others not, the wiring is present
# and the ones that failed are the fault -- while none following at all is what
# a bench with no loopback wiring fitted looks like, and also what a driver
# whose output never reaches the pins looks like. Stopping at the first failure
# throws that evidence away and reports one pair as though it were the finding.
HALF=$((GPIO_LINES / 2))
TOTAL=$((HALF * 2))
PAIRS_PASSED=0
PAIRS_FAILED=""

drive_and_read() {
	# args: output pin, input pin, value to drive. Echoes what the input read,
	# or nothing when the read itself failed.
	local out="$1" in="$2" value="$3" got pid
	gpioset $GPIOSET_HOLD $GPIO_CHIP "$out=$value" &  # unchecked-ok: a gpioset that cannot drive leaves the input at its idle level, which is exactly the mismatch the caller then reports; its own stderr is on the console
	pid=$!
	sleep 0.1
	got=$(gpioget $GPIO_CHIP "$in" 2>/dev/null)  # unchecked-ok: a failed read yields an empty value, which the caller's comparison treats as a mismatch and names as "unreadable"
	kill "$pid" 2>/dev/null  # unchecked-ok: releasing the line; a gpioset that already exited is nothing to kill and the next iteration re-requests the line anyway
	wait "$pid" 2>/dev/null  # unchecked-ok: reaps the background gpioset, whose own status is not the measurement -- the value read above is
	printf '%s' "$got"
}

check_pair() {
	# args: output pin, input pin. Returns 0 when the input followed the output
	# to both values; otherwise prints what it actually read and returns 1.
	local out="$1" in="$2" low high
	low=$(drive_and_read "$out" "$in" 0)
	high=$(drive_and_read "$out" "$in" 1)

	if [ "$low" = "0" ] && [ "$high" = "1" ]; then
		return 0
	fi
	printf 'drove %s=0 and %s read %s; drove %s=1 and %s read %s' \
		"$out" "$in" "${low:-unreadable}" "$out" "$in" "${high:-unreadable}"
	return 1
}

run_direction() {
	# args: "forward" | "reverse". Walks every pair once in that direction.
	local direction="$1" i out in why
	for i in $(seq 0 $((HALF - 1))); do
		if [ "$direction" = "forward" ]; then
			out=$i
			in=$((i + HALF))
		else
			in=$i
			out=$((i + HALF))
		fi
		echo -n "Testing GPIO line pair: output=$out <--> input=$in ... "
		if why=$(check_pair "$out" "$in"); then
			echo "PASSED"
			PAIRS_PASSED=$((PAIRS_PASSED + 1))
		else
			echo "FAILED -- $why"
			PAIRS_FAILED="${PAIRS_FAILED:+$PAIRS_FAILED; }$out->$in ($why)"
		fi
	done
}

run_direction forward
run_direction reverse

echo
if [ -z "$PAIRS_FAILED" ]; then
	echo "All $TOTAL GPIO line pair tests passed."
	exit 0
fi

if [ "$PAIRS_PASSED" -eq 0 ]; then
	echo "Error: no line pair responded, in either direction -- all $TOTAL failed."
	echo "       Every input stayed at its idle level whatever was driven at the"
	echo "       other end. Two things look exactly like this and this test cannot"
	echo "       tell them apart on its own: a bench with no loopback wiring fitted,"
	echo "       and a driver whose output never reaches the pins. Fit the wiring"
	echo "       and run again -- if it still fails this way, the wiring is not the"
	echo "       explanation and the driver is."
	echo "       $PAIRS_FAILED"
	exit 2
fi

echo "Error: $PAIRS_PASSED of $TOTAL line pairs passed and the rest did not, so the"
echo "       wiring is present and these pairs are the fault:"
echo "       $PAIRS_FAILED"
exit 1
