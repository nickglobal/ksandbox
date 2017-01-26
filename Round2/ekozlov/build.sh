export CROSS_COMPILE=/home/me/KernelWorkshop/ops/bbb/module_dbg/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
export ARCH=arm

#MODNAME=kmod_led_ctrl

# parse commandline options
while [ ! -z "$1"  ] ; do
	case $1 in
	--clean)
		echo "cleaning the module sources..."
		make clean
		;;
	--module)
		echo "building the kernel module..."
		make
		;;
	--deploy)
		echo "deploying the kernel module..."
		make deploy
		;;
	--dtb)
		echo "configure kernel"
		make dtb
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

echo "done."
