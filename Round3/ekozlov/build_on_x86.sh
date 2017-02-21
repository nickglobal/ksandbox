#!/bin/bash -e


source ${HOME}/KernelWorkshop/ops/setup-xcompile-env.sh

export CROSS_COMPILE=${ARM_AM335X_TOOLCHAIN_PREFIX}
export ARCH=arm
DTB_PATH=${HOME}/KernelWorkshop/ops/bbb/module_dbg/KERNEL/arch/arm/boot/dts

MODULES=("ssd1306_drv.ko" "ssd1306_lcd_drv.ko" "mpu6050_reader.ko")
USERAPPS=()


BBBIP="192.168.7.2"

# parse commandline options
while [ ! -z "$1"  ] ; do
        case $1 in
           -h|--help)
                echo "TODO: help"
                ;;
            --clean)
                echo "Clean module sources"
                make clean
                ;;
            --module)
                echo "Build module"
                make
                ;;
            --user)
            	echo "Build userspace app"
            	make user
            	;;
            --deploy)
                echo "Deploy kernel module(s)"
                for M in ${MODULES[@]}; do
			deploy-on-bbb.exp $M
		done
                ;;
            --kconfig)
                echo "configure kernel"
                make config
                ;;
            
            --dtb)
                echo "configure kernel"
                make dtb
                deploy-on-bbb.exp $DTB_PATH/am335x-boneblack.dtb
                ;;
	    *)
		echo "No such command"
		exit
		;;
        esac
        shift
done
