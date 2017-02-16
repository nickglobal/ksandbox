#!/bin/sh
insmod ssd1306_drv.ko
while [ 1 ] ; do sudo ./prog $(date); sleep 1; done
