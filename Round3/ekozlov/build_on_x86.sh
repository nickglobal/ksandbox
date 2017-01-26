#!/bin/bash -e

export CROSS_COMPILE=/home/me/KernelWorkshop/ops/bbb/module_dbg/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
export ARCH=arm

MODNAME="ssd1306_drv.ko"

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
            --deploy)
                echo "Deploy kernel module"
                scp $MODNAME debian@${BBBIP}:/home/debian/
                ;;
            --kconfig)
                echo "configure kernel"
                make config
                ;;
            
            --dtb)
                echo "configure kernel"
                make dtb
                scp /home/user/KernelWorkshop/ops/bbb/module_dbg/KERNEL/arch/arm/boot/dts/am335x-boneblack.dtb debian@${BBBIP}:/home/debian/
                ;;
        esac
        shift
done

echo "Done!"
