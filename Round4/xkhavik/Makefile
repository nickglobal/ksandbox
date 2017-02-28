#ARM_AM335X_SYSROOT = /home/xkhavik/am335x/sysroot-glibc-linaro-2.21-2016.05-arm-linux-gnueabihf
all:
	$(CC) --sysroot=$(ARM_AM335X_SYSROOT) -o user_app user_app.c -g -O0 -lrt
clean:
	rm user_app

