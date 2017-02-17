#!/bin/bash

sudo dmesg -C
echo ""
sudo insmod ssd1306_drv_SanB.ko
sudo insmod mpu6050_to_ssd1306.ko
sleep 1
dmesg
