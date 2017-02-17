/*
 * Gyro mpu6050 to ssd1306 printing module.
 * O. Bobro
 *
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/i2c.h>

//#define PRINT_ACCEL_ONLY		// Uncomment this line to print accelerometer's XYZ only
//#define PRINT_GYRO_ONLY		// Uncomment this line to print gyroscope's XYZ only


#ifdef PRINT_ACCEL_ONLY
# undef PRINT_GYRO_ONLY
#endif

#if defined(PRINT_ACCEL_ONLY) || defined(PRINT_GYRO_ONLY)
# define VALUES_COUNT 3
#else
# define VALUES_COUNT 6
#endif

//#define READ_PROCESSED_CHANEL

extern int ssd1306_print_XYZ(void *ssd1306, int *vals, int count, int print_accel_only_flag);

static struct device *gyroDevice = NULL;
static struct device *lcdDevice = NULL;
static struct task_struct *tsk;

typedef struct
{
	struct iio_dev *indioDevOfGyro;
	void *ssd1306;
} POINTERS;

POINTERS pointers;

typedef struct device_compare
{
	const char *dev_of_node_name;
	const char *dev_driver_name;
	const char *dev_name;
} DEVICE_COMPARE;

const DEVICE_COMPARE deviceCompareMPU6050 =
{
		.dev_of_node_name = "san_gyro0",
		.dev_driver_name = "inv-mpu6050-i2c",
		.dev_name = "1-0068",
};

const DEVICE_COMPARE deviceCompareSsd1306 =
{
		.dev_of_node_name = "san_lcd0",
		.dev_driver_name = "ssd1306_drv_SanB",
		.dev_name = "2-003c",
};

static int matchDeviceFunc(struct device *dev, void *data)
{
	const struct device_compare *dc = (const struct device_compare *)data;

	if (unlikely(!dev || !dc))
		return 0;

	if (unlikely(!dev->of_node || !dev->driver))
	{
//		printk(KERN_ERR "dev->of_node = [%p], dev->driver = [%p]\n", dev->of_node, dev->driver);
		return 0;
	}

	if (unlikely(!dev->of_node->name || !dev->driver->name))
	{
//		printk(KERN_ERR "dev->of_node->name = [%p], dev->driver->name = [%p]\n", dev->of_node->name, dev->driver->name);
		return 0;
	}

	return (!strcmp( dc->dev_of_node_name, dev->of_node->name )
			&& !strcmp( dc->dev_driver_name, dev->driver->name )
			&& !strcmp( dc->dev_name, dev_name(dev) )
			);
}

static int mpu6050_to_ssd1306_kthread( void *data )
{
	int ret, i;
	int vals[VALUES_COUNT] = {0};

	struct iio_channel iioChan = {
			.indio_dev = ((POINTERS *)data)->indioDevOfGyro,
			.data = NULL,
	};

	while (!kthread_should_stop())
	{
		for (i=0; i<VALUES_COUNT; i++)
		{
			if (kthread_should_stop())
				return 0;
			iioChan.channel = iioChan.indio_dev->channels + i
#ifdef PRINT_ACCEL_ONLY
					+ 3
#endif
					+ 2;
//			printk(KERN_INFO "before iio_read_channel_raw(%d): mdelay = %d\n", i, mdelay);
#ifndef READ_PROCESSED_CHANEL
			ret = iio_read_channel_raw(&iioChan, &vals[i]);
#else
			ret = iio_read_channel_processed(&iioChan, &vals[i]);
#endif
//			printk(KERN_INFO "iio_read_channel_raw(%d): val = %d, ret = %d\n", i, vals[i], ret);
		}
		if (kthread_should_stop())
			break;
#ifdef PRINT_ACCEL_ONLY
		ret = ssd1306_print_XYZ(((POINTERS *)data)->ssd1306, vals, VALUES_COUNT, 1);
#else
		ret = ssd1306_print_XYZ(((POINTERS *)data)->ssd1306, vals, VALUES_COUNT, 0);
#endif
		if (unlikely(ret != 0))
		{
			printk(KERN_ERR "ssd1306_print_XYZ() has returned ERROR = %d\n", ret);
			return ret;
		}
		if (kthread_should_stop())
			break;
	}

	return 0;
}


/* Module init */
static int __init mpu6050_to_ssd1306_init ( void )
{
    struct iio_dev *indioDevOfGyro;
    void *ssd1306;//    struct ssd1306_data *ssd1306;

    printk(KERN_INFO "mpu6050_to_ssd1306: module init\n");

    gyroDevice = bus_find_device(&i2c_bus_type, NULL, (void *)&deviceCompareMPU6050, matchDeviceFunc);
    if (unlikely(!gyroDevice || IS_ERR(gyroDevice)))
    {
    	printk(KERN_ERR "Device \"mpu6050\" with driver \"inv-mpu6050-i2c\" is not found in system!\n");
    	return -ENODEV;
    }
    printk(KERN_DEBUG "gyroDevice = [%p], dev_driver_string = [%s]\n", gyroDevice, dev_driver_string(gyroDevice));

    indioDevOfGyro = dev_get_drvdata(gyroDevice);
    if (unlikely(!indioDevOfGyro))
    {
    	put_device(gyroDevice);
    	printk(KERN_ERR "ERROR: indioDevOfGyro is NULL!\n");
    	return -1;
    }
    printk(KERN_DEBUG "indioDevOfGyro = [%p]\n", indioDevOfGyro);


    lcdDevice = bus_find_device(&i2c_bus_type, NULL, (void *)&deviceCompareSsd1306, matchDeviceFunc);
    if (unlikely(!lcdDevice || IS_ERR(lcdDevice)))
    {
    	put_device(gyroDevice);
    	printk(KERN_ERR "Device \"ssd1306\" is not found in system!\n");
    	return -ENODEV;
    }
    printk(KERN_DEBUG "lcdDevice = [%p], dev_driver_string = [%s]\n", lcdDevice, dev_driver_string(lcdDevice));

    ssd1306 = i2c_get_clientdata(to_i2c_client(lcdDevice));
    if (unlikely(!ssd1306))
    {
    	put_device(gyroDevice);
    	put_device(lcdDevice);
    	printk(KERN_ERR "ERROR: ssd1306 is NULL!\n");
    	return -1;
    }
    printk(KERN_DEBUG "ssd1306 = [%p]\n", ssd1306);

    pointers.indioDevOfGyro = indioDevOfGyro;
    pointers.ssd1306 = ssd1306;

    tsk = kthread_run(mpu6050_to_ssd1306_kthread, (void *)&pointers, "mpu6050_to_ssd1306_kthread");
    if (IS_ERR(tsk))
    {
    	put_device(gyroDevice);
    	put_device(lcdDevice);
    	printk(KERN_ERR "ERROR: kthread_run() has returned error %ld!\n", PTR_ERR(tsk));
    	return -1;
    }

    printk(KERN_INFO  "mpu6050_to_ssd1306 module successfully loaded!\n");
    return 0;
}

/* Module exit */
static void __exit mpu6050_to_ssd1306_exit(void)
{
    printk(KERN_INFO "mpu6050_to_ssd1306: cleanup\n");
    if (likely(tsk))
    {
    	kthread_stop(tsk);
    	tsk = NULL;
    }
    if (likely(gyroDevice))
    {
    	put_device(gyroDevice);
    	gyroDevice = NULL;
    }
    if (likely(lcdDevice))
    {
    	put_device(lcdDevice);
    	lcdDevice = NULL;
    }
    printk(KERN_INFO  "mpu6050_to_ssd1306 module successfully unloaded.\n");
}

/* -------------- kernel thread description -------------- */
module_init(mpu6050_to_ssd1306_init);
module_exit(mpu6050_to_ssd1306_exit);


MODULE_DESCRIPTION("Gyro mpu6050 to ssd1306 printing module");
MODULE_AUTHOR("SanB (O.Bobro)");
MODULE_LICENSE("GPL");
