#!/bin/bash

TIMEOUT="/sys/devices/platform/ocp/4819c000.i2c/i2c-2/2-003c/timer/timeout"
AUTOSTART="/sys/devices/platform/ocp/4819c000.i2c/i2c-2/2-003c/timer/autostart"
START="/sys/devices/platform/ocp/4819c000.i2c/i2c-2/2-003c/timer/start"

if [ ! -f ${TIMEOUT} ] || \
	[ ! -f ${AUTOSTART} ] || \
	[ ! -f ${START} ]; then
	echo "Kernel module seems to be not loaded...Terminating"
	exit 1
fi

if [ $# -ne 1 ] && [ $# -ne 2 ]; then
	echo "Illegal number of parameters..."
	echo "Usage: ./start_count <timeout> [autostart]"
	exit 2
fi

if [ $1 -le 0 ] || [ $1 -gt 999 ]; then
	echo "Timeout must be 1..999"
	exit 3
fi

if [ ! "$2" = "autostart" ] && [ ! -z "$2" ]; then
	echo -e "Second parameter must be \"autostart\" or empty"
	exit 3
fi

echo $1 | sudo tee ${TIMEOUT} > /dev/null
if [ "$2" = "autostart" ]; then
	echo "1" | sudo tee ${START} > /dev/null
else
	echo "0" | sudo tee ${START} > /dev/null
fi
