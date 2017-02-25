/*
 * ssd1306 oled display driver
 *
 * 7-bit I2C slave addresses:
 *  0x3c (ADDR)
  *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>

#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "pkdisp.h"

static int __init pkdisp_init(void);
static void __exit pkdisp_exit(void);

#define DEVICE_NAME "ssd1306"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

#define SSD1306FB_DATA			0x40
#define SSD1306FB_COMMAND		0x80

#define SSD1306FB_SET_ADDRESS_MODE	0x20
#define SSD1306FB_SET_ADDRESS_MODE_HORIZONTAL	(0x00)
#define SSD1306FB_SET_ADDRESS_MODE_VERTICAL	(0x01)
#define SSD1306FB_SET_ADDRESS_MODE_PAGE		(0x02)
#define SSD1306FB_SET_COL_RANGE		0x21
#define SSD1306FB_SET_PAGE_RANGE	0x22
#define SSD1306FB_CONTRAST		0x81
#define	SSD1306FB_CHARGE_PUMP		0x8d
#define SSD1306FB_SEG_REMAP_ON		0xa1
#define SSD1306FB_DISPLAY_OFF		0xae
#define SSD1306FB_SET_MULTIPLEX_RATIO	0xa8
#define SSD1306FB_DISPLAY_ON		0xaf
#define SSD1306FB_START_PAGE_ADDRESS	0xb0
#define SSD1306FB_SET_DISPLAY_OFFSET	0xd3
#define	SSD1306FB_SET_CLOCK_FREQ	0xd5
#define	SSD1306FB_SET_PRECHARGE_PERIOD	0xd9
#define	SSD1306FB_SET_COM_PINS_CONFIG	0xda
#define	SSD1306FB_SET_VCOMH		0xdb

#define pkdisp_ctrl(client, cmd) { \
	i2c_smbus_write_byte_data(client, SSD1306FB_COMMAND, cmd); \
}

/* all display-related data is stored here */
static struct pkdisp disp = {0};

/* File operations for pkdisp char device */
static struct file_operations pkdisp_dev_ops = {
	.owner   = THIS_MODULE,
	.llseek  = pkdisp_llseek,
	.read	= pkdisp_read,
	.write   = pkdisp_write,
};

/* Init sequence taken from original driver drivers/video/fbdev/ssd1307fb.c */
static void ssd1306_init_lcd(struct i2c_client *client)
{
	/* contrast */
	pkdisp_ctrl(client, SSD1306FB_CONTRAST);
	pkdisp_ctrl(client, 255);

	/* set segment re-map */
	pkdisp_ctrl(client, SSD1306FB_SEG_REMAP_ON);
	pkdisp_ctrl(client, 0xC8); /* Set COM Output Scan Direction */

	/* set multiplex ratio(1 to 64) */
	pkdisp_ctrl(client, SSD1306FB_SET_MULTIPLEX_RATIO);
	pkdisp_ctrl(client, 0x3F);

	/* set display clock divide ratio/oscillator frequency */
	pkdisp_ctrl(client, SSD1306FB_SET_CLOCK_FREQ);
	pkdisp_ctrl(client, 0xa0);

	/* precharge period */
	pkdisp_ctrl(client, SSD1306FB_SET_PRECHARGE_PERIOD);
	pkdisp_ctrl(client, 0xD9);

	/* Set COM pins configuration */
	pkdisp_ctrl(client, SSD1306FB_SET_COM_PINS_CONFIG);
	pkdisp_ctrl(client, 0x12);

	/* set vcomh 0x20,0.77xVcc */
	pkdisp_ctrl(client, SSD1306FB_SET_VCOMH);
	pkdisp_ctrl(client, 0x20);

	/* -set DC-DC enable */
	pkdisp_ctrl(client, SSD1306FB_CHARGE_PUMP);
	pkdisp_ctrl(client, 0x14);

	/* address mode: horisontal */
	pkdisp_ctrl(client, SSD1306FB_SET_ADDRESS_MODE);
	pkdisp_ctrl(client, SSD1306FB_SET_ADDRESS_MODE_HORIZONTAL);

	/* set column addressing range */
	pkdisp_ctrl(client, SSD1306FB_SET_COL_RANGE);
	pkdisp_ctrl(client, 0);
	pkdisp_ctrl(client, DISP_W-1);

	/* set page addressing range */
	pkdisp_ctrl(client, SSD1306FB_SET_PAGE_RANGE);
	pkdisp_ctrl(client, 0);
	pkdisp_ctrl(client, 7);

	/* Turn display ON */
	pkdisp_ctrl(client, SSD1306FB_DISPLAY_ON);
}

static int ssd1306_probe(struct i2c_client *drv_client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;

	dev_err(&drv_client->dev, "init I2C driver\n");

	disp.client = drv_client;
	i2c_set_clientdata(drv_client, &disp);

	adapter = drv_client->adapter;

	if (!adapter) {
		dev_err(&drv_client->dev, "adapter indentification error\n");
		return -ENODEV;
	}

	dev_err(&drv_client->dev, "I2C client address %d\n", drv_client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&drv_client->dev, "operation not supported\n");
		return -ENODEV;
	}

	ssd1306_init_lcd(drv_client);

	dev_err(&drv_client->dev, "ssd1306 driver successfully loaded\n");

	return pkdisp_init();
}

static int ssd1306_remove(struct i2c_client *client)
{
	pkdisp_ctrl(client, SSD1306FB_DISPLAY_OFF); /* Turn display OFF */
	pkdisp_exit();
	return 0;
}

/* push picture to OLED */
void pkdisp_flush(void)
{
	int x, y;
	char *buf;

	buf = kzalloc(SSD1306_WIDTH*SSD1306_HEIGHT/8 + 1, GFP_KERNEL);
	if (!buf) {
		dev_err(&disp.client->dev, "error alloc disp buffer\n");
		return;
	}
	buf[0] = 0x40;

	for (y = 0; y < DISP_H; y++)
		for (x = 0; x < DISP_W; x++)
			if (disp.buf[y*DISP_W+x] > 127)
				buf[1 + x + (y / 8) * DISP_W] |= 1 << (y % 8);
			else
				buf[1 + x + (y / 8) * DISP_W] &= ~(1 << (y % 8));

	i2c_master_send(disp.client, buf, (SSD1306_WIDTH*SSD1306_HEIGHT/8 + 1));
	kfree(buf);
}

/* File operations */
loff_t  pkdisp_llseek(struct file *filep, loff_t off, int whence)
{
	loff_t newpos;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filep->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = DISP_BUFSIZE + off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0)
		newpos = 0;
	if (newpos >= DISP_BUFSIZE)
		newpos = DISP_BUFSIZE-1;
	filep->f_pos = newpos;
	return newpos;
}

ssize_t pkdisp_read(struct file *filep, char __user *data,
		size_t size, loff_t *offset)
{
	/* do not go out of buffer */
	if ((*offset+size) > DISP_BUFSIZE)
		size = DISP_BUFSIZE - *offset;

	if (copy_to_user(data, disp.buf + *offset, size))
		return -EFAULT;

	*offset += size;
	return size;
}

ssize_t pkdisp_write(struct file *filep, const char __user *data,
		size_t size, loff_t *offset)
{
	/* do not go out of buffer */
	if ((*offset+size) > DISP_BUFSIZE)
		size = DISP_BUFSIZE - *offset;

	if (copy_from_user(disp.buf + *offset, data, size))
		return -EFAULT;

	*offset += size;

	pkdisp_flush();

	return size;
}

static void pkdisp_unregister(void)
{
	if (!IS_ERR(disp.device))
		device_destroy(&disp.class, disp.dev);
	class_unregister(&disp.class);
	cdev_del(&disp.cdev);
	kfree(disp.buf);
	unregister_chrdev_region(disp.dev, 1);
}

static int __init pkdisp_init(void)
{
	int res;

	res = alloc_chrdev_region(&disp.dev, 0, 1, DISP_NAME);
	if (res < 0) {
		dev_err(&disp.client->dev, "error alloc chrdev\n");
		return res;
	}

	disp.buf = kzalloc(sizeof(char) * DISP_W * DISP_H, GFP_KERNEL);
	if (!disp.buf) {
		dev_err(&disp.client->dev, "error alloc memory for bufffer\n");
		goto fail;
	}

	cdev_init(&disp.cdev, &pkdisp_dev_ops);
	disp.cdev.owner = THIS_MODULE;
	res = cdev_add(&disp.cdev, disp.dev, 1);
	if (res) {
		dev_err(&disp.client->dev, "error add chdev\n");
		goto fail;
	}

	disp.class.name = DISP_NAME;
	disp.class.owner = THIS_MODULE;
	res = class_register(&disp.class);
	if (res) {
		dev_err(&disp.client->dev, "fail register class\n");
		goto fail;
	}

	disp.device = device_create(&disp.class, NULL,
			disp.dev, NULL, DISP_NAME);
	if (IS_ERR(disp.device)) {
		res = PTR_ERR(disp.device);
		goto fail;
	}

	dev_info(&disp.client->dev, "The \"%s\" device registered (%d:%d)\n",
			DISP_NAME, MAJOR(disp.dev), MINOR(disp.dev));
	return 0;

fail:
	pkdisp_unregister();
	return res;
}

static void __exit pkdisp_exit(void)
{
	pkdisp_unregister();
	dev_info(&disp.client->dev, "The \"%s\" device removed\n",
			DISP_NAME);
}


/* i2c device_id structure */
static const struct i2c_device_id ssd1306_idtable[] = {
	{ "ssd1306", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssd1306_idtable);

static struct i2c_driver ssd1306_driver = {
	.probe	   = ssd1306_probe,
	.remove	  = ssd1306_remove,
	.id_table	= ssd1306_idtable,
	.driver = {
		.name = DEVICE_NAME,
	 },
};

module_i2c_driver(ssd1306_driver);

MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("Peter Kulakov");
MODULE_LICENSE("GPL");
