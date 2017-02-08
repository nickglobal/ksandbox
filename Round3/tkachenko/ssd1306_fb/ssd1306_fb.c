/*
 * ssd1306 oled display driver
 *
 * 7-bit I2C slave addresses:
 *  0x3c (ADDR)
  *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "ssd1306_fb"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define PAGES_NUM     8

#define SSD1306_BLOCK_SIZE 32

#define SSD1306_SEND_COMMAND(c, d) i2c_smbus_write_byte_data((c), 0x00, (d))
#define SSD1306_SEND_DATA_BLOCK(c, d) i2c_smbus_write_i2c_block_data( \
		(c), 0x40, SSD1306_BLOCK_SIZE, (u8 *)(d))

#define SET_LOW_COLUMN	0x00
#define SET_HIGH_COLUMN	0x10
#define COLUMN_ADDR		0x21
#define PAGE_ADDR		0x22
#define SET_START_PAGE	0xB0
#define CHARGE_PUMP		0x8D
#define DISPLAY_OFF		0xAE
#define DISPLAY_ON		0xAF

#define MEMORY_MODE				0x20
#define SET_CONTRAST			0x81
#define SET_NORMAL_DISPLAY		0xA6
#define SET_INVERT_DISPLAY		0xA7
#define COM_SCAN_INC			0xC0
#define COM_SCAN_DEC			0xC8
#define SET_DISPLAY_OFFSET		0xD3
#define SET_DISPLAY_CLOCK_DIV	0xD5
#define SET_PRECHARGE			0xD9
#define SET_COM_PINS			0xDA
#define SET_VCOM_DETECT			0xDB

struct ssd1306_data {
	struct i2c_client		*client;
	struct fb_info			*fbinfo;
	struct work_struct      display_update_ws;
	int						display_width;
	int						display_height;
	int						pages_number;
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
static int ssd1306_on(struct ssd1306_data *ssd1306);
static int ssd1306_off(struct ssd1306_data *ssd1306);
static int ssd1306_updateScreen(struct ssd1306_data *ssd1306);

/* FB */
static ssize_t ssd1306_write(
		struct fb_info *info,
		const char __user *buf,
		size_t count,
		loff_t *ppos);
static void
ssd1306_fillrect(struct fb_info *info, const struct fb_fillrect *rect);
static void
ssd1306_copyarea(struct fb_info *info, const struct fb_copyarea *area);
static void
ssd1306_imageblit(struct fb_info *info, const struct fb_image *image);
static int
ssd1306_blank(int blank_mode, struct fb_info *info);

static struct fb_fix_screeninfo ssd1306_fix = {
	.id			= "SSD1306 FB",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_MONO10,
	.xpanstep	= 0,
	.ypanstep	= 0,
	.ywrapstep	= 0,
	.accel		= FB_ACCEL_NONE,
};

static struct fb_var_screeninfo ssd1306_var = {
	.bits_per_pixel = 1,
};

static struct fb_ops ssd1306_ops = {
	.owner			= THIS_MODULE,
	.fb_blank		= ssd1306_blank,
	.fb_write		= ssd1306_write,
	.fb_fillrect	= ssd1306_fillrect,
	.fb_copyarea	= ssd1306_copyarea,
	.fb_imageblit	= ssd1306_imageblit,
};

static void ssd1306_init_lcd(struct i2c_client *client)
{
	struct ssd1306_data *ssd1306 = i2c_get_clientdata(client);

	dev_info(&client->dev, "ssd1306: Device init\n");
	/* Init LCD */
	SSD1306_SEND_COMMAND(client, DISPLAY_OFF);

	SSD1306_SEND_COMMAND(client, MEMORY_MODE);
	SSD1306_SEND_COMMAND(client, SSD1306_MEM_MODE_HORIZONAL);
	SSD1306_SEND_COMMAND(client, SET_START_PAGE); /* 0xB0~0xB7 */
	SSD1306_SEND_COMMAND(client, SET_LOW_COLUMN);
	SSD1306_SEND_COMMAND(client, SET_HIGH_COLUMN);
	SSD1306_SEND_COMMAND(client, 0x40); /* set start line address */

	SSD1306_SEND_COMMAND(client, COM_SCAN_DEC);

	SSD1306_SEND_COMMAND(client, SET_CONTRAST);
	SSD1306_SEND_COMMAND(client, 0xFF); /* Max contrast */

	SSD1306_SEND_COMMAND(client, 0xA1); /* set segment re-map 0 to 127 */
	SSD1306_SEND_COMMAND(client, SET_NORMAL_DISPLAY);
	SSD1306_SEND_COMMAND(client, 0xA8); /* set multiplex ratio(1 to 64) */
	SSD1306_SEND_COMMAND(client, 0x3F);
	SSD1306_SEND_COMMAND(client, 0xA4); /* 0xA4 => follows RAM content */
	SSD1306_SEND_COMMAND(client, SET_DISPLAY_OFFSET);
	SSD1306_SEND_COMMAND(client, 0x00); /* no offset */

	SSD1306_SEND_COMMAND(client, SET_DISPLAY_CLOCK_DIV);
	SSD1306_SEND_COMMAND(client, 0xA0); /* set divide ratio */

	SSD1306_SEND_COMMAND(client, SET_PRECHARGE);
	SSD1306_SEND_COMMAND(client, 0x22);

	SSD1306_SEND_COMMAND(client, SET_COM_PINS);
	SSD1306_SEND_COMMAND(client, 0x12);

	SSD1306_SEND_COMMAND(client, SET_VCOM_DETECT);
	SSD1306_SEND_COMMAND(client, 0x20); /* 0x20, 0.77x Vcc */

	ssd1306_on(ssd1306);

	schedule_work(&ssd1306->display_update_ws);
}

static ssize_t ssd1306_write(
		struct fb_info *info,
		const char __user *buf,
		size_t count,
		loff_t *ppos)
{
	unsigned long total_size;
	unsigned long p = *ppos;
	struct ssd1306_data *ssd1306 = info->par;
	u8 __iomem *dst;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EINVAL;

	if (count + p > total_size)
		count = total_size - p;

	if (!count)
		return -EINVAL;

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		return -EFAULT;

	schedule_work(&ssd1306->display_update_ws);
	*ppos += count;
	return count;
}

static void
ssd1306_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct ssd1306_data *ssd1306 = info->par;

	cfb_fillrect(info, rect);
	schedule_work(&ssd1306->display_update_ws);
}

static void
ssd1306_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct ssd1306_data *ssd1306 = info->par;

	cfb_copyarea(info, area);
	schedule_work(&ssd1306->display_update_ws);
}

static void
ssd1306_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct ssd1306_data *ssd1306 = info->par;

	cfb_imageblit(info, image);
	schedule_work(&ssd1306->display_update_ws);
}

static int ssd1306_blank(int blank_mode, struct fb_info *info)
{
	struct ssd1306_data *ssd1306 = info->par;

	if (blank_mode != FB_BLANK_UNBLANK)
		return ssd1306_off(ssd1306);
	else
		return ssd1306_on(ssd1306);
}

int ssd1306_updateScreen(struct ssd1306_data *ssd1306)
{
	struct i2c_client *client = ssd1306->client;
	u8 *vmem = ssd1306->fbinfo->screen_base;
	int vmem_size = ssd1306->fbinfo->fix.smem_len;
	int chunks_num = vmem_size / SSD1306_BLOCK_SIZE;
	int i;

	SSD1306_SEND_COMMAND(client, SET_LOW_COLUMN);
	SSD1306_SEND_COMMAND(client, SET_HIGH_COLUMN);
	SSD1306_SEND_COMMAND(client, SET_START_PAGE);

	/* Set column start / end */
	SSD1306_SEND_COMMAND(client, COLUMN_ADDR);
	SSD1306_SEND_COMMAND(client, 0);
	SSD1306_SEND_COMMAND(client, (ssd1306->display_width - 1));

	/* Set page start / end */
	SSD1306_SEND_COMMAND(client, PAGE_ADDR);
	SSD1306_SEND_COMMAND(client, 0);
	SSD1306_SEND_COMMAND(client, (ssd1306->pages_number - 1));

	for (i = 0; i < chunks_num; i++) {
		SSD1306_SEND_DATA_BLOCK(client, vmem);
		vmem += SSD1306_BLOCK_SIZE;
	}
	return 0;
}

int ssd1306_on(struct ssd1306_data *ssd1306)
{
	struct i2c_client *client = ssd1306->client;

	SSD1306_SEND_COMMAND(client, CHARGE_PUMP);
	SSD1306_SEND_COMMAND(client, 0x14);
	SSD1306_SEND_COMMAND(client, DISPLAY_ON);
	return 0;
}

int ssd1306_off(struct ssd1306_data *ssd1306)
{
	struct i2c_client *client = ssd1306->client;

	if (client == NULL)
		return -ENODEV;

	SSD1306_SEND_COMMAND(client, CHARGE_PUMP);
	SSD1306_SEND_COMMAND(client, 0x10);
	SSD1306_SEND_COMMAND(client, DISPLAY_OFF);
	return 0;
}

static void update_display_work(struct work_struct *work)
{
	struct ssd1306_data *ssd1306 =
		container_of(work, struct ssd1306_data, display_update_ws);
	ssd1306_updateScreen(ssd1306);
}

static void ssd1306_destroy(struct ssd1306_data *ssd1306)
{
	if (ssd1306 == NULL)
		return;

	cancel_work_sync(&ssd1306->display_update_ws);
	ssd1306_off(ssd1306);
	if (ssd1306->fbinfo) {
		unregister_framebuffer(ssd1306->fbinfo);
		framebuffer_release(ssd1306->fbinfo);
	}
}

static int
ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ssd1306_data *ssd1306;
	struct i2c_adapter  *adapter;
	struct fb_info		*fbinfo;
	u8 __iomem			*vmem;
	u32					vmem_size;
	int					ret;

	dev_info(&client->dev, "ssd1306 I2C probe\n");

	fbinfo = framebuffer_alloc(sizeof(struct ssd1306_data), &client->dev);
	if (!fbinfo) {
		dev_err(&client->dev, "Couldn't allocate framebuffer.\n");
		return -ENOMEM;
	}

	ssd1306 = devm_kzalloc(&client->dev,
			sizeof(struct ssd1306_data),
			GFP_KERNEL);

	if (!ssd1306) {
		dev_err(&client->dev, "Couldn't allocate SSD1306 private data\n");
		return -ENOMEM;
	}

	ssd1306->client = client;
	ssd1306->display_width = SCREEN_WIDTH;
	ssd1306->display_height = SCREEN_HEIGHT;
	ssd1306->pages_number = PAGES_NUM;

	ssd1306->fbinfo = fbinfo;
	fbinfo->par = ssd1306;

	vmem_size = ssd1306->display_width * ssd1306->display_height / 8;
	vmem = (u8 __force __iomem *) devm_kzalloc(&client->dev,
			vmem_size, GFP_KERNEL);
	if (!vmem) {
		dev_err(&client->dev, "Couldn't allocate video memory\n");
		return -ENOMEM;
	}

	fbinfo->screen_base = vmem;

	fbinfo->fbops	= &ssd1306_ops;
	fbinfo->fix		= ssd1306_fix;

	fbinfo->fix.line_length = ssd1306->display_width / 8;

	fbinfo->var					= ssd1306_var;
	fbinfo->var.xres			= ssd1306->display_width;
	fbinfo->var.xres_virtual	= ssd1306->display_width;
	fbinfo->var.yres			= ssd1306->display_height;
	fbinfo->var.yres_virtual	= ssd1306->display_height;

	fbinfo->var.red.length		= 1;
	fbinfo->var.red.offset		= 0;
	fbinfo->var.green.length	= 1;
	fbinfo->var.green.offset	= 0;
	fbinfo->var.blue.length		= 1;
	fbinfo->var.blue.offset		= 0;

	fbinfo->fix.smem_start	= (unsigned long)fbinfo->screen_base;
	fbinfo->fix.smem_len	= vmem_size;

	INIT_WORK(&ssd1306->display_update_ws, update_display_work);

	adapter = client->adapter;

	if (!adapter) {
		dev_err(&client->dev, "Adapter indentification error\n");
		ret = -ENODEV;
		goto cleanup;
	}

	dev_info(&client->dev, "I2C client address %d\n", client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "Operation not supported\n");
		ret = -ENODEV;
		goto cleanup;
	}

	i2c_set_clientdata(client, ssd1306);

	ssd1306_init_lcd(client);
	ret = register_framebuffer(fbinfo);
	if (ret) {
		dev_err(&client->dev, "Couldn't register the framebuffer\n");
		goto cleanup;
	}

	dev_info(&client->dev, "ssd1306 driver successfully loaded\n");

	return 0;

cleanup:
	dev_info(&client->dev, "ssd1306 driver loading error\n");
	ssd1306_destroy(ssd1306);
	return ret;
}

static int ssd1306_remove(struct i2c_client *client)
{
	struct ssd1306_data *ssd1306 = i2c_get_clientdata(client);

	ssd1306_destroy(ssd1306);
	dev_info(&client->dev, "ssd1306 driver removed\n");
	return 0;
}

static struct i2c_driver ssd1306_driver = {
	.driver = {
		.name = DEVICE_NAME,
	},

	.probe       = ssd1306_probe,
	.remove      = ssd1306_remove,
	.id_table    = ssd1306_idtable,
};

module_i2c_driver(ssd1306_driver);

MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("GL team");
MODULE_LICENSE("GPL");
