CONFIG_MODULE_SIG=n

# for fb_init_device
#KBUILD_EXTRA_SYMBOLS=

obj-m += mpu6050_reader.o #ssd1306_drv.o #ssd1306_lcd_drv.o

KDIR ?= $(ARM_AM335X_KERNEL)

PWD := $(shell pwd)


default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(ARM_AM335X_TOOLCHAIN_PREFIX)gcc --sysroot=$(ARM_AM335X_SYSROOT) -o mpu-display mpu-display.c -g -O0 -Wall -Wl,-rpath,. -lpthread
	@#-Xlinker $(LIBPATH)/libc.a $(LIBPATH)/librt.a $(LIBPATH)/libc_nonshared.a

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

config:
	$(MAKE) -C $(KDIR) menuconfig

dtb:
	$(MAKE) -C $(KDIR) dtbs

.PHONY: clean default config dtb user
