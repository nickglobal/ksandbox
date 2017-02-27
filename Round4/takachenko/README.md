# Tools to show MPU6050 indicators on the SSD1306 display

# Parts:

- default MPU6050 driver;
- SSD1306 custom driver;
- user-space application (gyro2disp);
- modified am335x-bone-common.dtsi conifurationfile.

# Setup:

- connect MPU6050 to i2c-1;
- connect SSD1306 to i2c-2;
- prepare custom am335x-boneblack.dts and put it to the /boot/dts/%%KERNELVERSION%%/ folder on the BeagleBoneBlack;
- build and load the ssd1306.ko kernel module;
- build and run the gyro2disp user-space application;
- observe display

# Applicaiton options:

- DEFAUL: read & show accelerometer indicators every 100 ms
- option "-m" (mode): [acc] - accelerometer (same as default) / [angvel]- read angular velocity indicators, [temp] - temperature;
- option "-p" (polling): [file] - poll file change (do not perfrom reading operation);
- option "-u" (update): [onchange] - update screen only if change detected, [XX] - custom update screen interval in ms.

# How it works:

Application reads MPU6050 indicators according to selected mode and shows them on the screen.
SysFs interfaces used to interact with the MPU6050, IOCTL & MMAP used for interaction with the display.

