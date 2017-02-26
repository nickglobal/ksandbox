CONFIG_MODULE_SIG=n

obj-m += ssd1306_drv.o
#obj-m += pkbbb.o
user_app := pkcons

KDIR ?= `pwd`/KERNEL/

PWD := $(shell pwd)

module:
		  $(MAKE) ARCH=arm  -C $(KDIR) M=$(PWD) modules

clean_module:
		  $(MAKE) ARCH=arm  -C $(KDIR) SUBDIRS=$(PWD) clean

config:
		$(MAKE) ARCH=arm -C $(KDIR) menuconfig

bbuild:
		$(MAKE) ARCH=arm -C $(KDIR)

dtb:
		$(MAKE) ARCH=arm -C $(KDIR) dtbs

app: $(user_app).c
	~/am335x/gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux/bin/arm-linux-gnueabihf-gcc -o $(user_app) $^
	scp $(user_app) debian@192.168.7.2:/home/debian/

clean_app:
	rm $(user_app)

.PHONY: clean_module default config dtb user clean_user
