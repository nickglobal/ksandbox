CONFIG_MODULE_SIG=n

TARGET_MODULE = coremodprop
obj-m += $(TARGET_MODULE).o

KDIR ?= $(HOME)/KernelWorkshop/ops/bbb/module_dbg/KERNEL

PWD := $(shell pwd)

default:
	$(MAKE) ARCH=arm  -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=arm  -C $(KDIR) SUBDIRS=$(PWD) clean

config:
	$(MAKE) ARCH=arm -C $(KDIR) menuconfig

dtb:
	$(MAKE) ARCH=arm -C $(KDIR) dtbs

verify: $(TARGET_MODULE).c
	$(KDIR)/scripts/checkpatch.pl -f $(TARGET_MODULE).c 

.PHONY: clean default config dtb verify
