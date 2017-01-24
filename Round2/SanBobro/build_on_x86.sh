#!/bin/bash -e

export CROSS_COMPILE=`pwd`/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

MODNAME="sanB_leds.ko" #"san_mod.ko"
DTBNAME=`pwd`"/KERNEL/arch/arm/boot/dts/am335x-boneblack.dtb"


DTSI_LOCALNAME="am335x-boneblack_SanB.dtsi"
DTSINAME=`pwd`"/KERNEL/arch/arm/boot/dts/"$DTSI_LOCALNAME

if [ $# -eq 0 ] ; then
    echo "ERROR: No parameters were found!"
    exit
fi

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
                #echo $DTBNAME
                scp $MODNAME $DTBNAME debian@192.168.7.2:~/
                ;;
            --kconfig)
                echo "configure kernel"
                make ARCH=arm config
                ;;
            --dtb)
                echo "configure kernel: DTBs rebuilding..."
                cp $DTSI_LOCALNAME $DTSINAME
                make ARCH=arm dtb
                ;;
            --cpdtsi)
                echo "$DTSI_LOCALNAME coping to $DTSINAME"
                cp $DTSI_LOCALNAME $DTSINAME
                ;;
            *)
                echo "ERROR: Incorrect parameter is found, exit!"
                exit
                ;;
        esac
        shift
done

echo "Done!"
