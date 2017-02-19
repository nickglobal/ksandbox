/*
 * ssd1306.h
 *
 *  Created on: Jan 29, 2017
 *      Author: vvs
 */

#ifndef SSD1306_H_
#define SSD1306_H_


#define DEVICE_NAME				"ssd1306"
#define CLASS_NAME				"oled_display"
#define BUS_NAME				"i2c2"

#define SSD1306_WIDTH			128
#define SSD1306_HEIGHT			64

#define WRITECOMMAND			0x00
#define WRITEDATA				0x40


/* register def
#define SSD1306_LCDWIDTH				128
#define SSD1306_LCDHEIGHT				64
#define SSD1306_SETCONTRAST				0x81
#define SSD1306_DISPLAYALLON_RESUME		0xA4


#define SSD1306_DISPLAYALLON			0xA5
#define SSD1306_NORMALDISPLAY			0xA6
#define SSD1306_INVERTDISPLAY			0xA7
#define SSD1306_DISPLAYOFF				0xAE
#define SSD1306_DISPLAYON				0xAF
#define SSD1306_SETDISPLAYOFFSET		0xD3
#define SSD1306_SETCOMPINS				0xDA
#define SSD1306_SETVCOMDETECT			0xDB
#define SSD1306_SETDISPLAYCLOCKDIV		0xD5
#define SSD1306_SETPRECHARGE			0xD9
#define SSD1306_SETMULTIPLEX			0xA8
#define SSD1306_SETLOWCOLUMN			0x00
#define SSD1306_SETHIGHCOLUMN			0x10
#define SSD1306_SETSTARTLINE			0x40
#define SSD1306_MEMORYMODE				0x20
#define SSD1306_COLUMNADDR				0x21
#define SSD1306_PAGEADDR				0x22
#define SSD1306_COMSCANINC				0xC0
#define SSD1306_COMSCANDEC				0xC8
#define SSD1306_SEGREMAP				0xA0
#define SSD1306_CHARGEPUMP				0x8D
#define SSD1306_EXTERNALVCC				0x01
#define SSD1306_SWITCHCAPVCC			0x02
*/

/* -------------- hardware description -------------- */
/* device models */
enum {
        ssd1306_oled,
};
/* device address */
enum {
        ssd1306_addr  = 0x3c,
};

struct dpoz{
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t  Inverted;
    uint8_t  Initialized;
};

struct ssd1306_data {
    struct i2c_client *client;
    int16_t status;
    struct dpoz poz;

    bool autostart;		/* dev_attrs[0] */
    uint32_t start;		/* dev_attrs[1] */
    int32_t timeout;	/* dev_attrs[2] */
    int32_t timeout_init;

    struct device_attribute dev_attrs[4];
	struct attribute *attrs[4];
	struct attribute_group attr_group;
	struct attribute_group *attr_groups[2];

	struct timer_list timer;

	struct timer_list timer_updater;
	struct work_struct work;
};

extern const struct i2c_device_id ssd1306_ids[];

void ssd1306_init_lcd(struct i2c_client *drv_client);
void ssd1306_deinit_lcd(struct ssd1306_data *lcd_data);

void ssd1306_register_attributes(struct ssd1306_data *lcd_data);
void ssd1306_unregister_attributes(struct ssd1306_data *lcd_data);

void ssd1306_init_timer(struct ssd1306_data *lcd_data);
void ssd1306_deinit_timer(struct ssd1306_data *lcd_data);

void ssd1306_init_updater(struct ssd1306_data *lcd_data);
void ssd1306_stop_updater(struct ssd1306_data *lcd_data);

void ssd1306_refresh(struct work_struct *work);

#endif /* SSD1306_H_ */
