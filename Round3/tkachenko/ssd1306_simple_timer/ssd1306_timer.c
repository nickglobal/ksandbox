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

#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "myfont.h"

#define DEVICE_NAME "ssd1306_timer"
#define CLASS_NAME  "ssd1306_display"
#define BUS_NAME    "i2c_1"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define TIMER_MAX 99999999

#define SSD1306_COLOR_BLACK 0x00 /* Black clr, no pixel */
#define	SSD1306_COLOR_WHITE 0x01 /* Pixel is set. */

#define INVERT_COLOR(c) ((c == SSD1306_COLOR_BLACK) ? \
			SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK)

#define SSD1306_WRITE_DATA(c, d) i2c_smbus_write_byte_data((c), 0x40, (d))
#define SSD1306_WRITE_COMMAND(c, d) i2c_smbus_write_byte_data((c), 0x00, (d))

#define MEMORY_MODE				0x20
#define SET_CONTRAST			0x81
#define CHARGE_PUMP				0x8D
#define SET_NORMAL_DISPLAY		0xA6
#define SET_INVERT_DISPLAY		0xA7
#define DISPLAY_OFF				0xAE
#define DISPLAY_ON				0xAF
#define COM_SCAN_INC			0xC0
#define COM_SCAN_DEC			0xC8
#define SET_DISPLAY_OFFSET		0xD3
#define SET_DISPLAY_CLOCK_DIV	0xD5
#define SET_PRECHARGE			0xD9
#define SET_COM_PINS			0xDA
#define SET_VCOM_DETECT			0xDB

#define interval_s(x) ((x) * HZ)

/* display status structure */
struct display_state {
	u16 curX;
	u16 curY;
	u8  inverted;
};

struct ssd1306_data {
	struct i2c_client		*client;
	struct display_state	state;
	struct kobject			display_kobj;
	struct timer_list		timer;
	spinlock_t				lock;
	struct work_struct      display_update_ws;
	int						status;
	int						display_width;
	int						display_height;
	int						interval;
	int						counter;
	u8						screen_buffer[];
};

/* -------------- hardware description -------------- */

/* Memory modes */
enum {
	SSD1306_MEM_MODE_HORIZONAL = 0,
	SSD1306_MEM_MODE_VERTICAL,
	SSD1306_MEM_MODE_PAGE,
	SSD1306_MEM_MODE_INVALID
};

/* i2c device_id structure */
static const struct i2c_device_id ssd1306_idtable[] = {
	{ DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssd1306_idtable);


/* internal API */
static int ssd1306_on(struct ssd1306_data *data);
static int ssd1306_off(struct ssd1306_data *data);
static int ssd1306_gotoXY(struct ssd1306_data *data, u16 x, u16 y);
static int ssd1306_updateScreen(struct ssd1306_data *data);
static char ssd1306_putc(
		struct ssd1306_data *data,
		char ch,
		struct TM_FontDef_t *Font,
		int clr);
static char ssd1306_puts(
		struct ssd1306_data *data,
		char *str,
		struct TM_FontDef_t *Font,
		int clr);
static int
ssd1306_drawPixel(struct ssd1306_data *data, u16 x, u16 y, int clr);

static ssize_t get_counter(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t set_counter(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len);
static ssize_t get_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t set_interval(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len);
static void krelease(struct kobject *kobj);




struct kobj_attribute kobj_attr_counter = __ATTR(
		counter,
		0664,
		get_counter,
		set_counter);

struct kobj_attribute kobj_attr_interval = __ATTR(
		interval,
		0664,
		get_interval,
		set_interval);

static struct attribute *ssd1306_attr[] = {
	&kobj_attr_interval.attr,
	&kobj_attr_counter.attr,
	NULL,
};

static struct attribute_group ssd1306_attr_group = {
	.attrs = ssd1306_attr,
};

static void krelease(struct kobject *kobj)
{
	/* nothing to do since memory is freed by ssd1306_remove */
}

static struct kobj_type ssd1306_ktype = {
	.release = krelease,
	.sysfs_ops = &kobj_sysfs_ops,
};

static inline int sizeof_ssd1306_data(int width, int height)
{
	return sizeof(struct ssd1306_data) + (width * height / 8);
}

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static void ssd1306_init_lcd(struct i2c_client *client)
{
	char m, i;
	struct ssd1306_data *data = i2c_get_clientdata(client);
	int width = data->display_width;

	dev_info(&client->dev, "ssd1306: Device init\n");
	/* Init LCD */
	SSD1306_WRITE_COMMAND(client, DISPLAY_OFF);

	SSD1306_WRITE_COMMAND(client, MEMORY_MODE);
	SSD1306_WRITE_COMMAND(client, SSD1306_MEM_MODE_PAGE);
	SSD1306_WRITE_COMMAND(client, 0xB0); /* Page Start Address 0xB0~0xB7 */
	SSD1306_WRITE_COMMAND(client, 0x00); /* set low column address */
	SSD1306_WRITE_COMMAND(client, 0x10); /* set high column address */
	SSD1306_WRITE_COMMAND(client, 0x40); /* set start line address */

	SSD1306_WRITE_COMMAND(client, COM_SCAN_DEC);

	SSD1306_WRITE_COMMAND(client, SET_CONTRAST);
	SSD1306_WRITE_COMMAND(client, 0xFF); /* Max contrast */

	SSD1306_WRITE_COMMAND(client, 0xA1); /* set segment re-map 0 to 127 */
	SSD1306_WRITE_COMMAND(client, SET_NORMAL_DISPLAY);
	SSD1306_WRITE_COMMAND(client, 0xA8); /* set multiplex ratio(1 to 64) */
	SSD1306_WRITE_COMMAND(client, 0x3F);
	SSD1306_WRITE_COMMAND(client, 0xA4); /* 0xA4 => follows RAM content */
	SSD1306_WRITE_COMMAND(client, SET_DISPLAY_OFFSET);
	SSD1306_WRITE_COMMAND(client, 0x00); /* no offset */

	SSD1306_WRITE_COMMAND(client, SET_DISPLAY_CLOCK_DIV);
	SSD1306_WRITE_COMMAND(client, 0xA0); /* set divide ratio */

	SSD1306_WRITE_COMMAND(client, SET_PRECHARGE);
	SSD1306_WRITE_COMMAND(client, 0x22);

	SSD1306_WRITE_COMMAND(client, SET_COM_PINS);
	SSD1306_WRITE_COMMAND(client, 0x12);

	SSD1306_WRITE_COMMAND(client, SET_VCOM_DETECT);
	SSD1306_WRITE_COMMAND(client, 0x20); /* 0x20, 0.77x Vcc */

	ssd1306_on(data);

	for (m = 0; m < 8; m++) {
		SSD1306_WRITE_COMMAND(client, 0xB0 + m);
		SSD1306_WRITE_COMMAND(client, 0x00);
		SSD1306_WRITE_COMMAND(client, 0x10);
		/* Write multi data */
		for (i = 0; i < width; i++)
			SSD1306_WRITE_DATA(client, 0xAA);
	}
}

/* Update Screen from buffer */
int ssd1306_updateScreen(struct ssd1306_data *data)
{
	struct i2c_client *client = data->client;
	int width = data->display_width;
	char m, i;

	client = data->client;

	for (m = 0; m < 8; m++) {
		SSD1306_WRITE_COMMAND(client, 0xB0 + m);
		SSD1306_WRITE_COMMAND(client, 0x00);
		SSD1306_WRITE_COMMAND(client, 0x10);
		/* Write multi data */
		for (i = 0; i < width; i++) {
			SSD1306_WRITE_DATA(
					client,
					data->screen_buffer[(width * m) + i]);
		}
	}
	return 0;
}

int
ssd1306_drawPixel(struct ssd1306_data *data, u16 x, u16 y, int clr)
{
	struct display_state *state = &data->state;
	int width = data->display_width;

	if (x >= width || y >= width)
		return -1;

	/* Check if pixels are inverted */
	if (state->inverted)
		clr = INVERT_COLOR(clr);

	/* Set clr */
	if (clr == SSD1306_COLOR_WHITE)
		data->screen_buffer[x + (y / 8) * width] |= 1 << (y % 8);
	else
		data->screen_buffer[x + (y / 8) * width] &= ~(1 << (y % 8));

	return 0;
}

int ssd1306_gotoXY(struct ssd1306_data *data, u16 x, u16 y)
{
	/* Set write pointers */
	struct display_state *state = &data->state;

	state->curX = x;
	state->curY = y;
	return 0;
}


char ssd1306_putc(
		struct ssd1306_data *data,
		char ch,
		struct TM_FontDef_t *Font,
		int clr)
{
	u32 i, b, j;
	struct display_state *state = &data->state;
	int width = data->display_width;
	int height = data->display_height;
	int pixel_clr;

	/* Check available space in LCD */
	if (width <= (state->curX + Font->FontWidth) ||
		height <= (state->curY + Font->FontHeight)) {
		return 0;
	}

	/* Go through font */
	for (i = 0; i < Font->FontHeight; i++) {
		b = Font->data[(ch - 32) * Font->FontHeight + i];
		for (j = 0; j < Font->FontWidth; j++) {
			pixel_clr = ((b << j) & 0x8000) ?
				clr	: INVERT_COLOR(clr);
			ssd1306_drawPixel(
					data,
					state->curX + j,
					state->curY + i,
					pixel_clr);
		}
	}
	/* Increase pointer */
	state->curX += Font->FontWidth;
    /* Return character written */
	return ch;
}

char ssd1306_puts(
		struct ssd1306_data *data,
		char *str,
		struct TM_FontDef_t *Font,
		int clr)
{
	while (*str) { /* Write character by character */
		if (ssd1306_putc(data, *str, Font, clr) != *str)
			return *str; /* Return error */
		str++; /* Increase string pointer */
	}
	return *str; /* Everything OK, zero should be returned */
}

int ssd1306_on(struct ssd1306_data *data)
{
	struct i2c_client *client = data->client;

	SSD1306_WRITE_COMMAND(client, CHARGE_PUMP);
	SSD1306_WRITE_COMMAND(client, 0x14);
	SSD1306_WRITE_COMMAND(client, DISPLAY_ON);
	return 0;
}

int ssd1306_off(struct ssd1306_data *data)
{
	struct i2c_client *client = data->client;

	SSD1306_WRITE_COMMAND(client, CHARGE_PUMP);
	SSD1306_WRITE_COMMAND(client, 0x10);
	SSD1306_WRITE_COMMAND(client, DISPLAY_OFF);
	return 0;
}

static void update_display_work(struct work_struct *work)
{
	char disp_str[16] = {0};
	struct ssd1306_data *ssd1306 =
		container_of(work, struct ssd1306_data, display_update_ws);

	if (ssd1306->interval)
		snprintf(disp_str, 31, "Timer: %8d", ssd1306->counter);
	else
		snprintf(disp_str, 31, "Timer: inactive");

	ssd1306_gotoXY(ssd1306, 8, 25);
	ssd1306_puts(ssd1306,
			disp_str,
			&TM_Font_7x10,
			SSD1306_COLOR_WHITE);

	ssd1306_updateScreen(ssd1306);
}

static void timer_tick(unsigned long __data)
{
	struct ssd1306_data *ssd1306 = (void *)__data;

	if (ssd1306->counter == TIMER_MAX)
		ssd1306->counter = 0;
	else
		ssd1306->counter++;

	schedule_work(&ssd1306->display_update_ws);

	if (ssd1306->interval)
		mod_timer(&ssd1306->timer,
				jiffies + interval_s(ssd1306->interval));
}

static ssize_t get_counter(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ssd1306_data *ssd1306 =
		container_of(kobj, struct ssd1306_data, display_kobj);

	spin_lock_bh(&ssd1306->lock);
	sprintf(buf, "%d", ssd1306->counter);
	spin_unlock_bh(&ssd1306->lock);
	return strlen(buf);
}

static ssize_t set_counter(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int new_value;
	struct ssd1306_data *ssd1306 =
		container_of(kobj, struct ssd1306_data, display_kobj);
	int ret = len;

	spin_lock_bh(&ssd1306->lock);
	if (len) {
		if (kstrtoint(buf, 10, &new_value)) {
			ret = 0;
			goto cleanup;
		}
	} else
		new_value = 0;

	ssd1306->counter = new_value;

cleanup:
	spin_unlock_bh(&ssd1306->lock);
	return ret;
}

static ssize_t get_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ssd1306_data *ssd1306 =
		container_of(kobj, struct ssd1306_data, display_kobj);

	spin_lock_bh(&ssd1306->lock);
	sprintf(buf, "%d", ssd1306->interval);
	spin_unlock_bh(&ssd1306->lock);
	return strlen(buf);
}

static ssize_t set_interval(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	struct ssd1306_data *ssd1306 =
		container_of(kobj, struct ssd1306_data, display_kobj);
	int ret = len;
	u32 new_interval;

	spin_lock_bh(&ssd1306->lock);
	if (len) {
		if (kstrtoint(buf, 10, &new_interval)) {
			ret = 0;
			goto cleanup;
		}
	} else
		new_interval = 0;

	if (ssd1306->interval == 0 && new_interval > 0) {
		ssd1306->counter = 0;
		mod_timer(&ssd1306->timer, jiffies + interval_s(new_interval));
	}

	ssd1306->interval = new_interval;

cleanup:
	spin_unlock_bh(&ssd1306->lock);
	return ret;
}


static int
ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ssd1306_data *ssd1306;
	struct i2c_adapter  *adapter;
	int ret;

	dev_info(&client->dev, "ssd1306 I2C probe\n");

	ssd1306 = devm_kzalloc(&client->dev,
			sizeof_ssd1306_data(SCREEN_WIDTH, SCREEN_HEIGHT),
			GFP_KERNEL);

	if (!ssd1306)
		return -ENOMEM;

	ssd1306->client = client;
	ssd1306->display_width = SCREEN_WIDTH;
	ssd1306->display_height = SCREEN_HEIGHT;
	ssd1306->status = 0xABCD;
	/* default timer settings */
	ssd1306->interval = 1;
	ssd1306->counter = 0;

	spin_lock_init(&ssd1306->lock);
	setup_timer(&ssd1306->timer, timer_tick, (unsigned long) ssd1306);

	ret = kobject_init_and_add(
			&ssd1306->display_kobj,
			&ssd1306_ktype,
			&client->dev.kobj,
			"%s", CLASS_NAME);
	if (ret)
		goto cleanup;

	ret = sysfs_create_group(&ssd1306->display_kobj, &ssd1306_attr_group);
	if (ret)
		goto cleanup;

	INIT_WORK(&ssd1306->display_update_ws, update_display_work);

	i2c_set_clientdata(client, ssd1306);
	adapter = client->adapter;

	if (!adapter) {
		dev_err(&client->dev, "adapter indentification error\n");
		ret = -ENODEV;
		goto cleanup;
	}

	dev_info(&client->dev, "I2C client address %d\n", client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "operation not supported\n");
		ret = -ENODEV;
		goto cleanup;
	}

	ssd1306_init_lcd(client);

	ssd1306_gotoXY(ssd1306, 8, 25);
	ssd1306_puts(ssd1306,
			"Timer: inactive",
			&TM_Font_7x10,
			SSD1306_COLOR_WHITE);
	ssd1306_updateScreen(ssd1306);

	mod_timer(&ssd1306->timer, jiffies + interval_s(ssd1306->interval));

	dev_info(&client->dev, "ssd1306 driver successfully loaded\n");

	return 0;

cleanup:
	dev_info(&client->dev, "ssd1306 driver loading error\n");
	del_timer_sync(&ssd1306->timer);
	sysfs_remove_group(&ssd1306->display_kobj, &ssd1306_attr_group);
	kobject_put(&ssd1306->display_kobj);
	return ret;
}

static int ssd1306_remove(struct i2c_client *client)
{
	struct ssd1306_data *ssd1306 = i2c_get_clientdata(client);

	del_timer_sync(&ssd1306->timer);
	cancel_work_sync(&ssd1306->display_update_ws);
	sysfs_remove_group(&ssd1306->display_kobj, &ssd1306_attr_group);
	kobject_put(&ssd1306->display_kobj);
	ssd1306_off(ssd1306);
	dev_info(&client->dev, "ssd1306 driver removed\n");
	return 0;
}

static struct i2c_driver ssd1306_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe       = ssd1306_probe,
	.remove      = ssd1306_remove,
	.id_table    = ssd1306_idtable,
};

module_i2c_driver(ssd1306_driver);

MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("GL team");
MODULE_LICENSE("GPL");
