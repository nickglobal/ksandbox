#!/bin/bash -e

BASE=${HOME}/KernelWorkshop/linaro-toolchain
export ARM_AM335X_TOOLCHAIN_PREFIX=${BASE}/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
export ARM_AM335X_SYSROOT=${BASE}/sysroot-glibc-linaro-2.21-2016.05-arm-linux-gnueabihf
export ARM_AM335X_ARCH=arm
export ARM_AM335X_KERNEL=${HOME}/KernelWorkshop/ops/bbb/module_dbg/KERNEL
