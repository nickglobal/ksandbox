/*
 * ssd1306 oled display driver
 *
 * 7-bit I2C slave addresses:
 *  0x3c (ADDR)
  *
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function
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


#define DEVICE_NAME "ssd1306"
#define CLASS_NAME  "oled_display" 
#define BUS_NAME    "i2c_1"

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

static struct pkdisp disp = {0};

static struct file_operations pkdisp_dev_ops = {
	.owner   = THIS_MODULE,
	.llseek  = pkdisp_llseek,
	.read    = pkdisp_read,
	.write   = pkdisp_write,
};


/* Private SSD1306 structure */
struct dpoz{
    u16 CurrentX;
    u16 CurrentY;
    u8  Inverted;
    u8  Initialized;
};


struct ssd1306_data {
    struct i2c_client *client;
    int status;
    struct dpoz poz;
};

/* -------------- hardware description -------------- */
/* device models */
enum {
        ssd1306oled,
};
/* device address */
enum {
        addrssd1306  = 0x3c,
};




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
static void ssd1306_init_lcd(struct i2c_client *drv_client)
{

#if 1
    char m;
    char i;

    printk(KERN_ERR "ssd1306: Device init \n");
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
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xFF);
    //i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);
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
#else
    i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_CONTRAST); /* contrast */
    i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 255);
    i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SEG_REMAP_ON); /* set segment re-map */
    i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0xC8); /* Set COM Output Scan Direction */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SET_MULTIPLEX_RATIO); /* set multiplex ratio(1 to 64) */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0x3F);
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SET_CLOCK_FREQ); /* set display clock divide ratio/oscillator frequency */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0xa0);
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SET_PRECHARGE_PERIOD); /* precharge period */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0xD9);
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SET_COM_PINS_CONFIG); /* Set COM pins configuration */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0x12);
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SET_VCOMH); /* set vcomh 0x20,0.77xVcc */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0x20);
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_CHARGE_PUMP); /* -set DC-DC enable */
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, 0x14);
	i2c_smbus_write_byte_data(drv_client, SSD1307FB_COMMAND, SSD1307FB_SET_ADDRESS_MODE); /* address mode: horisontal */
	
#endif
}

/**/
int ssd1306_UpdateScreen(struct ssd1306_data *drv_data) {

    struct i2c_client *drv_client;

    drv_client = drv_data->client;

#if 1
	{
		char *buf = kzalloc(SSD1306_WIDTH*SSD1306_HEIGHT/8 + 1, GFP_KERNEL);
		buf[0] = 0x40;
		memcpy(buf+1, ssd1306_Buffer, SSD1306_WIDTH*SSD1306_HEIGHT/8);
		i2c_master_send(drv_client, buf, (SSD1306_WIDTH*SSD1306_HEIGHT/8 + 1));
		kfree (buf);
	}
#else

	{
    char m;
    char i;
    for (m = 0; m < 8; m++) {
		/* we are in page display mode. Set the page taht we are printing now */
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
		/* these 2 bytes specifies the column start address */
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, ssd1306_Buffer[SSD1306_WIDTH*m +i]);
        } 
    }
	}
#endif

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

static int
ssd1306_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter;

    printk(KERN_ERR "init I2C driver\n");


    disp.ssd1306 = devm_kzalloc(&drv_client->dev, sizeof(struct ssd1306_data),
                        GFP_KERNEL);
    if (!disp.ssd1306)
        return -ENOMEM;

    disp.ssd1306->client = drv_client;
    disp.ssd1306->status = 0xABCD;

    i2c_set_clientdata(drv_client, disp.ssd1306);

    adapter = drv_client->adapter;

    if (!adapter)
    {
        dev_err(&drv_client->dev, "adapter indentification error\n");
        return -ENODEV;
    }

    printk(KERN_ERR "I2C client address %d \n", drv_client->addr);

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
            dev_err(&drv_client->dev, "operation not supported\n");
            return -ENODEV;
    }

    //cra[]
    ssd1306_init_lcd( drv_client);
    ssd1306_GotoXY(disp.ssd1306, 8, 25);
    ssd1306_Puts(disp.ssd1306, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen(disp.ssd1306);

    dev_err(&drv_client->dev, "disp.ssd1306 driver successfully loaded\n");

    return 0;
}

static int
ssd1306_remove(struct i2c_client *client)
{
        return 0;
}


static struct i2c_driver ssd1306_driver = {
    .driver = {
        .name = DEVICE_NAME,
     },

    .probe       = ssd1306_probe ,
    .remove      = ssd1306_remove,
    .id_table    = ssd1306_idtable,
};

/* push picture to OLED */
void pkdisp_flush (void)
{
	int x,y;

    ssd1306_GotoXY(disp.ssd1306, 0, 0);

	for (y=0; y<DISP_H; y++)
		for (x=0; x<DISP_W; x++)
			ssd1306_DrawPixel(disp.ssd1306, x, y,
					disp.buf[y*DISP_W+x]>0?SSD1306_COLOR_WHITE:SSD1306_COLOR_BLACK);

    ssd1306_UpdateScreen(disp.ssd1306);
}

/* File operations */
loff_t  pkdisp_llseek (struct file *filep, loff_t off, int whence)
{
	loff_t newpos;

	switch(whence) {
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
	if (newpos < 0) newpos = 0;
	if (newpos >= DISP_BUFSIZE) newpos = DISP_BUFSIZE-1;
	filep->f_pos = newpos;
	return newpos;
}

ssize_t pkdisp_read   (struct file *filep, char __user *data, size_t size, loff_t *offset)
{
	/* do not go out of buffer */
	if ((*offset+size) > DISP_BUFSIZE) size = DISP_BUFSIZE - *offset;

	if (copy_to_user(data, disp.buf + *offset, size))
		return -EFAULT;

	*offset += size;
	return size;
}

ssize_t pkdisp_write  (struct file *filep, const char __user *data, size_t size, loff_t *offset)
{
	/* do not go out of buffer */
	if ((*offset+size) > DISP_BUFSIZE) size = DISP_BUFSIZE - *offset;

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
	if(disp.buf)
		kfree(disp.buf);
	unregister_chrdev_region(disp.dev, 1);
}

static int __init pkdisp_init( void )
{
	int res;

	res = alloc_chrdev_region(&disp.dev, 0, 1, DISP_NAME);
	if (res < 0) {
		printk(KERN_ERR "error alloc chrdev\n");
		return res;
	}

	disp.buf = kzalloc(sizeof(char) * DISP_W * DISP_H, GFP_KERNEL);
	if (!disp.buf) {
		printk(KERN_ERR "error alloc memory for bufffer\n");
		goto fail;
	}

	cdev_init(&disp.cdev, &pkdisp_dev_ops);
	disp.cdev.owner = THIS_MODULE;
	res = cdev_add(&disp.cdev, disp.dev, 1);
	if (res) {
		printk(KERN_ERR "error add chdev\n");
		goto fail;
	}

	disp.class.name = DISP_NAME;
	disp.class.owner = THIS_MODULE;
	res = class_register(&disp.class);
	if (res) {
		printk(KERN_ERR "fail register class\n");
		goto fail;
	}

	disp.device = device_create(&disp.class, NULL,
			disp.dev, NULL, DISP_NAME);
	if (IS_ERR(disp.device)) {
		res = PTR_ERR(disp.device);
		goto fail;
	}

	printk(KERN_INFO "The \"%s\" device registered (%d:%d)\n",
			DISP_NAME, MAJOR(disp.dev), MINOR(disp.dev));
	return 0;

fail:
	pkdisp_unregister();
	return res;
}

static void __exit pkdisp_exit( void )
{
	pkdisp_unregister();
	printk(KERN_INFO "The \"%s\" device removed\n",	DISP_NAME);
}

/* Module init */
static int __init ssd1306_init ( void )
{
    int ret;
    /*
    * An i2c_driver is used with one or more i2c_client (device) nodes to access
    * i2c slave chips, on a bus instance associated with some i2c_adapter.
    */
    printk(KERN_ERR "ssd1306 mod init\n");
    ret = i2c_add_driver ( &ssd1306_driver);
    if(ret) 
    {
        printk(KERN_ERR "failed to add new i2c driver");
        return ret;
    }

    return pkdisp_init();
}

/* Module exit */
static void __exit ssd1306_exit(void)
{
    i2c_del_driver(&ssd1306_driver);
    printk(KERN_ERR "ssd1306: cleanup\n");  
	pkdisp_exit();
}

/* -------------- kernel thread description -------------- */
module_init(ssd1306_init);
module_exit(ssd1306_exit);


MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("GL team");
MODULE_LICENSE("GPL");
