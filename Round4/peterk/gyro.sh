#!/bin/sh

gyro=/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/

while [ 1 ]
do
	lx=accel_x=$(cat $gyro/in_accel_x_raw)
	ly=accel_x=$(cat $gyro/in_accel_y_raw)
	lz=accel_x=$(cat $gyro/in_accel_z_raw)
	ax=anglvel_x=$(cat $gyro/in_anglvel_x_raw)
	ay=anglvel_y=$(cat $gyro/in_anglvel_y_raw)
	az=anglvel_z=$(cat $gyro/in_anglvel_z_raw)

	sudo ./pkcons $lx $ly $lz " " $ax $ay $az
	
	sleep 0.3
done
