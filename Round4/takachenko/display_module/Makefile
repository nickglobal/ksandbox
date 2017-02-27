CONFIG_MODULE_SIG=n

PWD := $(shell pwd)
KDIR ?= $(PWD)/../../KERNEL/

obj-m += ssd1306.o

default:
	$(MAKE) ARCH=arm -C $(KDIR) M=$(PWD)

clean:
	$(MAKE) ARCH=arm  -C $(KDIR) SUBDIRS=$(PWD) clean

config:
	$(MAKE) ARCH=arm -C $(KDIR) menuconfig

dtb:
	$(MAKE) ARCH=arm -C $(KDIR) dtbs

.PHONY: clean default config dtb
