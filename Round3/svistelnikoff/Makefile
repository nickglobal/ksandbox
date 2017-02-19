CONFIG_MODULE_SIG=n

TARGET :=practice_3
#OUTPUT :=build

obj-m += $(TARGET).o

$(TARGET)-objs := ssd1306_drv.o ssd1306.o

#CFLAGS_coremod.o := -std=c99
CFLAGS_training_leds.o := -Wno-error=incompatible-pointer-types

KDIR = $(BBB_KERNEL)

PWD := $(shell pwd)

SRC := $(wildcard *.c)
EXCLUEDS := $(wildcard *.mod.c)
SRC := $(filter-out $(EXCLUEDS), $(SRC))
SOURCES = $(addprefix $(PWD)/, $(SRC))

all:
		$(MAKE) ARCH=arm  CROSS_COMPILE=$(CC) -C $(KDIR) M=$(PWD) modules

clean:
		$(MAKE) ARCH=arm  CROSS_COMPILE=$(CC) -C $(KDIR) SUBDIRS=$(PWD) clean

config:
		$(MAKE) ARCH=arm CROSS_COMPILE=$(CC) -C $(KDIR) menuconfig

dtb:
		$(MAKE) ARCH=arm CROSS_COMPILE=$(CC) -C $(KDIR) dtbs
		
deploy:	clean all
		cd $(PWD) && sshpass -e scp *.ko *.sh bbb:./Projects/$(TARGET)/
		
deploy_dtb: dtb
		sshpass -e scp $(BBB_DTB_FILE) bbb:

check:
		cd $(KDIR) && scripts/checkpatch.pl \
		--max-line-length=120 --no-tree --terse --file $(SOURCES)
		
.PHONY: clean all config deploy deploy_dtb dtb check
.DEFAULT_GOAL: all
