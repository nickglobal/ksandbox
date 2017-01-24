#  SandBox for Linux kernel drivers

# Round1: Develop driver for pass string from DTS to kernel log.
# Round2: GPIO control.
Beaglebonebalck has 4 on-board LED's you can check shematic page number 6.
https://beagleboard.org/static/beaglebone/latest/Docs/Hardware/BONE_SCH.pdf
in current software stack this LEDS controled by deriver placed here http://lxr.linux.no/#linux+v4.4/drivers/leds/leds-gpio.c


## The task for next loop are:
- diactivate current leds-gpio driver.
- add to driver ability to define GPIO poperty configuretion such gpios = <arg1 arg2 arg3>; etc..
- add new property in DTS for configure blinking timings in milliseconds, it shuld  support two paramiters one for turn on and second for turn off.
- led timing should be also configured via sysfs.

#Round 3: communication interfaces, 

##Task in short.
- build reference driver.
- define driver in DT file check driver. 
- extend driver for control for implement timer functionlity. 
- {stimer} for define timer value in seconds.
- {srun} for start timer down counting.
