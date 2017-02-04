CONFIG_MODULE_SIG=n

#export EXTRA_CFLAGS += -std=c90
obj-m += ssd1306_SanB.o

KDIR ?= `pwd`/KERNEL/

PWD := $(shell pwd)

default:
		  $(MAKE) ARCH=arm  -C $(KDIR) M=$(PWD) modules

clean:
		  $(MAKE) ARCH=arm  -C $(KDIR) SUBDIRS=$(PWD) clean

config:
		$(MAKE) ARCH=arm -C $(KDIR) menuconfig

dtb:
		$(MAKE) ARCH=arm -C $(KDIR) dtbs

.PHONY: clean default config dtb
