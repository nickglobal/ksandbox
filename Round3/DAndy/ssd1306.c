/*
 * Texas Instruments TMP103 SMBus temperature sensor driver
 * Copyright (C) 2014 Heiko Schocher <hs@denx.de>
 *
 * Based on:
 * Texas Instruments TMP102 SMBus temperature sensor driver
 *
 * Copyright (C) 2010 Steven King <sfking@fdwdc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>

//#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>


#define DEVICE_NAME	"lcd_ssd1306"
#define CLASS_NAME	"oled_display" 
#define BUS_NAME	"i2c_1"

#define SSD1306_WIDTH	128
#define SSD1306_HEIGHT	64

#define WRITECOMMAND	0x00 // 
#define WRITEDATA		0x40 // 


static struct device *dev;



/* Private SSD1306 structure */
struct dpoz{
    u16 CurrentX;
    u16 CurrentY;
    u8  Inverted;
    u8  Initialized;
};


struct ssd1306_data {
    struct i2c_client *client;
	struct class *sys_class;
    int status;
    struct dpoz poz;

	struct workqueue_struct *wq;
	int value;
};


static struct ssd1306_data *lcd;


static void lcd_work_handler(struct work_struct *w);
static DECLARE_DELAYED_WORK(lcd_work, lcd_work_handler);

static unsigned long onesec;

/* -------------- hardware description -------------- */
/* device models */
enum {
        ssd1306oled,
};
/* device address */
enum {
        addrssd1306  = 0x3c,
};



typedef struct{
	u16 	X;
	u16 	Y;
}_Point;


/**
* @brief SSD1306 color enumeration
*/
typedef enum {
        SSD1306_COLOR_BLACK = 0x00,   /*!< Black color, no pixel */
        SSD1306_COLOR_WHITE = 0x01    /*!< Pixel is set. Color depends on LCD */
} ssd1306_COLOR_t;

/* i2c device_id structure */
static const struct i2c_device_id ssd1306_idtable [] = {
    { "ssd1306", ssd1306oled },
    { }
};

//extern u16 shared_data;

/* SSD1306 data buffer */
static u8 ssd1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];


/* -------------- font description -------------- */
typedef struct {
    u8 FontWidth;     /*!< Font width in pixels */
    u8 FontHeight;    /*!< Font height in pixels */
    const u16 *data;  /*!< Pointer to data font data array */
} TM_FontDef_t;

const u16 TM_Font7x10 [] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // sp
0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x1000, 0x0000, 0x0000, // !
0x2800, 0x2800, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // "
0x2400, 0x2400, 0x7C00, 0x2400, 0x4800, 0x7C00, 0x4800, 0x4800, 0x0000, 0x0000, // #
0x3800, 0x5400, 0x5000, 0x3800, 0x1400, 0x5400, 0x5400, 0x3800, 0x1000, 0x0000, // $
0x2000, 0x5400, 0x5800, 0x3000, 0x2800, 0x5400, 0x1400, 0x0800, 0x0000, 0x0000, // %
0x1000, 0x2800, 0x2800, 0x1000, 0x3400, 0x4800, 0x4800, 0x3400, 0x0000, 0x0000, // &
0x1000, 0x1000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // '
0x0800, 0x1000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x1000, 0x0800, // (
0x2000, 0x1000, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x1000, 0x2000, // )
0x1000, 0x3800, 0x1000, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // *
0x0000, 0x0000, 0x1000, 0x1000, 0x7C00, 0x1000, 0x1000, 0x0000, 0x0000, 0x0000, // +
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000, // ,
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3800, 0x0000, 0x0000, 0x0000, 0x0000, // -
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, // .
0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x1000, 0x2000, 0x2000, 0x0000, 0x0000, // /
0x3800, 0x4400, 0x4400, 0x5400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // 0
0x1000, 0x3000, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // 1
0x3800, 0x4400, 0x4400, 0x0400, 0x0800, 0x1000, 0x2000, 0x7C00, 0x0000, 0x0000, // 2
0x3800, 0x4400, 0x0400, 0x1800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // 3
0x0800, 0x1800, 0x2800, 0x2800, 0x4800, 0x7C00, 0x0800, 0x0800, 0x0000, 0x0000, // 4
0x7C00, 0x4000, 0x4000, 0x7800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // 5
0x3800, 0x4400, 0x4000, 0x7800, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // 6
0x7C00, 0x0400, 0x0800, 0x1000, 0x1000, 0x2000, 0x2000, 0x2000, 0x0000, 0x0000, // 7
0x3800, 0x4400, 0x4400, 0x3800, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // 8
0x3800, 0x4400, 0x4400, 0x4400, 0x3C00, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // 9
0x0000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, // :
0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000, // ;
0x0000, 0x0000, 0x0C00, 0x3000, 0x4000, 0x3000, 0x0C00, 0x0000, 0x0000, 0x0000, // <
0x0000, 0x0000, 0x0000, 0x7C00, 0x0000, 0x7C00, 0x0000, 0x0000, 0x0000, 0x0000, // =
0x0000, 0x0000, 0x6000, 0x1800, 0x0400, 0x1800, 0x6000, 0x0000, 0x0000, 0x0000, // >
0x3800, 0x4400, 0x0400, 0x0800, 0x1000, 0x1000, 0x0000, 0x1000, 0x0000, 0x0000, // ?
0x3800, 0x4400, 0x4C00, 0x5400, 0x5C00, 0x4000, 0x4000, 0x3800, 0x0000, 0x0000, // @
0x1000, 0x2800, 0x2800, 0x2800, 0x2800, 0x7C00, 0x4400, 0x4400, 0x0000, 0x0000, // A
0x7800, 0x4400, 0x4400, 0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x0000, 0x0000, // B
0x3800, 0x4400, 0x4000, 0x4000, 0x4000, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, // C
0x7000, 0x4800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4800, 0x7000, 0x0000, 0x0000, // D
0x7C00, 0x4000, 0x4000, 0x7C00, 0x4000, 0x4000, 0x4000, 0x7C00, 0x0000, 0x0000, // E
0x7C00, 0x4000, 0x4000, 0x7800, 0x4000, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, // F
0x3800, 0x4400, 0x4000, 0x4000, 0x5C00, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // G
0x4400, 0x4400, 0x4400, 0x7C00, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // H
0x3800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x3800, 0x0000, 0x0000, // I
0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // J
0x4400, 0x4800, 0x5000, 0x6000, 0x5000, 0x4800, 0x4800, 0x4400, 0x0000, 0x0000, // K
0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x7C00, 0x0000, 0x0000, // L
0x4400, 0x6C00, 0x6C00, 0x5400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // M
0x4400, 0x6400, 0x6400, 0x5400, 0x5400, 0x4C00, 0x4C00, 0x4400, 0x0000, 0x0000, // N
0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // O
0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, // P
0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x5400, 0x3800, 0x0400, 0x0000, // Q
0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x4800, 0x4800, 0x4400, 0x0000, 0x0000, // R
0x3800, 0x4400, 0x4000, 0x3000, 0x0800, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // S
0x7C00, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // T
0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // U
0x4400, 0x4400, 0x4400, 0x2800, 0x2800, 0x2800, 0x1000, 0x1000, 0x0000, 0x0000, // V
0x4400, 0x4400, 0x5400, 0x5400, 0x5400, 0x6C00, 0x2800, 0x2800, 0x0000, 0x0000, // W
0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x2800, 0x2800, 0x4400, 0x0000, 0x0000, // X
0x4400, 0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // Y
0x7C00, 0x0400, 0x0800, 0x1000, 0x1000, 0x2000, 0x4000, 0x7C00, 0x0000, 0x0000, // Z
0x1800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1800, // [
0x2000, 0x2000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0800, 0x0800, 0x0000, 0x0000, /* \ */
0x3000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x3000, // ]
0x1000, 0x2800, 0x2800, 0x4400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // ^
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFE00, // _
0x2000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // `
0x0000, 0x0000, 0x3800, 0x4400, 0x3C00, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, // a
0x4000, 0x4000, 0x5800, 0x6400, 0x4400, 0x4400, 0x6400, 0x5800, 0x0000, 0x0000, // b
0x0000, 0x0000, 0x3800, 0x4400, 0x4000, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, // c
0x0400, 0x0400, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, // d
0x0000, 0x0000, 0x3800, 0x4400, 0x7C00, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, // e
0x0C00, 0x1000, 0x7C00, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // f
0x0000, 0x0000, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0400, 0x7800, // g
0x4000, 0x4000, 0x5800, 0x6400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // h
0x1000, 0x0000, 0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // i
0x1000, 0x0000, 0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0xE000, // j
0x4000, 0x4000, 0x4800, 0x5000, 0x6000, 0x5000, 0x4800, 0x4400, 0x0000, 0x0000, // k
0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // l
0x0000, 0x0000, 0x7800, 0x5400, 0x5400, 0x5400, 0x5400, 0x5400, 0x0000, 0x0000, // m
0x0000, 0x0000, 0x5800, 0x6400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // n
0x0000, 0x0000, 0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // o
0x0000, 0x0000, 0x5800, 0x6400, 0x4400, 0x4400, 0x6400, 0x5800, 0x4000, 0x4000, // p
0x0000, 0x0000, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0400, 0x0400, // q
0x0000, 0x0000, 0x5800, 0x6400, 0x4000, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, // r
0x0000, 0x0000, 0x3800, 0x4400, 0x3000, 0x0800, 0x4400, 0x3800, 0x0000, 0x0000, // s
0x2000, 0x2000, 0x7800, 0x2000, 0x2000, 0x2000, 0x2000, 0x1800, 0x0000, 0x0000, // t
0x0000, 0x0000, 0x4400, 0x4400, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, // u
0x0000, 0x0000, 0x4400, 0x4400, 0x2800, 0x2800, 0x2800, 0x1000, 0x0000, 0x0000, // v
0x0000, 0x0000, 0x5400, 0x5400, 0x5400, 0x6C00, 0x2800, 0x2800, 0x0000, 0x0000, // w
0x0000, 0x0000, 0x4400, 0x2800, 0x1000, 0x1000, 0x2800, 0x4400, 0x0000, 0x0000, // x
0x0000, 0x0000, 0x4400, 0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x1000, 0x6000, // y
0x0000, 0x0000, 0x7C00, 0x0800, 0x1000, 0x2000, 0x4000, 0x7C00, 0x0000, 0x0000, // z
0x1800, 0x1000, 0x1000, 0x1000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000, 0x1800, // {
0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, // |
0x3000, 0x1000, 0x1000, 0x1000, 0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x3000, // }
0x0000, 0x0000, 0x0000, 0x7400, 0x4C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // ~
};

TM_FontDef_t TM_Font_7x10 = {
    7,
    10,
    TM_Font7x10
};


//
int ssd1306_DrawPixel(struct ssd1306_data *drv_data, u16 x, u16 y, ssd1306_COLOR_t color);
int ssd1306_GotoXY(struct ssd1306_data *drv_data, u16 x, u16 y);
char ssd1306_Putc(struct ssd1306_data *drv_data, char ch, TM_FontDef_t* Font, ssd1306_COLOR_t color);
char ssd1306_Puts(struct ssd1306_data *drv_data, char* str, TM_FontDef_t* Font, ssd1306_COLOR_t color);
int ssd1306_UpdateScreen(struct ssd1306_data *drv_data);
int ssd1306_ON(struct ssd1306_data *drv_data);
int ssd1306_OFF(struct ssd1306_data *drv_data);

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static void ssd1306_init_lcd(struct i2c_client *drv_client) {

    char m;
    char i;

    dev_info(dev, "ssd1306: Device init \n");
    	/* Init LCD */
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE); //display off
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //Set Memory Addressing Mode
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0); //Set Page Start Address for Page Addressing Mode,0-7
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xC8); //Set COM Output Scan Direction
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //---set low column address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); //---set high column address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x40); //--set start line address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x81); //--set contrast control register
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x0A);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA1); //--set segment re-map 0 to 127
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA6); //--set normal display
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA8); //--set multiplex ratio(1 to 64)
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x3F); //
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD3); //-set display offset
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //-not offset

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD5); //--set display clock divide ratio/oscillator frequency
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xa0); //--set divide ratio
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD9); //--set pre-charge period
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x22); //

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xDA); //--set com pins hardware configuration
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x12);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xDB); //--set vcomh
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //0x20,0.77xVcc
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D); //--set DC-DC enable
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x14); //
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF); //--turn on SSD1306 panel

    for (m = 0; m < 8; m++) {
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, 0xaa);
        }
    }
}
/**/
int ssd1306_UpdateScreen(struct ssd1306_data *drv_data) {

    struct i2c_client *drv_client;
    char m;
    char i;

    drv_client = drv_data->client;

    for (m = 0; m < 8; m++) {
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, ssd1306_Buffer[SSD1306_WIDTH*m +i]);
        }   
    }

    return 0;
}
/**/
int ssd1306_DrawPixel(struct ssd1306_data *drv_data, u16 x, u16 y, ssd1306_COLOR_t color) {
    struct dpoz *poz;
    poz = &drv_data->poz;
    

    if ( x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT ) {
        return -1;
    }
    /* Check if pixels are inverted */
    if (poz->Inverted) {
        color = (ssd1306_COLOR_t)!color;
    }
    /* Set color */
    if (color == SSD1306_COLOR_WHITE) {
        ssd1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    }
    else {
        ssd1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }

    return 0;
}

/**/
int ssd1306_GotoXY(struct ssd1306_data *drv_data, u16 x, u16 y) {
    /* Set write pointers */
    struct dpoz *poz;
   
    poz = &drv_data->poz;

    poz->CurrentX = x;
    poz->CurrentY = y;

    return 0;
}


/**/
char ssd1306_Putc(struct ssd1306_data *drv_data, char ch, TM_FontDef_t* Font, ssd1306_COLOR_t color) {
    u32 i, b, j;
    struct dpoz *poz;
    poz = &drv_data->poz;

    /* Check available space in LCD */
    if ( SSD1306_WIDTH <= (poz->CurrentX + Font->FontWidth) || SSD1306_HEIGHT <= (poz->CurrentY + Font->FontHeight)) {
        return 0;
    }
    /* Go through font */
    for (i = 0; i < Font->FontHeight; i++) {
        b = Font->data[(ch - 32) * Font->FontHeight + i];
        for (j = 0; j < Font->FontWidth; j++) {
            if ((b << j) & 0x8000) {
                ssd1306_DrawPixel(drv_data, poz->CurrentX + j, (poz->CurrentY + i), (ssd1306_COLOR_t) color);
            } else {
                ssd1306_DrawPixel(drv_data, poz->CurrentX + j, (poz->CurrentY + i), (ssd1306_COLOR_t)!color);
            }
        }
    }
    /* Increase pointer */ 
    poz->CurrentX += Font->FontWidth;
    /* Return character written */
    return ch;
}
/**/
char ssd1306_Puts(struct ssd1306_data *drv_data, char* str, TM_FontDef_t* Font, ssd1306_COLOR_t color) {
    while (*str) { /* Write character by character */
        if (ssd1306_Putc(drv_data, *str, Font, color) != *str) {
            return *str; /* Return error */
        }
        str++; /* Increase string pointer */
    }
    return *str; /* Everything OK, zero should be returned */
}

/**/
int ssd1306_ON(struct ssd1306_data *drv_data) {
    struct i2c_client *drv_client;

    drv_client = drv_data->client;

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x14);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF);
    return 0;
}

/**/
int ssd1306_OFF(struct ssd1306_data *drv_data) {
    struct i2c_client *drv_client;

    drv_client = drv_data->client;

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE);
    return 0;
}


void Graphic_setPoint(const u16 X, const u16 Y)
{
	ssd1306_DrawPixel(lcd, X, Y, SSD1306_COLOR_WHITE);
}



void Graphic_drawLine(_Point p1, _Point p2){
	int dx, dy, inx, iny, e;
	u16 x1 = p1.X, x2 = p2.X;
	u16 y1 = p1.Y, y2 = p2.Y;

    //u16 Color = Graphic_GetForeground();

    dx = x2 - x1;
    dy = y2 - y1;
    inx = dx > 0 ? 1 : -1;
    iny = dy > 0 ? 1 : -1;

//	dx = (u16)abs(dx);
//    dy = (u16)abs(dy);
    dx = (dx > 0) ? dx : -dx;
    dy = (dy > 0) ? dy : -dy;


    if(dx >= dy) {
        dy <<= 1;
        e = dy - dx;
        dx <<= 1;
        while (x1 != x2){
        	Graphic_setPoint(x1, y1);//, Color);
			if(e >= 0){
				y1 += iny;
				e-= dx;
			}
			e += dy;
			x1 += inx;
		}
	}
    else{
		dx <<= 1;
		e = dx - dy;
		dy <<= 1;
		while (y1 != y2){
			Graphic_setPoint(x1, y1);//, Color);
			if(e >= 0){
				x1 += inx;
				e -= dy;
			}
			e += dx;
			y1 += iny;
		}
	}
    Graphic_setPoint(x1, y1);//, Color);
}
// ---------------------------------------------------------------------------


void Graphic_drawLine_(u16 x1, u16 y1, u16 x2, u16 y2){
	_Point p1 = {0}, p2 = {0};
	p1.X = x1;
	p1.Y = y1;
	p2.X = x2;
	p2.Y = y2;

	Graphic_drawLine(p1, p2);
}
// ---------------------------------------------------------------------------



void Graphic_ClearScreen(void){
	memset(ssd1306_Buffer, 0, sizeof(ssd1306_Buffer));
}

void ssd1306_clear(void){

	ssd1306_GotoXY(lcd, 0, 0);
	
	memset(ssd1306_Buffer, 0, sizeof(ssd1306_Buffer));

	ssd1306_UpdateScreen(lcd);
}






static int lcd_work_init(void)
{
	onesec = msecs_to_jiffies(1000);

	if (!lcd->wq)
		lcd->wq = create_singlethread_workqueue(DEVICE_NAME);

	if (lcd->wq)
		queue_delayed_work(lcd->wq, &lcd_work, onesec);

	return 0;
}



static void lcd_work_handler(struct work_struct *w)
{

	char str[20];

	if (lcd->value > 0) {
		sprintf(str, "value = %5d  ", lcd->value);

		ssd1306_GotoXY(lcd, 8, 40);
		ssd1306_Puts(lcd, str, &TM_Font_7x10, SSD1306_COLOR_WHITE);
		ssd1306_UpdateScreen(lcd);

		if (lcd->wq)
			queue_delayed_work(lcd->wq, &lcd_work, onesec);

		//sprintf(str, "value = %5d  \n", lcd->value);
		//dev_info(dev, str);

		lcd->value--;
	}
	else{
		ssd1306_GotoXY(lcd, 8, 40);
		ssd1306_Puts(lcd, "countdown done", &TM_Font_7x10, SSD1306_COLOR_WHITE);
		ssd1306_UpdateScreen(lcd);
	}
}




static ssize_t sys_lcd_clear(struct class *class,
	struct class_attribute *attr, char *buf)
{
	//int ret;
	ssize_t i = 0;

	i += sprintf(buf, "sys_lcd_clear\n");
	
	//ssd1306_clear();
	Graphic_ClearScreen();
    ssd1306_UpdateScreen(lcd);

	return i;
}


static ssize_t sys_lcd_paint(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;
	//_Point center = {64, 32};

	i += sprintf(buf, "sys_lcd_paint\n");


	Graphic_drawLine_(0, 32, 127, 32);
	Graphic_drawLine_(64, 0, 64, 64);

	Graphic_drawLine_(0, 0, 127, 64);
	Graphic_drawLine_(0, 64, 127, 0);

    //Graphic_drawLine_(64, 32, 127, 0);
	ssd1306_UpdateScreen(lcd);

	return i;
}



static ssize_t sys_lcd_interval(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int value;
	ret = sscanf(buf, "%d", &value);
	if(ret!=1)
		return ret;
	
	if(!value)
		return ret;

	if (lcd->wq){
		flush_workqueue(lcd->wq);
		lcd->value = value;
		queue_delayed_work(lcd->wq, &lcd_work, onesec);
	}
	else{
		lcd->wq = 0;
		lcd->value = value;
		lcd_work_init();
	}

	dev_info(dev, "lcd->value = %d\n", lcd->value);
	return count;
}

CLASS_ATTR(clear, 0664, &sys_lcd_clear, NULL);
CLASS_ATTR(paint, 0664, &sys_lcd_paint, NULL);
CLASS_ATTR(interval, 0664, NULL, &sys_lcd_interval);


static void make_sysfs_entry(struct i2c_client *drv_client)
{
	struct device_node *np = drv_client->dev.of_node;
	const char *name;
	int res;

	struct class *sys_class;

	if (np) {

		if (!of_property_read_string(np, "label", &name))
			dev_info(dev, "label = %s\n", name);


		sys_class = class_create(THIS_MODULE, name);

		if (IS_ERR(sys_class)){
			dev_info(dev, "bad class create\n");
		}
		else{
			res = class_create_file(sys_class, &class_attr_clear);
			res = class_create_file(sys_class, &class_attr_paint);
			res = class_create_file(sys_class, &class_attr_interval);


			lcd->sys_class = sys_class;
		}
	}

}




static int ssd1306_probe(struct i2c_client *drv_client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;

	dev = &drv_client->dev;

	dev_info(dev, "init I2C driver\n");


    lcd = devm_kzalloc(&drv_client->dev, sizeof(struct ssd1306_data),
                        GFP_KERNEL);
    if (!lcd)
        return -ENOMEM;

    lcd->client = drv_client;
    lcd->status = 0xABCD;
    lcd->value 	= 10;

    i2c_set_clientdata(drv_client, lcd);

    adapter = drv_client->adapter;

    if (!adapter)
    {
        dev_err(dev, "adapter indentification error\n");
        return -ENODEV;
    }

    dev_info(dev, "I2C client address %d \n", drv_client->addr);

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
            dev_err(dev, "operation not supported\n");
            return -ENODEV;
    }


	make_sysfs_entry(drv_client);
	
	lcd_work_init();

    ssd1306_init_lcd(drv_client);
    ssd1306_GotoXY(lcd, 8, 25);
    ssd1306_Puts(lcd, "HELLO DANDY", &TM_Font_7x10, SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen(lcd);

    dev_info(dev, "ssd1306 driver successfully loaded\n");

	return 0;
}



static int ssd1306_remove(struct i2c_client *client)
{
	struct class *sys_class;

	sys_class = lcd->sys_class;

	class_remove_file(sys_class, &class_attr_clear);
	class_remove_file(sys_class, &class_attr_paint);
	class_remove_file(sys_class, &class_attr_interval);
	class_destroy(sys_class);

	if (lcd->wq)
		destroy_workqueue(lcd->wq);

	dev_info(dev, "Goodbye, world!\n");
	return 0;
}



static const struct of_device_id ssd1306_match[] = {
	{ .compatible = "DAndy,lcd_ssd1306", },
	{ },
};
MODULE_DEVICE_TABLE(of, ssd1306_match);

static const struct i2c_device_id ssd1306_id[] = {
	{ "lcd_ssd1306", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssd1306_id);


static struct i2c_driver ssd1306_driver = {
	.driver = {
		.name	= DEVICE_NAME,
		.of_match_table = ssd1306_match,
		//.pm	= SSD1306_DEV_PM_OPS,
	},
	.probe		= ssd1306_probe,
	.remove 	= ssd1306_remove,
	.id_table	= ssd1306_id,
};
module_i2c_driver(ssd1306_driver);

MODULE_AUTHOR("Devyatov Andrey <devyatov.andey@litech.com>");
MODULE_DESCRIPTION("ssd1306 I2C");
MODULE_LICENSE("GPL");
