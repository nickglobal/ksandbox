CONFIG_MODULE_SIG=n

TARGET :=practice_1
OUTPUT :=build

obj-m += $(TARGET).o

$(TARGET)-objs := coremod.o training_leds.o

#CFLAGS_coremod.o := -std=c99
CFLAGS_training_leds.o := -Wno-error=incompatible-pointer-types

KDIR = $(BBB_KERNEL)

PWD := $(shell pwd)

all: dirs
		$(MAKE) ARCH=arm  CROSS_COMPILE=$(CC) -C $(KDIR) M=$(PWD) modules

clean:
		$(MAKE) ARCH=arm  CROSS_COMPILE=$(CC) -C $(KDIR) SUBDIRS=$(PWD) clean

config:
		$(MAKE) ARCH=arm CROSS_COMPILE=$(CC) -C $(KDIR) menuconfig

dtb:
		$(MAKE) ARCH=arm CROSS_COMPILE=$(CC) -C $(KDIR) dtbs
		
deploy:	clean all
		cd $(PWD) && sshpass -e scp *.ko bbb:./Projects/$(TARGET)/
		
deploy_dtb: dtb
		sshpass -e scp $(BBB_DTB_FILE) bbb:
		
dirs:
		mkdir -p $(PWD)/$(OUTPUT)

.PHONY: clean all config deploy deploy_dtb dtb dirs
.DEFAULT_GOAL: all
