CONFIG_MODULE_SIG=n

TARGET_MODULE = kmod_led_ctrl
obj-m += $(TARGET_MODULE).o

KDIR ?= $(HOME)/KernelWorkshop/ops/bbb/module_dbg/KERNEL

PWD := $(shell pwd)

#export CROSS_COMPILE := /home/me/KernelWorkshop/ops/bbb/module_dbg/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#export ARCH := arm

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

deploy:
	./deploy.exp $(TARGET_MODULE).ko
#config:
#	$(MAKE) -C $(KDIR) menuconfig

dtb:
	$(MAKE) -C $(KDIR) dtbs

verify: $(TARGET_MODULE).c
	$(KDIR)/scripts/checkpatch.pl -f $(TARGET_MODULE).c 

#.PHONY: clean default config dtb verify
.PHONY: clean default deploy