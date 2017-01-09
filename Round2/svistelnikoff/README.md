# Globallogic Linux Kernel classes

# Round1 and Round2 practice work
Goals: Develop driver for pass string from DTS to kernel log, implement GPIO control.
Schematic: https://beagleboard.org/static/beaglebone/latest/Docs/Hardware/BONE_SCH.pdf
Current software stack this LEDS controled by deriver placed here http://lxr.linux.no/#linux+v4.4/drivers/leds/leds-gpio.c

## Tasks
- deactivate current leds-gpio driver.
- add to driver ability to define GPIO poperty configuretion such gpios = <arg1 arg2 arg3>; etc..
- add new property in DTS for configure blinking timings in milliseconds, it should  support two parameters one for turn on and second for turn off.
- led timing should be also configured via sysfs.