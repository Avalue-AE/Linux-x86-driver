#!/bin/bash

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

GPIO_CHIP=$(gpiodetect | grep '\[gpio\]' | grep -oP 'gpiochip\d+')
if [ -z "$GPIO_CHIP" ]; then
	echo "No GPIO chip found. Please check your GPIO setup."
	exit 1
fi
echo "Found GPIO chip: $GPIO_CHIP"

GPIO_LINES=$(gpiodetect | grep '\[gpio\]' | grep -oP 'gpiochip\d+.*\(\d+ lines\)' | grep -oP '\d+(?= lines)')
if [ $GPIO_LINES -eq 0 ]; then
	echo "No GPIO lines found. Please check your GPIO setup."
	exit 1
fi
echo "Found GPIO lines: $GPIO_LINES"


# Test each GPIO line pair: first half as outputs, second half as inputs.
# gpioset is run in the background to hold the driven value while gpioget reads it,
# then the background process is killed to release the line before the next iteration.
HALF=$((GPIO_LINES / 2))
for i in $(seq 0 $((HALF - 1))); do
	OUTPUT_PIN=$i
	INPUT_PIN=$((i + HALF))
	echo -n "Testing GPIO line pair: output=$OUTPUT_PIN <--> input=$INPUT_PIN ... "

	# Drive output LOW, read back input
	gpioset $GPIOSET_HOLD $GPIO_CHIP $OUTPUT_PIN=0 &
	GPIOSET_PID=$!
	sleep 0.1
	INPUT_VALUE=$(gpioget $GPIO_CHIP $INPUT_PIN)
	kill $GPIOSET_PID 2>/dev/null
	wait $GPIOSET_PID 2>/dev/null

	if [ "$INPUT_VALUE" -ne 0 ]; then
		echo "Error: GPIO line $OUTPUT_PIN test failed. Expected 0, got $INPUT_VALUE."
		exit 1
	fi

	# Drive output HIGH, read back input
	gpioset $GPIOSET_HOLD $GPIO_CHIP $OUTPUT_PIN=1 &
	GPIOSET_PID=$!
	sleep 0.1
	INPUT_VALUE=$(gpioget $GPIO_CHIP $INPUT_PIN)
	kill $GPIOSET_PID 2>/dev/null
	wait $GPIOSET_PID 2>/dev/null

	if [ "$INPUT_VALUE" -ne 1 ]; then
		echo "Error: GPIO line $OUTPUT_PIN test failed. Expected 1, got $INPUT_VALUE."
		exit 1
	fi

	echo "PASSED"
done

# Reverse the test: first half as inputs, second half as outputs.
for i in $(seq 0 $((HALF - 1))); do
	INPUT_PIN=$i
	OUTPUT_PIN=$((i + HALF))
	echo -n "Testing GPIO line pair: input=$INPUT_PIN <--> output=$OUTPUT_PIN ... "

	# Drive output LOW, read back input
	gpioset $GPIOSET_HOLD $GPIO_CHIP $OUTPUT_PIN=0 &
	GPIOSET_PID=$!
	sleep 0.1
	INPUT_VALUE=$(gpioget $GPIO_CHIP $INPUT_PIN)
	kill $GPIOSET_PID 2>/dev/null
	wait $GPIOSET_PID 2>/dev/null

	if [ "$INPUT_VALUE" -ne 0 ]; then
		echo "Error: GPIO line $OUTPUT_PIN test failed. Expected 0, got $INPUT_VALUE."
		exit 1
	fi

	# Drive output HIGH, read back input
	gpioset $GPIOSET_HOLD $GPIO_CHIP $OUTPUT_PIN=1 &
	GPIOSET_PID=$!
	sleep 0.1
	INPUT_VALUE=$(gpioget $GPIO_CHIP $INPUT_PIN)
	kill $GPIOSET_PID 2>/dev/null
	wait $GPIOSET_PID 2>/dev/null

	if [ "$INPUT_VALUE" -ne 1 ]; then
		echo "Error: GPIO line $OUTPUT_PIN test failed. Expected 1, got $INPUT_VALUE."
		exit 1
	fi

	echo "PASSED"
done

echo "All $HALF GPIO line pair tests passed."

