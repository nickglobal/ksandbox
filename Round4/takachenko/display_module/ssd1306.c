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
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/mm.h>

#include "gyro2disp/include/hwconfig.h"

#ifndef VM_RESERVED
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif /* VM_RESERVED */

#define DEVICE_NAME "ssd1306mmap"

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

struct ssd1306_data {
	struct i2c_client	*client;
	struct dentry		*file;
	spinlock_t		lock;
	struct work_struct	display_update_ws;
	int			width;
	int			height;
	int			screen_buffer_size;
	u8			*screen_buffer_i2c_chunk;
	u8			*screen_buffer;
};

struct mmap_info {
	char	*data;
	int	reference;
};

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

static void mmap_open(struct vm_area_struct *vma);
static void mmap_close(struct vm_area_struct *vma);
static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

static int op_mmap(struct file *filp, struct vm_area_struct *vma);
static int mmapfop_close(struct inode *inode, struct file *filp);
static int mmapfop_open(struct inode *inode, struct file *filp);

static long
trigger_update(struct file *filp, unsigned int cmd, unsigned long arg);

struct vm_operations_struct mmap_vm_ops = {
	.open	= mmap_open,
	.close	= mmap_close,
	.fault	= mmap_fault,
};

static const struct file_operations mmap_fops = {
	.open		= mmapfop_open,
	.release	= mmapfop_close,
	.mmap		= op_mmap,
	.unlocked_ioctl	= trigger_update,
};

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static void ssd1306_init_lcd(struct i2c_client *client)
{
	struct ssd1306_data *ssd1306 = i2c_get_clientdata(client);

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

	ssd1306_on(ssd1306);
}

/* Update Screen from buffer */
int ssd1306_updateScreen(struct ssd1306_data *ssd1306)
{
	struct i2c_client *client = ssd1306->client;
	int width = ssd1306->width;
	char m;

	client = ssd1306->client;

	for (m = 0; m < 8; m++) {
		SSD1306_WRITE_COMMAND(client, 0xB0 + m);
		SSD1306_WRITE_COMMAND(client, 0x00);
		SSD1306_WRITE_COMMAND(client, 0x10);
		memcpy(&ssd1306->screen_buffer_i2c_chunk[1],
			&ssd1306->screen_buffer[(width * m)],
			width);
		i2c_master_send(
			client, ssd1306->screen_buffer_i2c_chunk, width + 1);
	}
	return 0;
}

static void update_display_work(struct work_struct *work)
{
	struct ssd1306_data *ssd1306 =
		container_of(work, struct ssd1306_data, display_update_ws);

	ssd1306_updateScreen(ssd1306);
}

int ssd1306_on(struct ssd1306_data *ssd1306)
{
	struct i2c_client *client = ssd1306->client;

	SSD1306_WRITE_COMMAND(client, CHARGE_PUMP);
	SSD1306_WRITE_COMMAND(client, 0x14);
	SSD1306_WRITE_COMMAND(client, DISPLAY_ON);
	return 0;
}

int ssd1306_off(struct ssd1306_data *ssd1306)
{
	struct i2c_client *client = ssd1306->client;

	SSD1306_WRITE_COMMAND(client, CHARGE_PUMP);
	SSD1306_WRITE_COMMAND(client, 0x10);
	SSD1306_WRITE_COMMAND(client, DISPLAY_OFF);
	return 0;
}

static long
trigger_update(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ssd1306_data *ssd1306 =
		(struct ssd1306_data *) filp->f_inode->i_private;

	if (_IOC_TYPE(cmd) != IOC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case IOCTL_UPDATE_SCREEN:
		schedule_work(&ssd1306->display_update_ws);
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static void mmap_open(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;

	info->reference++;
}

static void mmap_close(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;

	info->reference--;
}

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct mmap_info *info;

	info = (struct mmap_info *)vma->vm_private_data;
	if (!info->data)
		return 0;
	page = virt_to_page(info->data);
	get_page(page);
	vmf->page = page;
	return 0;
}

static int op_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

static int mmapfop_close(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = filp->private_data;

	kfree(info);
	filp->private_data = NULL;
	return 0;
}

static int mmapfop_open(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
	struct ssd1306_data *ssd1306 = (struct ssd1306_data *)inode->i_private;

	info->data = ssd1306->screen_buffer;
	/* assign this info struct to the file */
	filp->private_data = info;
	return 0;
}

static void ssd1306_destroy(struct ssd1306_data *ssd1306)
{
	if (ssd1306 == NULL)
		return;

	free_page((unsigned long)ssd1306->screen_buffer);
	kfree(ssd1306->screen_buffer_i2c_chunk);
	debugfs_remove(ssd1306->file);
}

static int
ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ssd1306_data *ssd1306;
	struct i2c_adapter  *adapter;
	int ret;

	dev_info(&client->dev, "ssd1306 I2C probe\n");

	ssd1306 = devm_kzalloc(
			&client->dev, sizeof(struct ssd1306_data), GFP_KERNEL);
	if (!ssd1306)
		return -ENOMEM;

	ssd1306->client = client;
	ssd1306->width = DISPLAY_WIDTH;
	ssd1306->height = DISPLAY_HEIGHT;

	ssd1306->screen_buffer_size = ssd1306->width * ssd1306->height / 8;
	ssd1306->screen_buffer = (u8 *)get_zeroed_page(GFP_KERNEL);
	if (!ssd1306->screen_buffer) {
		ret = -ENOMEM;
		goto cleanup;
	}

	ssd1306->screen_buffer_i2c_chunk =
		kzalloc(ssd1306->width + 1, GFP_KERNEL);
	if (!ssd1306->screen_buffer_i2c_chunk) {
		ret = -ENOMEM;
		goto cleanup;
	}

	ssd1306->screen_buffer_i2c_chunk[0] = 0x40;

	ssd1306->file =	debugfs_create_file(
			MMAP_FILE_NAME, 0644, NULL, ssd1306, &mmap_fops);

	if (IS_ERR_OR_NULL(ssd1306->file)) {
		ret = -EBADF;
		goto cleanup;
	}

	spin_lock_init(&ssd1306->lock);
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
	ssd1306_updateScreen(ssd1306);

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

	cancel_work_sync(&ssd1306->display_update_ws);
	ssd1306_destroy(ssd1306);
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
MODULE_AUTHOR("O.Tkachenko");
MODULE_LICENSE("GPL");
