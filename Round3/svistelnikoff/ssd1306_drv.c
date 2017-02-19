/*
 * ssd1306_drv.c
 *
 *  Created on: Jan 28, 2017
 *      Author: vvs
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm-generic/errno-base.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "ssd1306.h"

#define DRIVER_LIC		"GPL"
#define DRIVER_AUTHOR	"Vladimir Svistelnikov <svistelnikoff@gmail.com>"
#define DRIVER_DESC		"SSD1306 LCD I2C driver"
#define DRIVER_NAME		"SSD1306 Driver"

/* LinuxWS/module_dbg/KERNEL/arch/arm/boot/dts/am335x-bone-common.dtsi */
/* lxr -> Documentation/i2c/writing-clients */

static int ssd1306_probe(struct i2c_client *drv_client,
						const struct i2c_device_id *id)
{
	struct ssd1306_data *ssd1306;
	struct i2c_adapter *adapter;

	dev_info(&drv_client->dev, "init I2C driver\n");

	ssd1306 = devm_kzalloc(&drv_client->dev, sizeof(struct ssd1306_data),
						GFP_KERNEL);
	dev_info(&drv_client->dev, "lcd data allocated %p", ssd1306);

	if (!ssd1306)
	return -ENOMEM;

	ssd1306->client = drv_client;
	ssd1306->status = 0xABCD;
	ssd1306_register_attributes(ssd1306);
	ssd1306_init_timer(ssd1306);

	i2c_set_clientdata(drv_client, ssd1306);

		adapter = drv_client->adapter;

	if (!adapter) {
		dev_err(&drv_client->dev, "adapter indentification error\n");
		return -ENODEV;
	}

	dev_info(&drv_client->dev, "i2c client address %d\n", drv_client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&drv_client->dev, "operation not supported\n");
		return -ENODEV;
	}

	ssd1306_init_lcd(drv_client);

	return 0;
}

static int ssd1306_remove(struct i2c_client *drv_client)
{
	struct ssd1306_data *lcd_data =
			(struct ssd1306_data *)drv_client->dev.driver_data;

	ssd1306_deinit_lcd(lcd_data);

	devm_kfree(&drv_client->dev, lcd_data);
	dev_info(&drv_client->dev, "ssd1306 driver successfully removed\n");
	return 0;
}

static struct i2c_driver ssd1306_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = ssd1306_probe,
	.remove = ssd1306_remove,
	.id_table = ssd1306_ids
};
module_i2c_driver(ssd1306_driver);

MODULE_LICENSE(DRIVER_LIC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
