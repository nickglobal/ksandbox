

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>

#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include <linux/fb.h>
#include <linux/uaccess.h>

//#include "font.c"
//#include "sysfs.c"


#define DEVICE_NAME	"lcd_ssd1306"
//#define CLASS_NAME	"oled_display" 
//#define BUS_NAME	"i2c_1"

#define LCD_WIDTH	      128
#define LCD_HEIGHT	     64

#define WRITECOMMAND	0x00 // 
#define WRITEDATA		0x40 // 


static struct device *dev;




struct ssd1306_data {
	struct i2c_client *client;
	struct fb_info *info;
	u32 height;
	u32 width;

};


//static struct ssd1306_data *lcd;






static struct fb_fix_screeninfo ssd1307fb_fix = {
	.id     = "Solomon SSD1307",
	.type       = FB_TYPE_PACKED_PIXELS,
	.visual     = FB_VISUAL_MONO10,
	.xpanstep   = 0,
	.ypanstep   = 0,
	.ywrapstep  = 0,
	.accel      = FB_ACCEL_NONE,
};


static struct fb_var_screeninfo ssd1307fb_var = {
	.bits_per_pixel = 1,
};


//extern u16 shared_data;

/* SSD1306 data buffer */
static u8 ssd1306_Buffer[LCD_WIDTH * LCD_HEIGHT / 8];
static u8 *lcd_vmem;


//
int ssd1306_GotoXY(struct ssd1306_data *drv_data, u16 x, u16 y);
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
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x02);//0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
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
		for (i = 0; i < LCD_WIDTH; i++) {
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
		for (i = 0; i < drv_data->width; i++) {
			i2c_smbus_write_byte_data(drv_client, 0x40, ssd1306_Buffer[drv_data->width*m +i]);
		}   
	}

	return 0;
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




static void ssd1307fb_update_display(struct ssd1306_data *lcd)
{
	u8 *vmem = lcd->info->screen_base;
	int i, j, k;


	for (i = 0; i < (lcd->height / 8); i++) {
		for (j = 0; j < lcd->width; j++) {
			u32 array_idx = i * lcd->width + j;
			ssd1306_Buffer[array_idx] = 0;
			for (k = 0; k < 8; k++) {
				u32 page_length = lcd->width * i;
				u32 index = page_length + (lcd->width * k + j) / 8;
				u8 byte = *(vmem + index);
				u8 bit = byte & (1 << (j % 8));
				bit = bit >> (j % 8);
				ssd1306_Buffer[array_idx] |= bit << k;
			}
		}
	}


	ssd1306_UpdateScreen(lcd);
}




static ssize_t ssd1307fb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
    struct ssd1306_data *lcd = info->par;
	unsigned long total_size;
	unsigned long p = *ppos;
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

	ssd1307fb_update_display(lcd);

	*ppos += count;

	return count;
}

static int ssd1307fb_blank(int blank_mode, struct fb_info *info)
{
    struct ssd1306_data *lcd = info->par;

/*
	if (blank_mode != FB_BLANK_UNBLANK)
		return ssd1307fb_write_cmd(lcd->client, SSD1307FB_DISPLAY_OFF);
	else
		return ssd1307fb_write_cmd(lcd->client, SSD1307FB_DISPLAY_ON);
*/
    if (blank_mode != FB_BLANK_UNBLANK)
    	return ssd1306_OFF(lcd);
    else
    	return ssd1306_ON(lcd);
}

static void ssd1307fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    struct ssd1306_data *lcd = info->par;
	sys_fillrect(info, rect);
	ssd1307fb_update_display(lcd);
}

static void ssd1307fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
    struct ssd1306_data *lcd = info->par;
	sys_copyarea(info, area);
	ssd1307fb_update_display(lcd);
}

static void ssd1307fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
    struct ssd1306_data *lcd = info->par;
	sys_imageblit(info, image);
	ssd1307fb_update_display(lcd);
}

static struct fb_ops ssd1307fb_ops = {
	.owner          = THIS_MODULE,
	.fb_read        = fb_sys_read,
	.fb_write       = ssd1307fb_write,
	.fb_blank       = ssd1307fb_blank,
	.fb_fillrect    = ssd1307fb_fillrect,
	.fb_copyarea    = ssd1307fb_copyarea,
	.fb_imageblit   = ssd1307fb_imageblit,
};



static int ssd1306_probe(struct i2c_client *drv_client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;
	struct fb_info *info;
	struct ssd1306_data *lcd;
	u32 vmem_size;
	u8 *vmem;
	int ret;

	dev = &drv_client->dev;

	dev_info(dev, "init I2C driver\n");


	info = framebuffer_alloc(sizeof(struct ssd1306_data), &drv_client->dev);

	if (!info)
		return -ENOMEM;


	lcd = info->par;
	lcd->info = info;
	lcd->client = drv_client;

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

	lcd->info = info;

	lcd->width  = LCD_WIDTH;
	lcd->height = LCD_HEIGHT;

	vmem_size = lcd->width * lcd->height / 8;

	
    lcd_vmem = devm_kzalloc(&drv_client->dev, vmem_size, GFP_KERNEL);

/*	lcd_vmem = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
					get_order(vmem_size));
*/	
    if (!lcd_vmem) {
		dev_err(dev, "Couldn't allocate graphical memory.\n");
		return -ENOMEM;        
	}
	
	vmem = lcd_vmem;//&ssd1306_Buffer[0];


	info->fbops     = &ssd1307fb_ops;
	info->fix       = ssd1307fb_fix;
	info->fix.line_length = lcd->width / 8;
	//info->fbdefio = ssd1307fb_defio;

	info->var = ssd1307fb_var;
	info->var.xres = lcd->width;
	info->var.xres_virtual = lcd->width;
	info->var.yres = lcd->height;
	info->var.yres_virtual = lcd->height;

	info->var.red.length = 1;
	info->var.red.offset = 0;
	info->var.green.length = 1;
	info->var.green.offset = 0;
	info->var.blue.length = 1;
	info->var.blue.offset = 0;

	info->screen_base = (u8 __force __iomem *)vmem;
	info->fix.smem_start = __pa(vmem);
	info->fix.smem_len = vmem_size;


	i2c_set_clientdata(drv_client, info);

	ssd1306_init_lcd(drv_client);
	ssd1306_UpdateScreen(lcd);


	ret = register_framebuffer(info);
	if (ret) {
		dev_err(dev, "Couldn't register the framebuffer\n");
		return ret;
		//goto panel_init_error;
	}    

	dev_info(dev, "ssd1306 driver successfully loaded\n");
	dev_info(dev, "DAndy fb%d: %s device registered, using %d bytes of video memory\n", info->node, info->fix.id, vmem_size);

	return 0;
}



static int ssd1306_remove(struct i2c_client *drv_client)
{
	struct fb_info *info = i2c_get_clientdata(drv_client);

	unregister_framebuffer(info);

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
