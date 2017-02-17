Due to the fact that an exact task has not been formulated, here is my implementation based on my opinion.

Here are 2 kernel modules: ssd1306_drv_SanB.ko and mpu6050_to_ssd1306.ko

ssd1306_drv_SanB.ko - is the GL ssd1306_drv.ko driver for ssd1306 display with some changes/extensions:
  1). Screen updating is accelerated by using functions that write data blocks (and not 1 byte) into i2c device.
  2). The function 'ssd1306_print_XYZ()' is added and EXPORTed. It prints on ssd1306 display the data from the Gyroscope device.

mpu6050_to_ssd1306.ko - is a kernel module that works between inv_mpu6050 driver and ssd1306 driver.
  This module asks the values from the mpu6050 Gyroscope and calls the function 'ssd1306_print_XYZ()' to print these values.
  mpu6050_to_ssd1306.ko depends on ssd1306_drv_SanB.ko. 
  Also 'inv_mpu6050' and 'inv_mpu6050_i2c' default drivers from 4.4.39-ti-r76 Kernel are required (they should be loaded automaticaly). 

To test these modules on BB please run the 'test_gyro_printing.sh' script or do the following:
	$ sudo insmod ssd1306_drv_SanB.ko
	$ sudo insmod mpu6050_to_ssd1306.ko

Tested on 4.4.39-ti-r76 Kernel on BeagleBone Black device.

!!! All operations occur on the Kernel Level, User Space is not used. !!!

Author: O. Bobro. (SanBobro)