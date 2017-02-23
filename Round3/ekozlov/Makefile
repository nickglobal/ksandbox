CONFIG_MODULE_SIG=n

# for fb_init_device
#KBUILD_EXTRA_SYMBOLS=

obj-m += ssd1306_drv.o

KDIR ?= $(ARM_AM335X_KERNEL)

PWD := $(shell pwd)


default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(ARM_AM335X_TOOLCHAIN_PREFIX)gcc --sysroot=$(ARM_AM335X_SYSROOT) -o mpu-display mpu-display.c -g -O0 -Wall -Werror -Wl,-rpath,. -lpthread

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

config:
	$(MAKE) -C $(KDIR) menuconfig

dtb:
	$(MAKE) -C $(KDIR) dtbs

.PHONY: clean default config dtb user
