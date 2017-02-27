/* Hardware configuration */
#ifndef HWCONFIG_H
#define HWCONFIG_H

/* Gyroscope */

/**
 * SysFS Files
 */

#define HW_ACC_X_PATH     "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_accel_x_raw"
#define HW_ACC_Y_PATH     "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_accel_y_raw"
#define HW_ACC_Z_PATH     "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_accel_z_raw"
#define HW_ACC_SCALE_PATH "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_accel_scale"

#define HW_GYRO_X_PATH     "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_anglvel_x_raw"
#define HW_GYRO_Y_PATH     "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_anglvel_y_raw"
#define HW_GYRO_Z_PATH     "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_anglvel_z_raw"
#define HW_GYRO_SCALE_PATH "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_anglvel_scale"

#define HW_TEMP_PATH        "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_temp_raw"
#define HW_TEMP_SCALE_PATH  "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_temp_scale"
#define HW_TEMP_OFFSET_PATH "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0/in_temp_offset"

/* I2C */
#define HW_DEFAULT_GYRO_ADDR         0x68
#define HW_DEFAULT_GYRO_I2C_BUS_PATH "/dev/i2c-1"

#define MPU6050_RA_USER_CTRL  0x6A
#define MPU6050_RA_FIFO_EN    0x23
#define MPU6050_RA_INT_ENABLE 0x38

#define MPU6050_TEMP_FIFO_EN_BIT  7
#define MPU6050_XG_FIFO_EN_BIT    6
#define MPU6050_YG_FIFO_EN_BIT    5
#define MPU6050_ZG_FIFO_EN_BIT    4
#define MPU6050_ACCEL_FIFO_EN_BIT 3
#define MPU6050_SLV2_FIFO_EN_BIT  2
#define MPU6050_SLV1_FIFO_EN_BIT  1
#define MPU6050_SLV0_FIFO_EN_BIT  0

#define MPU6050_USERCTRL_DMP_EN_BIT         7
#define MPU6050_USERCTRL_FIFO_EN_BIT        6
#define MPU6050_USERCTRL_I2C_MST_EN_BIT     5
#define MPU6050_USERCTRL_I2C_IF_DIS_BIT     4
#define MPU6050_USERCTRL_DMP_RESET_BIT      3
#define MPU6050_USERCTRL_FIFO_RESET_BIT     2
#define MPU6050_USERCTRL_I2C_MST_RESET_BIT  1
#define MPU6050_USERCTRL_SIG_COND_RESET_BIT 0

#define MPU6050_INTERRUPT_FF_BIT            7
#define MPU6050_INTERRUPT_MOT_BIT           6
#define MPU6050_INTERRUPT_ZMOT_BIT          5
#define MPU6050_INTERRUPT_FIFO_OFLOW_BIT    4
#define MPU6050_INTERRUPT_I2C_MST_INT_BIT   3
#define MPU6050_INTERRUPT_PLL_RDY_INT_BIT   2
#define MPU6050_INTERRUPT_DMP_INT_BIT       1
#define MPU6050_INTERRUPT_DATA_RDY_BIT      0


/* Display */

/**
 * Parameters
 */
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_BITS_PER_PIXEL 1

#define MMAP_FILE_NAME "ssd1306_mmap"

#define IOC_MAGIC           0x13
#define IOCTL_UPDATE_SCREEN _IO( IOC_MAGIC, 1)

#define MMAP_BUFFER_SIZE 4096 /* 4K */

#endif /* HWCONFIG_H */
