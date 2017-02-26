#!/bin/bash -e

export CROSS_COMPILE=`pwd`/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

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
                make ARCH=arm clean_module
                ;;
            --module)
                echo "Build module"
                make ARCH=arm
                ;;
            --deploy)
                echo "Deploy kernel module"
                scp $MODNAME debian@${BBBIP}:/home/debian/
                ;;
            --kconfig)
                echo "configure kernel"
                make ARCH=arm config
                ;;
            
            --bbuild)
                echo "build kernel"
                make ARCH=arm bbuild
                ;;
            
            --dtb)
                echo "configure kernel"
                make ARCH=arm dtb
                scp KERNEL/arch/arm/boot/dts/am335x-boneblack.dtb debian@${BBBIP}:/home/debian/
                ;;

            --app)
                echo "build user app"
                echo $CROSS_COMPILE
                make app
                scp pkdisp debian@${BBBIP}:/home/debian/
                ;;
            --clean_app)
                echo "Clean user app"
                make clean_app
                ;;
        esac
        shift
done

echo "Done!"
