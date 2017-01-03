#!/bin/bash -e

export CROSS_COMPILE=$HOME/KernelWorkshop/ops/bbb/module_dbg/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

MODNAME="coremodprop.ko"

# parse commandline options
while [ ! -z "$1"  ] ; do
        case $1 in
            -h|--help)
                echo "TODO: help"
                ;;
            --clean)
                echo "Clean module sources"
                make ARCH=arm clean
                ;;
            --module)
                echo "Build module"
                make ARCH=arm
                ;;
            --deploy)
                echo "Deploy kernel module"
                scp $MODNAME debian@192.168.7.2:/home/debian/
                ;;
            --kconfig)
                echo "configure kernel"
                make ARCH=arm config
                ;;
            --dtb)
                echo "configure kernel"
                make ARCH=arm dtb
                ;;
            --verify)
                echo "Verifying the kernel compliance"
                make verify
                ;;
            *)
                echo "No such operation"
                exit
                ;;
        esac
        shift
done

echo "Done!"
