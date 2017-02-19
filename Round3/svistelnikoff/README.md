# Globallogic Linux Kernel classes

# Round3 practice work
Goals: Communication interfages (I2C), interrupt handling and delayed processing.
Schematic: hhttps://github.com/CircuitCo/BeagleBone-Black/blob/master/BBB_SCH.pdf?raw=true

## Tasks
- build reference driver
- define driver in DST file, check driver
- extend driver to add timer functionlity. After driver load, display showing initial value and then start counting with period 1s
- for schedule update dispaly action shuld be used timers or delayed workqueue
- optional: use sysfs for initializing timeout value and for start timer counting

### Notes
DTS section:

	ssd1306_lcd: ssd1306_lcd@3c {
		compatible = "ssd1306";
		reg = <0x3c>;
		autostart = "false";
		start = <0>;
		timeout = <0>;
	};

sysfs path to **timer** contol file

	debian@beaglebone:/sys/devices/platform/ocp/4819c000.i2c/i2c-2/2-003c/timer

* **autostart**	- accepts *true* or *false*
* **start**		- *0* - stops counting, *other* - starts counting
* **timeout**	- timeout counting value

To start counting load *practice_3.ko* module and run ./start_count.sh <timeout value> autostart