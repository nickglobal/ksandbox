/*
 * ssd1306.c
 *
 *  Created on: Jan 29, 2017
 *      Author: vvs
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm-generic/errno-base.h>
#include <linux/workqueue.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "ssd1306.h"

const struct i2c_device_id ssd1306_ids[] = {
	{ "ssd1306", ssd1306_oled },
	{ }, /* Terminating entry */
};

/**
* @brief SSD1306 color enumeration
*/
typedef enum {
	SSD1306_COLOR_BLACK = 0x00,   /*!< Black color, no pixel */
	SSD1306_COLOR_WHITE = 0x01    /*!< Pixel is set. Color depends on LCD */
} ssd1306_COLOR_t;


/* SSD1306 data buffer */
static uint8_t ssd1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
typedef enum {
	message_hello,
	message_counting
} ssd1306_message_t;

/* -------------- font description -------------- */
typedef struct {
	uint8_t FontWidth;     /*!< Font width in pixels */
	uint8_t FontHeight;    /*!< Font height in pixels */
	const uint16_t *data;  /*!< Pointer to data font data array */
} TM_FontDef_t;

const uint16_t TM_Font7x10[] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /*sp */
0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x1000, 0x0000, 0x0000, /* ! */
0x2800, 0x2800, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* " */
0x2400, 0x2400, 0x7C00, 0x2400, 0x4800, 0x7C00, 0x4800, 0x4800, 0x0000, 0x0000, /* # */
0x3800, 0x5400, 0x5000, 0x3800, 0x1400, 0x5400, 0x5400, 0x3800, 0x1000, 0x0000, /* $ */
0x2000, 0x5400, 0x5800, 0x3000, 0x2800, 0x5400, 0x1400, 0x0800, 0x0000, 0x0000, /* % */
0x1000, 0x2800, 0x2800, 0x1000, 0x3400, 0x4800, 0x4800, 0x3400, 0x0000, 0x0000, /* & */
0x1000, 0x1000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* ' */
0x0800, 0x1000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x1000, 0x0800, /* ( */
0x2000, 0x1000, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x1000, 0x2000, /* ) */
0x1000, 0x3800, 0x1000, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* * */
0x0000, 0x0000, 0x1000, 0x1000, 0x7C00, 0x1000, 0x1000, 0x0000, 0x0000, 0x0000, /* + */
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000, /* , */
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3800, 0x0000, 0x0000, 0x0000, 0x0000, /* - */
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, /* . */
0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x1000, 0x2000, 0x2000, 0x0000, 0x0000, /* / */
0x3800, 0x4400, 0x4400, 0x5400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* 0 */
0x1000, 0x3000, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, /* 1 */
0x3800, 0x4400, 0x4400, 0x0400, 0x0800, 0x1000, 0x2000, 0x7C00, 0x0000, 0x0000, /* 2 */
0x3800, 0x4400, 0x0400, 0x1800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, /* 3 */
0x0800, 0x1800, 0x2800, 0x2800, 0x4800, 0x7C00, 0x0800, 0x0800, 0x0000, 0x0000, /* 4 */
0x7C00, 0x4000, 0x4000, 0x7800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, /* 5 */
0x3800, 0x4400, 0x4000, 0x7800, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* 6 */
0x7C00, 0x0400, 0x0800, 0x1000, 0x1000, 0x2000, 0x2000, 0x2000, 0x0000, 0x0000, /* 7 */
0x3800, 0x4400, 0x4400, 0x3800, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* 8 */
0x3800, 0x4400, 0x4400, 0x4400, 0x3C00, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, /* 9 */
0x0000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, /* : */
0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000, /* ; */
0x0000, 0x0000, 0x0C00, 0x3000, 0x4000, 0x3000, 0x0C00, 0x0000, 0x0000, 0x0000, /* < */
0x0000, 0x0000, 0x0000, 0x7C00, 0x0000, 0x7C00, 0x0000, 0x0000, 0x0000, 0x0000, /* = */
0x0000, 0x0000, 0x6000, 0x1800, 0x0400, 0x1800, 0x6000, 0x0000, 0x0000, 0x0000, /* > */
0x3800, 0x4400, 0x0400, 0x0800, 0x1000, 0x1000, 0x0000, 0x1000, 0x0000, 0x0000, /* ? */
0x3800, 0x4400, 0x4C00, 0x5400, 0x5C00, 0x4000, 0x4000, 0x3800, 0x0000, 0x0000, /* @ */
0x1000, 0x2800, 0x2800, 0x2800, 0x2800, 0x7C00, 0x4400, 0x4400, 0x0000, 0x0000, /* A */
0x7800, 0x4400, 0x4400, 0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x0000, 0x0000, /* B */
0x3800, 0x4400, 0x4000, 0x4000, 0x4000, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, /* C */
0x7000, 0x4800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4800, 0x7000, 0x0000, 0x0000, /* D */
0x7C00, 0x4000, 0x4000, 0x7C00, 0x4000, 0x4000, 0x4000, 0x7C00, 0x0000, 0x0000, /* E */
0x7C00, 0x4000, 0x4000, 0x7800, 0x4000, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, /* F */
0x3800, 0x4400, 0x4000, 0x4000, 0x5C00, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* G */
0x4400, 0x4400, 0x4400, 0x7C00, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, /* H */
0x3800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x3800, 0x0000, 0x0000, /* I */
0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, /* J */
0x4400, 0x4800, 0x5000, 0x6000, 0x5000, 0x4800, 0x4800, 0x4400, 0x0000, 0x0000, /* K */
0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x7C00, 0x0000, 0x0000, /* L */
0x4400, 0x6C00, 0x6C00, 0x5400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, /* M */
0x4400, 0x6400, 0x6400, 0x5400, 0x5400, 0x4C00, 0x4C00, 0x4400, 0x0000, 0x0000, /* N */
0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* O */
0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, /* P */
0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x5400, 0x3800, 0x0400, 0x0000, /* Q */
0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x4800, 0x4800, 0x4400, 0x0000, 0x0000, /* R */
0x3800, 0x4400, 0x4000, 0x3000, 0x0800, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, /* S */
0x7C00, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, /* T */
0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* U */
0x4400, 0x4400, 0x4400, 0x2800, 0x2800, 0x2800, 0x1000, 0x1000, 0x0000, 0x0000, /* V */
0x4400, 0x4400, 0x5400, 0x5400, 0x5400, 0x6C00, 0x2800, 0x2800, 0x0000, 0x0000, /* W */
0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x2800, 0x2800, 0x4400, 0x0000, 0x0000, /* X */
0x4400, 0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, /* Y */
0x7C00, 0x0400, 0x0800, 0x1000, 0x1000, 0x2000, 0x4000, 0x7C00, 0x0000, 0x0000, /* Z */
0x1800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1800, /* [ */
0x2000, 0x2000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0800, 0x0800, 0x0000, 0x0000, /* \ */
0x3000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x3000, /* ] */
0x1000, 0x2800, 0x2800, 0x4400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* ^ */
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFE00, /* _ */
0x2000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* ` */
0x0000, 0x0000, 0x3800, 0x4400, 0x3C00, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, /* a */
0x4000, 0x4000, 0x5800, 0x6400, 0x4400, 0x4400, 0x6400, 0x5800, 0x0000, 0x0000, /* b */
0x0000, 0x0000, 0x3800, 0x4400, 0x4000, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, /* c */
0x0400, 0x0400, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, /* d */
0x0000, 0x0000, 0x3800, 0x4400, 0x7C00, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, /* e */
0x0C00, 0x1000, 0x7C00, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, /* f */
0x0000, 0x0000, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0400, 0x7800, /* g */
0x4000, 0x4000, 0x5800, 0x6400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, /* h */
0x1000, 0x0000, 0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, /* i */
0x1000, 0x0000, 0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0xE000, /* j */
0x4000, 0x4000, 0x4800, 0x5000, 0x6000, 0x5000, 0x4800, 0x4400, 0x0000, 0x0000, /* k */
0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, /* l */
0x0000, 0x0000, 0x7800, 0x5400, 0x5400, 0x5400, 0x5400, 0x5400, 0x0000, 0x0000, /* m */
0x0000, 0x0000, 0x5800, 0x6400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, /* n */
0x0000, 0x0000, 0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, /* o */
0x0000, 0x0000, 0x5800, 0x6400, 0x4400, 0x4400, 0x6400, 0x5800, 0x4000, 0x4000, /* p */
0x0000, 0x0000, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0400, 0x0400, /* q */
0x0000, 0x0000, 0x5800, 0x6400, 0x4000, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, /* r */
0x0000, 0x0000, 0x3800, 0x4400, 0x3000, 0x0800, 0x4400, 0x3800, 0x0000, 0x0000, /* s */
0x2000, 0x2000, 0x7800, 0x2000, 0x2000, 0x2000, 0x2000, 0x1800, 0x0000, 0x0000, /* t */
0x0000, 0x0000, 0x4400, 0x4400, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, /* u */
0x0000, 0x0000, 0x4400, 0x4400, 0x2800, 0x2800, 0x2800, 0x1000, 0x0000, 0x0000, /* v */
0x0000, 0x0000, 0x5400, 0x5400, 0x5400, 0x6C00, 0x2800, 0x2800, 0x0000, 0x0000, /* w */
0x0000, 0x0000, 0x4400, 0x2800, 0x1000, 0x1000, 0x2800, 0x4400, 0x0000, 0x0000, /* x */
0x0000, 0x0000, 0x4400, 0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x1000, 0x6000, /* y */
0x0000, 0x0000, 0x7C00, 0x0800, 0x1000, 0x2000, 0x4000, 0x7C00, 0x0000, 0x0000, /* z */
0x1800, 0x1000, 0x1000, 0x1000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000, 0x1800, /* { */
0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, /* | */
0x3000, 0x1000, 0x1000, 0x1000, 0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x3000, /* } */
0x0000, 0x0000, 0x0000, 0x7400, 0x4C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, /* ~ */
};

TM_FontDef_t TM_Font_7x10 = {
	7,
	10,
	TM_Font7x10
};

/* Private functions */
int32_t ssd1306_set_pixel(struct ssd1306_data *drv_data, uint16_t x,
						uint16_t y, ssd1306_COLOR_t color);
int ssd1306_set_xy(struct ssd1306_data *drv_data, uint16_t x, uint16_t y);
char ssd1306_putc(struct ssd1306_data *drv_data, char ch,
				TM_FontDef_t *Font, ssd1306_COLOR_t color);
char ssd1306_puts(struct ssd1306_data *drv_data, char *str,
				TM_FontDef_t *Font, ssd1306_COLOR_t color);
int32_t ssd1306_on(struct ssd1306_data *drv_data);
int32_t ssd1306_off(struct ssd1306_data *drv_data);
void ssd1306_clear_screen(void);
void ssd1306_put_message(struct ssd1306_data *lcd_data,
						ssd1306_message_t message_type);

void timer_callback(unsigned long p_data);
void ssd1306_start_timer(struct ssd1306_data *lcd_data, int32_t timeout_sec);
void ssd1306_stop_timer(struct ssd1306_data *lcd_data);

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
void ssd1306_init_lcd(struct i2c_client *drv_client)
{
	char m, i;
	struct device *dev = &drv_client->dev;
	struct ssd1306_data *lcd_data =
			(struct ssd1306_data *) dev_get_drvdata(&drv_client->dev);
	struct ssd1306_data *wrong_lcd_data = container_of(&drv_client,
			struct ssd1306_data, client);

	lcd_data->autostart = false;
	if (device_property_read_u32(dev, "start", &lcd_data->start))
		lcd_data->start = -1;
	if (device_property_read_u32(dev, "timeout", &lcd_data->timeout))
		lcd_data->timeout = -1;

	dev_info(dev, "ssd1306: ssd1306_init_lcd()\n");
	/* Init sequence */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE); /*display off */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); /*Set Memory Addressing Mode */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); /*Addr Modes: 00,Horiz;01,Vert;10,Page (d);11,Invalid */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0); /*Set Page Start Address for Page Addressing Mode,0-7 */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xC8); /*Set COM Output Scan Direction */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); /*---set low column address */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); /*---set high column address */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x40); /*--set start line address */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x81); /*--set contrast control register */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA1); /*--set segment re-map 0 to 127 */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA6); /*--set normal display */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA8); /*--set multiplex ratio(1 to 64) */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x3F);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA4); /*0xa4,follows RAM content;0xa5,ignores RAM content */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xD3); /*-set display offset */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); /*-not offset */

	i2c_smbus_write_byte_data(drv_client, 0x00, 0xD5); /*--set display clock divide ratio/oscillator frequency */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xa0); /*--set divide ratio */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xD9); /*--set pre-charge period */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x22);

	i2c_smbus_write_byte_data(drv_client, 0x00, 0xDA); /*--set com pins hardware configuration */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x12);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xDB); /*--set vcomhv */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); /*0x20,0.77xVcc */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D); /*--set DC-DC enable */
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x14);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF); /*--turn on SSD1306 panel */

	for (m = 0; m < 8; m++) {
		i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
		i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
		i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
		/* Write multi data */
		for (i = 0; i < SSD1306_WIDTH; i++) {
			i2c_smbus_write_byte_data(drv_client, 0x40, 0xaa);
		}
	}

	ssd1306_put_message(lcd_data, message_hello);

	ssd1306_init_updater(lcd_data);

	dev_info(dev, "ssd1306 at %p, wrong_lcd_data at %p\n",
			lcd_data, wrong_lcd_data);
	dev_info(dev, "ssd1306 driver successfully loaded\n");
}

void ssd1306_deinit_lcd(struct ssd1306_data *lcd_data)
{
	ssd1306_off(lcd_data);

	ssd1306_unregister_attributes(lcd_data);
	ssd1306_deinit_timer(lcd_data);
	ssd1306_stop_updater(lcd_data);
}

void ssd1306_refresh(struct work_struct *work)
{
	struct ssd1306_data *lcd_data = container_of(work,
								struct ssd1306_data, work);

	struct i2c_client *drv_client;
	char m;
	char i;

	drv_client = lcd_data->client;

	for (m = 0; m < 8; m++) {
		i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
		i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
		i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
		/*Write multi data*/
		for (i = 0; i < SSD1306_WIDTH; i++)
			i2c_smbus_write_byte_data(drv_client, 0x40, ssd1306_Buffer[SSD1306_WIDTH*m + i]);
	}
}

void ssd1306_clear_screen(void)
{
	memset(ssd1306_Buffer, 0, sizeof(ssd1306_Buffer));
}

void ssd1306_put_message(struct ssd1306_data *lcd_data,
						ssd1306_message_t message_type)
{
	char string[20];

	switch (message_type) {
	case message_hello:
		ssd1306_clear_screen();
		ssd1306_set_xy(lcd_data, 8, 25);
		ssd1306_puts(lcd_data, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
		break;
	case message_counting:
		ssd1306_clear_screen();
		ssd1306_set_xy(lcd_data, 8, 13);
		ssd1306_puts(lcd_data, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
		ssd1306_set_xy(lcd_data, 15, 25);
		snprintf(string, sizeof(string), "Counting %3d", lcd_data->timeout);
		ssd1306_puts(lcd_data, string, &TM_Font_7x10, SSD1306_COLOR_WHITE);
		ssd1306_set_xy(lcd_data, 8, 47);
		snprintf(string, sizeof(string), "Timeout: %3d", lcd_data->timeout_init);
		ssd1306_puts(lcd_data, string, &TM_Font_7x10, SSD1306_COLOR_WHITE);
		break;
	default:
		break;
	}
}

/**/
int ssd1306_set_pixel(struct ssd1306_data *drv_data, uint16_t x, uint16_t y, ssd1306_COLOR_t color)
{
	struct dpoz *poz;

	poz = &drv_data->poz;

	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
		return -1;
	/* Check if pixels are inverted */
	if (poz->Inverted)
		color = (ssd1306_COLOR_t)!color;
	/* Set color */
	if (color == SSD1306_COLOR_WHITE)
		ssd1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
	else
		ssd1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));

	return 0;
}

int ssd1306_set_xy(struct ssd1306_data *drv_data, uint16_t x, uint16_t y)
{
	/* Set write pointers */
	struct dpoz *poz;

	poz = &drv_data->poz;

	poz->CurrentX = x;
	poz->CurrentY = y;

	return 0;
}

char ssd1306_putc(struct ssd1306_data *drv_data, char ch, TM_FontDef_t *Font, ssd1306_COLOR_t color)
{
	uint32_t i, b, j;
	struct dpoz *poz;

	poz = &drv_data->poz;
	/* Check available space in LCD */
	if (SSD1306_WIDTH <= (poz->CurrentX + Font->FontWidth) ||
		SSD1306_HEIGHT <= (poz->CurrentY + Font->FontHeight))
		return 0;
	/* Go through font */
	for (i = 0; i < Font->FontHeight; i++) {
		b = Font->data[(ch - 32) * Font->FontHeight + i];
		for (j = 0; j < Font->FontWidth; j++) {
			if ((b << j) & 0x8000)
				ssd1306_set_pixel(drv_data, poz->CurrentX + j, (poz->CurrentY + i),
									(ssd1306_COLOR_t) color);
			else
				ssd1306_set_pixel(drv_data, poz->CurrentX + j, (poz->CurrentY + i),
									(ssd1306_COLOR_t)!color);
		}
	}
	/* Increase pointer */
	poz->CurrentX += Font->FontWidth;
	/* Return character written */
	return ch;
}
/**/
char ssd1306_puts(struct ssd1306_data *drv_data, char *str, TM_FontDef_t *Font, ssd1306_COLOR_t color)
{
	while (*str) { /* Write character by character */
		if (ssd1306_putc(drv_data, *str, Font, color) != *str)
			return *str; /* Return error */
		str++; /* Increase string pointer */
	}
	return *str; /* Everything OK, zero should be returned */
}

int ssd1306_on(struct ssd1306_data *drv_data)
{
	struct i2c_client *drv_client;

	drv_client = drv_data->client;

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x14);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF);
	return 0;
}

int ssd1306_off(struct ssd1306_data *drv_data)
{
	struct i2c_client *drv_client;

	drv_client = drv_data->client;

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE);
	return 0;
}

static ssize_t	autostart_show(struct device *dev,
						struct device_attribute *attr, char *str)
{
	struct ssd1306_data *lcd_data = container_of(attr,
								struct ssd1306_data, dev_attrs[0]);

	dev_info(&lcd_data->client->dev, "autostart_show");

	return snprintf(str, 16, "%s\n",
			((lcd_data->autostart == true) ? "true" : "false"));
}

static ssize_t	autostart_store(struct device *dev,
						struct device_attribute *attr,
						const char *str, size_t count)
{
	struct ssd1306_data *lcd_data = container_of(attr,
								struct ssd1306_data, dev_attrs[0]);

	dev_info(&lcd_data->client->dev, "autostart_store: %s", str);

	if (strncmp(str, "true", 4) == 0) {
		lcd_data->autostart = true;
		if (lcd_data->timeout > 0 && lcd_data->start == 0)
			ssd1306_start_timer(lcd_data, -1);
	} else
		if (strncmp(str, "false", 5) == 0)
			lcd_data->autostart = false;

	return count;
}

static ssize_t	start_show(struct device *dev,
						struct device_attribute *attr, char *str)
{
	struct ssd1306_data *lcd_data = container_of(attr,
								struct ssd1306_data, dev_attrs[1]);

	dev_info(&lcd_data->client->dev, "start_show");

	return snprintf(str, 16, "%u\n", lcd_data->start);
}

static ssize_t	start_store(struct device *dev,
						struct device_attribute *attr,
						const char *str, size_t count)
{
	struct ssd1306_data *lcd_data = container_of(attr,
								struct ssd1306_data, dev_attrs[1]);

	long new_value;

	dev_info(&lcd_data->client->dev, "start_store");

	if (kstrtol(str, 0, &new_value) == 0) {
		dev_info(&lcd_data->client->dev, "input value = %ld\n", new_value);
		if (new_value > UINT_MAX) {
			dev_err(&lcd_data->client->dev,
								"start input value overflow\n");
			return count;
		}
		lcd_data->start = (uint32_t) new_value;
		if (lcd_data->start > 0)
			ssd1306_start_timer(lcd_data, -1);
		else
			ssd1306_stop_timer(lcd_data);
	} else
		dev_err(&lcd_data->client->dev,
					"fail to parse start input value\n");
	return count;
}

static ssize_t	timeout_show(struct device *dev,
						struct device_attribute *attr, char *str)
{
	struct ssd1306_data *lcd_data = container_of(attr,
								struct ssd1306_data, dev_attrs[2]);

	dev_info(&lcd_data->client->dev, "timeout_show");

	return snprintf(str, 16, "%d\n", lcd_data->timeout);
}

static ssize_t	timeout_store(struct device *dev,
						struct device_attribute *attr,
						const char *str, size_t count)
{
	struct ssd1306_data *lcd_data = container_of(attr,
								struct ssd1306_data, dev_attrs[2]);
	long new_value;

	dev_info(&lcd_data->client->dev, "timeout_store");

	if (kstrtol(str, 0, &new_value) == 0) {
		dev_info(&lcd_data->client->dev, "input value = %ld\n", new_value);
		if (new_value > LONG_MAX || new_value < LONG_MIN) {
			dev_err(&lcd_data->client->dev,
								"timeout input value overflow\n");
			return count;
		}
		lcd_data->timeout = (int32_t) new_value;
		lcd_data->timeout_init = (int32_t) new_value;
		/* strart/restart timer here if autostart */
		if (lcd_data->autostart)
			ssd1306_start_timer(lcd_data, lcd_data->timeout);
	} else
		dev_err(&lcd_data->client->dev,
					"fail to parse timeout input value\n");
	return count;
}

void ssd1306_register_attributes(struct ssd1306_data *lcd_data)
{
	int32_t result;

	lcd_data->dev_attrs[0].attr.name = "autostart";
	lcd_data->dev_attrs[0].attr.mode = 0664;
	lcd_data->dev_attrs[0].show = autostart_show;
	lcd_data->dev_attrs[0].store = autostart_store;

	lcd_data->dev_attrs[1].attr.name = "start";
	lcd_data->dev_attrs[1].attr.mode = 0664;
	lcd_data->dev_attrs[1].show = start_show;
	lcd_data->dev_attrs[1].store = start_store;

	lcd_data->dev_attrs[2].attr.name = "timeout";
	lcd_data->dev_attrs[2].attr.mode = 0664;
	lcd_data->dev_attrs[2].show = timeout_show;
	lcd_data->dev_attrs[2].store = timeout_store;

	lcd_data->attrs[0] = &lcd_data->dev_attrs[0].attr;
	lcd_data->attrs[1] = &lcd_data->dev_attrs[1].attr;
	lcd_data->attrs[2] = &lcd_data->dev_attrs[2].attr;
	lcd_data->attrs[3] = NULL;

	lcd_data->attr_group.name = "timer";
	lcd_data->attr_group.attrs =
			(struct attribute **) &lcd_data->attrs[0];

	lcd_data->attr_groups[0] = &lcd_data->attr_group;
	lcd_data->attr_groups[1] = NULL;

	result = sysfs_create_groups(&lcd_data->client->dev.kobj,
			(const struct attribute_group **) lcd_data->attr_groups);

	if (result != 0)
		dev_err(&lcd_data->client->dev, "ssd1306 attribute register failed");
}

void ssd1306_unregister_attributes(struct ssd1306_data *lcd_data)
{
	sysfs_remove_groups(&lcd_data->client->dev.kobj,
			(const struct attribute_group **) &lcd_data->attr_groups);
}

void ssd1306_init_timer(struct ssd1306_data *lcd_data)
{
	init_timer(&lcd_data->timer);
	lcd_data->timer.function = timer_callback;
	lcd_data->timer.data = (unsigned long)lcd_data;
}

void ssd1306_deinit_timer(struct ssd1306_data *lcd_data)
{
	ssd1306_stop_timer(lcd_data);
}

void ssd1306_start_timer(struct ssd1306_data *lcd_data, int32_t timeout_sec)
{
	if (timeout_sec > 0)
		lcd_data->timeout = timeout_sec;
	lcd_data->start = 1;
	if (timer_pending(&lcd_data->timer))
		mod_timer(&lcd_data->timer, msecs_to_jiffies(100));
	else {
		lcd_data->timer.expires = jiffies + msecs_to_jiffies(100);
		add_timer(&lcd_data->timer);
	}
}

void ssd1306_stop_timer(struct ssd1306_data *lcd_data)
{
	lcd_data->timeout = 0;
	lcd_data->start = 0;
	del_timer(&lcd_data->timer);
}

void timer_callback(unsigned long p_data)
{
	struct ssd1306_data *lcd_data = (struct ssd1306_data *) p_data;

	lcd_data->timeout--;
	if (lcd_data->timeout > 0) {
		dev_info(&lcd_data->client->dev, "counting %d...\n", lcd_data->timeout);
		lcd_data->timer.expires = jiffies + msecs_to_jiffies(100);
		add_timer(&lcd_data->timer);
		ssd1306_put_message(lcd_data, message_counting);
	} else{
		dev_info(&lcd_data->client->dev, "counting finished\n");
		lcd_data->timeout = 0;
		lcd_data->timeout_init = 0;
		lcd_data->start = 0;
		ssd1306_put_message(lcd_data, message_hello);
	}
}

void updater_callback(unsigned long data);
void ssd1306_init_updater(struct ssd1306_data *lcd_data)
{
	INIT_WORK(&lcd_data->work, ssd1306_refresh);

	init_timer(&lcd_data->timer_updater);
	lcd_data->timer_updater.function = updater_callback;
	lcd_data->timer_updater.data = (unsigned long)lcd_data;

	if (timer_pending(&lcd_data->timer_updater))
		mod_timer(&lcd_data->timer_updater, msecs_to_jiffies(20));
	else {
		lcd_data->timer.expires = jiffies + msecs_to_jiffies(20);
		add_timer(&lcd_data->timer_updater);
	}
}

void ssd1306_stop_updater(struct ssd1306_data *lcd_data)
{
	del_timer(&lcd_data->timer_updater);
	cancel_work_sync(&lcd_data->work);
}

void updater_callback(unsigned long data)
{
	struct ssd1306_data *lcd_data = (struct ssd1306_data *) data;
	/*dev_info(&lcd_data->client->dev, "refreshing lcd...\n");*/
	schedule_work(&lcd_data->work);
	if (timer_pending(&lcd_data->timer_updater))
		mod_timer(&lcd_data->timer_updater, msecs_to_jiffies(20));
	else {
		lcd_data->timer_updater.expires = jiffies + msecs_to_jiffies(20);
		add_timer(&lcd_data->timer_updater);
	}
}
