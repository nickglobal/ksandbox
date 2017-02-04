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

#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int TURN_OFF_DISPLAY = 1;
module_param_named( off, TURN_OFF_DISPLAY, int, 0);

static int initTimerValue = 0;
module_param_named( time, initTimerValue, int, 0);

static int mdelay = 4;
module_param_named( md, mdelay, int, 0);

#define DRIVER_AUTHOR  "SanB (O.Bobro)"
#define DRIVER_DESC "Test-san_lcd.ko"
#define CLASS_NAME "san_lcd0"
#define DEVICE_NAME "ssd1306"
#define BUS_NAME    "i2c_1"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

#define WRITECOMMAND  0x00 // 
#define WRITEDATA     0x40 // 

/* register def
#define SSD1306_LCDWIDTH      128
#define SSD1306_LCDHEIGHT      64
#define SSD1306_SETCONTRAST   0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2
 */

typedef struct  {
	u8	byteOffset;
	u8	bitOffset;
} SYMBOL_ROW_OFFSET;

typedef struct ssd1306_data {
	struct i2c_client *client;
	struct class *sanB_LCD_class;
	SYMBOL_ROW_OFFSET hundreds;
	SYMBOL_ROW_OFFSET tens;
	SYMBOL_ROW_OFFSET units;
	struct class_attribute class_attr_time;
	struct class_attribute class_attr_run;
} SSD1306_DATA;

typedef struct {
	struct delayed_work work;
	u16 seconds;
	u16 workStatus;
	SSD1306_DATA *ssd1306;
} TIMERWORK;

/* -------------- hardware description -------------- */
#define SYM_WIDTH	18 	//column
#define SYM_HEIGHT	3	//bytes = 3*8 rows

#define HUNDREDS_COLUMN 29 //29
#define TENS_COLUMN 	55 //55
#define UNITS_COLUMN 	81 //81


#define ARRAY_COL 5
#define ARRAY_ROW SYM_WIDTH

#define DONT_CLEAN_COLUMNS

//numbers are decreasing
const u64 symArray[ARRAY_ROW][ARRAY_COL+1] = {
 //		8		9			6		7			4		5			2		3			0		1			8		9
 { 0x080fe000080007f0, 0x080ff80008000000, 0x08000000080c0000, 0x08f00070080c0007, 0x080ffff008000000, 0x080fe000080007f0 },
 { 0x081ff00008000ff8, 0x081ffe0008000000, 0x0801c000081c0fff, 0x08fc0078081c0007, 0x081ffff808000000, 0x081ff00008000ff8 },
 { 0x083ff87808001ffc, 0x083fff8008000007, 0x0801f000083c0fff, 0x08fc007c083c0007, 0x083ffffc08000000, 0x083ff87808001ffc },
 { 0x08783cfc08003c1e, 0x08783fc008000007, 0x0801f80008780fff, 0x08fe001e08780007, 0x0878001e08000000, 0x08783cfc08003c1e },
 { 0x08f01dfe0800780f, 0x08f01fe008c00007, 0x0801fe0008f00e07, 0x08ee000f08f00007, 0x08f0000f08e00070, 0x08f01dfe0800780f },
 { 0x08e00fcf08007007, 0x08e00ff008f00007, 0x0801ff0008e00e07, 0x08e7000708e00007, 0x08e0000708e00078, 0x08e00fcf08007007 },
 { 0x08e00f8708c07007, 0x08e00e7808fc0007, 0x0801cfc008e00e07, 0x08e7000708e00007, 0x08e0000708e0003c, 0x08e00f8708c07007 },
 { 0x08e00f0708e07007, 0x08e00e3c08ff0007, 0x0801c7e008e00e07, 0x08e3800708e00e07, 0x08e0000708e0001e, 0x08e00f0708e07007 },
 { 0x08e00f0708f07007, 0x08e00e1e083fc007, 0x0801c1f808e00e07, 0x08e3800708e00f07, 0x08e0000708ffffff, 0x08e00f0708f07007 },
 { 0x08e00f0708787007, 0x08e00e0f080ff007, 0x0801c0fe08e00e07, 0x08e1c00708e00f87, 0x08e0000708ffffff, 0x08e00f0708787007 },
 { 0x08e00f07083c7007, 0x08e00e070803fc07, 0x0801c03f08e00e07, 0x08e1c00708e00fc7, 0x08e0000708ffffff, 0x08e00f07083c7007 },
 { 0x08e00f87081e7007, 0x08e00e030800ff07, 0x0801c01f08e00e07, 0x08e0e00708e00fe7, 0x08e0000708ffffff, 0x08e00f87081e7007 },
 { 0x08e00fcf080ff007, 0x08e00e0008003fc7, 0x08ffffff08e00e07, 0x08e0e00708e00ef7, 0x08e0000708e00000, 0x08e00fcf080ff007 },
 { 0x08f01dfe0807f80f, 0x08f01e0008000ff7, 0x08ffffff08f01e07, 0x08e0700f08f01e7f, 0x08f0000f08e00000, 0x08f01dfe0807f80f },
 { 0x08783cfc0803fc1e, 0x08783c00080003ff, 0x08ffffff08783c07, 0x08e0781e08783c3f, 0x0878001e08e00000, 0x08783cfc0803fc1e },
 { 0x083ff8780801fffc, 0x083ff800080000ff, 0x08ffffff083ff807, 0x08e03ffc083ff81f, 0x083ffffc08e00000, 0x083ff8780801fffc },
 { 0x081ff00008007ff8, 0x081ff0000800003f, 0x0801c000081ff007, 0x08e01ff8081ff00f, 0x081ffff808000000, 0x081ff00008007ff8 },
 { 0x080fe00008001ff0, 0x080fe0000800000f, 0x0801c000080fe000, 0x08e00ff0080fe007, 0x080ffff008000000, 0x080fe00008001ff0 },
};

//Start pointer position (byte offset) for each symbol
//0 = (9-0)*4-2;
//1 = (9-1)*4-2;
//2 = (9-2)*4-2;
//..
//7 = (9-7)*4-2;
//8 = (9-8)*4-2;
//9 = 9*4+2; - exception
//#define GET_BYTE_OFFSET(symbol) ((symbol)==9 ? 38 : (9-(symbol))*4-2)
#define GET_BYTE_OFFSET(symbol) ((symbol)==9 ? 38 : 34-(symbol)*4)

/* device models */
enum {
	ssd1306oled,
};
/* device address */
enum {
	addrssd1306  = 0x3c,
};

/* i2c device_id structure */
static const struct i2c_device_id ssd1306_idtable [] = {
		{ "ssd1306", ssd1306oled },
		{ }
};

static atomic_t sysFStime = ATOMIC_INIT(0);

static struct workqueue_struct *timerWQ = NULL;

static DEFINE_SEMAPHORE(ssd1306_sem);
static DEFINE_SPINLOCK(ssd1306_spin);


static void ssd1306_init_lcd(struct i2c_client *drv_client);
//static int ssd1306_ON(struct ssd1306_data *drv_data);
static int ssd1306_OFF(struct ssd1306_data *drv_data);
static int ssd1306_clean_mode_02(struct ssd1306_data *drv_data);
//static int ssd1306_clean_mode_01(struct ssd1306_data *drv_data);
static int setColumnForNumeral(SSD1306_DATA *ssd1306, u8 startColumn);
static void shiftNumeral(SSD1306_DATA *ssd1306, u8 startColumn, u8 cleanColumns);
static void timerWQ_callback( struct work_struct *work );
static void LCD_graphics_init(SSD1306_DATA *ssd1306, unsigned int initSecondsValue);

static ssize_t time_show(struct class *class, struct class_attribute *classAttr, char *buf);
static ssize_t time_store(struct class *class, struct class_attribute *classAttr, const char *buf, size_t count);
static ssize_t run_store(struct class *class, struct class_attribute *classAttr, const char *buf, size_t count);
static int createSysFS(SSD1306_DATA *ssd1306);

static int ssd1306_probe(struct i2c_client *drv_client, const struct i2c_device_id *id);
static int ssd1306_remove(struct i2c_client *drv_client);

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static void ssd1306_init_lcd(struct i2c_client *drv_client)
{
	char m;
	char i;

	printk(KERN_INFO "ssd1306: Device init \n");
	/* Init LCD */
	if (TURN_OFF_DISPLAY)
		i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE); //display off

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //Set Memory Addressing Mode
	// San Bobro: the bug is detected in the next line.
	// Modes are described by "00b", "01b", "10b" in binary format, not hex!
	// So to set "Page Addressing Mode" we should sent 0x02, not 0x10.
	// 0x10 - enables "Vertical Addressing Mode" as and 0x00
	// because the lower 2 bits are taken into account only. (See advanced datasheet).
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x02);	// 0x00 - Horizontal Addressing Mode;
														// 0x01 - Vertical Addressing Mode;
														// 0x02 - Page Addressing Mode (RESET);
														// 0x03 - Invalid.

	i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0); //Set Page Start Address for Page Addressing Mode,0-7
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xC8); //Set COM Output Scan Direction
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //---set low column address
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); //---set high column address
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x40); //--set start line address
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x81); //--set contrast control register
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xFE);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA1); //--set segment re-map 0 to 127
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA6); //--set normal display
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA8); //--set multiplex ratio(1 to 64)
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x3F); //ox3f
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xD3); //-set display offset
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //-not offset

	i2c_smbus_write_byte_data(drv_client, 0x00, 0xD5); //--set display clock divide ratio/oscillator frequency
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xF0); //--set divide ratio //0xa0
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
			i2c_smbus_write_byte_data(drv_client, 0x40, 0x00);
		}
	}
}

/**/
//static int ssd1306_ON(struct ssd1306_data *drv_data) {
//	struct i2c_client *drv_client;
//
//	drv_client = drv_data->client;
//
//	i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
//	i2c_smbus_write_byte_data(drv_client, 0x00, 0x14);
//	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF);
//	return 0;
//}

/**/
static int ssd1306_OFF(struct ssd1306_data *drv_data) {
	struct i2c_client *drv_client;

	drv_client = drv_data->client;

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE);
	return 0;
}

static int ssd1306_clean_mode_02(struct ssd1306_data *drv_data)
{
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
			i2c_smbus_write_byte_data(drv_client, 0x40, 0x00);
		}
	}

	return 0;
}

//static int ssd1306_clean_mode_01(struct ssd1306_data *drv_data)
//{
//	// It isn't needed now.
//	return 0;
//}

static int setColumnForNumeral(SSD1306_DATA *ssd1306, u8 startColumn)
{
	u8 blockData[6];
	blockData[0] = 0x21;
	blockData[1] = startColumn;				//Start column
	blockData[2] = startColumn+SYM_WIDTH-1;	//End column
	blockData[3] = 0x22;
	blockData[4] = 0x01;					//Start page
	blockData[5] = 0x07;					//End page
	return i2c_smbus_write_i2c_block_data(ssd1306->client, 0x00, 6, blockData);
}

static void shiftNumeral(SSD1306_DATA *ssd1306, u8 startColumn, u8 cleanColumns)
{
	int i, j, ret;
	u64 tmp;
	static u8 prevStartColumn = 0;
	u8 empty[7] = { 0 };
	u8 startColumnTmp, cleanColumnsTmp;

#ifdef DONT_CLEAN_COLUMNS
	cleanColumns = 0;
#endif

	if ( (-ETIME == down_timeout(&ssd1306_sem, msecs_to_jiffies(800))) )
	{
		switch (startColumn)
		{//withour "break;"
		case HUNDREDS_COLUMN:
			ssd1306->hundreds.byteOffset = (ssd1306->hundreds.byteOffset+4) % (ARRAY_COL*8);
		case TENS_COLUMN:
			ssd1306->tens.byteOffset = (ssd1306->tens.byteOffset+4) % (ARRAY_COL*8);
		default:
			ssd1306->units.byteOffset = (ssd1306->units.byteOffset+4) % (ARRAY_COL*8);
		}
		return;
	}

	for (j=0; j < 32; j++)
	{
		startColumnTmp = startColumn;
		cleanColumnsTmp = cleanColumns;
		if (startColumnTmp == HUNDREDS_COLUMN)
		{
			if ( likely(startColumnTmp != prevStartColumn ))
			{
				setColumnForNumeral(ssd1306, HUNDREDS_COLUMN);
				prevStartColumn = HUNDREDS_COLUMN;
			}
			startColumnTmp = TENS_COLUMN;

			if (cleanColumnsTmp)
			{
				for (i = 0; i < SYM_WIDTH; i++)
					i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, &empty[0]);
				cleanColumnsTmp = 0;
			}
			else
			{
				for (i = 0; i < SYM_WIDTH; i++)
				{
					tmp = *( (u64*) ((u8*)&(symArray[i][0]) + ssd1306->hundreds.byteOffset) );
					tmp = tmp >> ssd1306->hundreds.bitOffset;

					ret = i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, (u8*)&tmp);
					if (unlikely(ret))
						printk(KERN_ERR "%s(): i2c_smbus_write_i2c_block_data has returned %d\n", __func__, ret);
				}

				ssd1306->hundreds.bitOffset = (ssd1306->hundreds.bitOffset+1) % 8;
				if (unlikely(ssd1306->hundreds.bitOffset == 0))
					ssd1306->hundreds.byteOffset = (ssd1306->hundreds.byteOffset+1) % (ARRAY_COL*8);
			}
		}

		if (startColumnTmp == TENS_COLUMN)
		{
			if ( likely(startColumnTmp != prevStartColumn ))
			{
				setColumnForNumeral(ssd1306, TENS_COLUMN);
				prevStartColumn = TENS_COLUMN;
			}
			startColumnTmp = UNITS_COLUMN;
			if (cleanColumnsTmp)
			{
				for (i = 0; i < SYM_WIDTH; i++)
					i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, &empty[0]);
				cleanColumnsTmp = 0;
			}
			else
			{
				for (i = 0; i < SYM_WIDTH; i++)
				{
					tmp = *( (u64*) ((u8*)&(symArray[i][0]) + ssd1306->tens.byteOffset) );
					tmp = tmp >> ssd1306->tens.bitOffset;

					ret = i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, (u8*)&tmp);
					if (unlikely(ret))
						printk(KERN_ERR "%s(): i2c_smbus_write_i2c_block_data has returned %d\n", __func__, ret);
				}

				ssd1306->tens.bitOffset = (ssd1306->tens.bitOffset+1) % 8;
				if (unlikely(ssd1306->tens.bitOffset == 0))
					ssd1306->tens.byteOffset = (ssd1306->tens.byteOffset+1) % (ARRAY_COL*8);
			}
		}

		if (startColumnTmp == UNITS_COLUMN)
		{
			if ( likely(startColumnTmp != prevStartColumn ))
			{
				setColumnForNumeral(ssd1306, UNITS_COLUMN);
				prevStartColumn = UNITS_COLUMN;
			}
			for (i = 0; i < SYM_WIDTH; i++)
			{
				tmp = *( (u64*) ((u8*)&(symArray[i][0]) + ssd1306->units.byteOffset) );
				tmp = tmp >> ssd1306->units.bitOffset;

				ret = i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, (u8*)&tmp);
				if (unlikely(ret))
					printk(KERN_ERR "%s(): i2c_smbus_write_i2c_block_data has returned %d\n", __func__, ret);
			}

			ssd1306->units.bitOffset = (ssd1306->units.bitOffset+1) % 8;
			if (unlikely(ssd1306->units.bitOffset == 0))
				ssd1306->units.byteOffset = (ssd1306->units.byteOffset+1) % (ARRAY_COL*8);
		}

		if (mdelay)
			msleep( mdelay );
	} // for (j=0; j < 32; j++)

	up(&ssd1306_sem);
}

static void timerWQ_callback( struct work_struct *work )
{
	unsigned long startHZ = jiffies;
	TIMERWORK* timeWork = (TIMERWORK*)to_delayed_work(work);

//	printk(KERN_DEBUG "%s(): @@@@@@@@@@ seconds = %u\n", __func__, timeWork->seconds);
	if ( timeWork->seconds == 0)
		return;

	spin_lock_bh(&ssd1306_spin);
	timeWork->seconds--;
	spin_unlock_bh(&ssd1306_spin);

	if ( timeWork->seconds%100 == 99 )
		shiftNumeral( timeWork->ssd1306, HUNDREDS_COLUMN, (timeWork->seconds == 99) );
	else if ( timeWork->seconds%10 == 9 )
		shiftNumeral( timeWork->ssd1306, TENS_COLUMN, (timeWork->seconds == 9) );
	else
		shiftNumeral( timeWork->ssd1306, UNITS_COLUMN, 0);

	if (timeWork->seconds > 0)
	{
		//Calculate the delay that we should wait after the screen update
		// (the update can last different times) to get period == 1s exactly.
		long delay = msecs_to_jiffies(1000) + startHZ - jiffies;/*msecs_to_jiffies(1000) - (jiffies-startHZ)*/
		if (delay < 0)
			delay = 0;
		schedule_delayed_work((struct delayed_work *)timeWork, (unsigned long)delay);
	}

	return;
}

static void LCD_graphics_init(SSD1306_DATA *ssd1306, unsigned int initSecondsValue)
{
	int i, ret;
	u8 blockData[7];//[I2C_SMBUS_BLOCK_MAX];
	u64 tmp;
	struct i2c_client *drv_client = ssd1306->client;

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //Set Memory Addressing Mode
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);	// 0x00 - Horizontal Addressing Mode;
														// 0x01 - Vertical Addressing Mode;
														// 0x02 - Page Addressing Mode (RESET);
														// 0x03 - Invalid.
	//Drow left triangle
	blockData[0] = 0x21;
	blockData[1] = 8;
	blockData[2] = 8 + 3;
	blockData[3] = 0x22;
	blockData[4] = 0x04;
	blockData[5] = 0x04;
	i2c_smbus_write_i2c_block_data(drv_client, 0x00, 6, blockData);
	blockData[0] = 0xff;
	blockData[1] = 0x7e;
	blockData[2] = 0x3c;
	blockData[3] = 0x18;
	i2c_smbus_write_i2c_block_data(drv_client, 0x40, 4, blockData);

	//Drow right triangle
	blockData[0] = 0x21;
	blockData[1] = SSD1306_WIDTH - 8 - 4;
	blockData[2] = SSD1306_WIDTH - 8 - 4 + 3;
	blockData[3] = 0x22;
	blockData[4] = 0x04;
	blockData[5] = 0x04;
	i2c_smbus_write_i2c_block_data(drv_client, 0x00, 6, blockData);
	blockData[0] = 0x18;
	blockData[1] = 0x3c;
	blockData[2] = 0x7e;
	blockData[3] = 0xff;
	i2c_smbus_write_i2c_block_data(drv_client, 0x40, 4, blockData);

	memset(blockData, 0, sizeof(blockData));

	initSecondsValue = (initSecondsValue > 999 || initSecondsValue < 0) ? 0 : initSecondsValue;

	if ( initSecondsValue/100 > 0)
	{
		setColumnForNumeral(ssd1306, HUNDREDS_COLUMN);
		ssd1306->hundreds.byteOffset = GET_BYTE_OFFSET(initSecondsValue/100%10);
		ssd1306->hundreds.bitOffset = 0;
		for (i = 0; i < SYM_WIDTH; i++)
		{
			tmp = *( (u64*) ((u8*)&(symArray[i][0]) + ssd1306->hundreds.byteOffset) );

			ret = i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, (u8*)&tmp);
			if (unlikely(ret))
				printk(KERN_ERR "%s(): i2c_smbus_write_i2c_block_data has returned %d\n", __func__, ret);
		}
		ssd1306->hundreds.bitOffset++;
		ssd1306->hundreds.bitOffset %= 8;
		if (unlikely(ssd1306->hundreds.bitOffset == 0))
		{
			ssd1306->hundreds.byteOffset++;
			ssd1306->hundreds.byteOffset %= (ARRAY_COL*8);
		}
	}
	else
	{
		setColumnForNumeral(ssd1306, HUNDREDS_COLUMN);
		for (i = 0; i < SYM_WIDTH; i++)
			i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, &blockData[0]);
	}

	if ( initSecondsValue/10 > 0 )
	{
		setColumnForNumeral(ssd1306, TENS_COLUMN);
		ssd1306->tens.byteOffset = GET_BYTE_OFFSET(initSecondsValue/10%10);
		ssd1306->tens.bitOffset = 0;
		for (i = 0; i < SYM_WIDTH; i++)
		{
			tmp = *( (u64*) ((u8*)&(symArray[i][0]) + ssd1306->tens.byteOffset) );

			ret = i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, (u8*)&tmp);
			if (unlikely(ret))
				printk(KERN_ERR "%s(): i2c_smbus_write_i2c_block_data has returned %d\n", __func__, ret);
		}
		ssd1306->tens.bitOffset++;
		ssd1306->tens.bitOffset %= 8;
		if (unlikely(ssd1306->tens.bitOffset == 0))
		{
			ssd1306->tens.byteOffset++;
			ssd1306->tens.byteOffset %= (ARRAY_COL*8);
		}
	}
	else
	{
		setColumnForNumeral(ssd1306, TENS_COLUMN);
		for (i = 0; i < SYM_WIDTH; i++)
			i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, &blockData[0]);
	}

	setColumnForNumeral(ssd1306, UNITS_COLUMN);
	ssd1306->units.byteOffset = GET_BYTE_OFFSET(initSecondsValue%10);
	ssd1306->units.bitOffset = 0;
	for (i = 0; i < SYM_WIDTH; i++)
	{
		tmp = *( (u64*) ((u8*)&(symArray[i][0]) + ssd1306->units.byteOffset) );

		ret = i2c_smbus_write_i2c_block_data(ssd1306->client, 0x40, 7, (u8*)&tmp);
		if (unlikely(ret))
			printk(KERN_ERR "%s(): i2c_smbus_write_i2c_block_data has returned %d\n", __func__, ret);
	}
	ssd1306->units.bitOffset++;
	ssd1306->units.bitOffset %= 8;
	if (unlikely(ssd1306->units.bitOffset == 0))
	{
		ssd1306->units.byteOffset++;
		ssd1306->units.byteOffset %= (ARRAY_COL*8);
	}
}


static ssize_t time_show(struct class *class, struct class_attribute *classAttr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Timer's start value = %u\n", atomic_read(&sysFStime));
}

static ssize_t time_store(struct class *class, struct class_attribute *classAttr, const char *buf, size_t count)
{
	int ret;
	unsigned int time;

	ret = sscanf(buf, "%u", &time);
	if (unlikely(ret != 1))
	{
		printk(KERN_ERR "time_store(): can not recognize the string \"%s\"\n", buf);
		return -EINVAL;
	}
	if (time > 999)
	{
		printk(KERN_ERR "time_store(): entered initial timer value %u is too large. Use 0 <= time <= 999 .\n", time);
		return -EINVAL;
	}
	atomic_set(&sysFStime, time ); //Inputed value
	printk(KERN_INFO "time_store(): stored time = %u\n", time);
	return count;
}

static ssize_t run_store(struct class *class, struct class_attribute *classAttr, const char *buf, size_t count)
{
	int ret;
	TIMERWORK *timerDelayedWork;
	unsigned int run;


	SSD1306_DATA *ssd1306 = container_of(classAttr, SSD1306_DATA, class_attr_run);
	timerDelayedWork = dev_get_drvdata( &(ssd1306->client->dev) );

	ret = sscanf(buf, "%u", &run);
	if (unlikely(ret != 1))
	{
		printk(KERN_ERR "run_store(): can not recognize the string \"%s\"\n", buf);
		return -EINVAL;
	}

	if (timerDelayedWork)
	{
		cancel_delayed_work_sync(&(timerDelayedWork->work));
		if (run)
		{
			spin_lock_bh(&ssd1306_spin);
			timerDelayedWork->seconds = atomic_read(&sysFStime);
			spin_unlock_bh(&ssd1306_spin);

			LCD_graphics_init(ssd1306, timerDelayedWork->seconds);

			schedule_delayed_work(&(timerDelayedWork->work), msecs_to_jiffies(800));
			printk(KERN_INFO "run_store(): run timer with time = %u\n", atomic_read(&sysFStime));
		}
		else
			printk(KERN_INFO "run_store(): timer is stoped\n");
	}

	return count;
}

static int createSysFS(SSD1306_DATA *ssd1306)
{
	long ret;

	// Create class:
	ssd1306->sanB_LCD_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(ssd1306->sanB_LCD_class))
	{
		ret = PTR_ERR(ssd1306->sanB_LCD_class);
		dev_err(&ssd1306->client->dev,
				"ERROR: class_create("CLASS_NAME") has returned error: %ld\n", ret);
		ssd1306->sanB_LCD_class = NULL;
		return (int)ret;
	}

	ssd1306->class_attr_time.attr.name = "time";
	ssd1306->class_attr_time.attr.mode = 0666;
	ssd1306->class_attr_time.show = time_show;
	ssd1306->class_attr_time.store = time_store;
	ret = class_create_file(ssd1306->sanB_LCD_class, &(ssd1306->class_attr_time) );
	if (unlikely(IS_ERR_VALUE(ret)))
	{
		dev_err(&ssd1306->client->dev,
				"ERROR: class_create_file(\"class_attr_time\") has returned error: %ld\n", ret);
		goto RESTORE_SYSFS_1;
	}

	ssd1306->class_attr_run.attr.name = "run";
	ssd1306->class_attr_run.attr.mode = 0222;
	ssd1306->class_attr_run.store  = run_store;
	ret = class_create_file(ssd1306->sanB_LCD_class, &(ssd1306->class_attr_run) );
	if (unlikely(IS_ERR_VALUE(ret)))
	{
		dev_err(&ssd1306->client->dev,
				"ERROR: class_create_file(\"class_attr_run\") has returned error: %ld\n", ret);
		goto RESTORE_SYSFS_2;
	}

	return 0;

	RESTORE_SYSFS_2:
	class_remove_file(ssd1306->sanB_LCD_class, &ssd1306->class_attr_time);
	RESTORE_SYSFS_1:
	class_destroy(ssd1306->sanB_LCD_class);
	ssd1306->sanB_LCD_class = NULL;
	return (int)ret;
}

static int
ssd1306_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
	int ret;
	SSD1306_DATA *ssd1306;
	struct i2c_adapter *adapter;
	TIMERWORK *timerDelayedWork = NULL;

	printk(KERN_INFO "init I2C driver\n");

	ssd1306 = devm_kzalloc(&drv_client->dev, sizeof(SSD1306_DATA),
			GFP_KERNEL);
	if (!ssd1306)
		return -ENOMEM;

	ssd1306->client = drv_client;

	i2c_set_clientdata(drv_client, ssd1306);

	adapter = drv_client->adapter;
	if (!adapter)
	{
		dev_err(&drv_client->dev, "adapter identification error\n");
		return -ENODEV;
	}

	printk(KERN_INFO "I2C client address %d \n", drv_client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&drv_client->dev, "I2C_FUNC_SMBUS_BYTE_DATA operation not supported\n");
		return -ENODEV;
	}
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&drv_client->dev, "I2C_FUNC_SMBUS_WORD_DATA operation not supported\n");
		return -ENODEV;
	}
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&drv_client->dev, "I2C_FUNC_SMBUS_I2C_BLOCK operation not supported\n");
		return -ENODEV;
	}


	ssd1306_init_lcd( drv_client);
	//    ssd1306_GotoXY(ssd1306, 5, 4);
	//    ssd1306_Puts(ssd1306, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
	//    ssd1306_UpdateScreen(ssd1306);
	ssd1306_clean_mode_02(ssd1306);

	ret = createSysFS(ssd1306);
	if ( unlikely(ret) )
		return ret;

	initTimerValue = (initTimerValue > 999 || initTimerValue < 0) ? 0 : initTimerValue;
	LCD_graphics_init(ssd1306, initTimerValue); //Preparation for my graphic mode.

	timerWQ = create_singlethread_workqueue( "timerWQ" );
	if( likely(timerWQ) )
	{
		/* Queue some work */
		timerDelayedWork = (TIMERWORK*)devm_kzalloc(&drv_client->dev, sizeof(TIMERWORK), GFP_KERNEL );
		if( likely(timerDelayedWork) )
		{
			INIT_DELAYED_WORK( (struct delayed_work *)timerDelayedWork, timerWQ_callback );
			timerDelayedWork->seconds = initTimerValue;
			timerDelayedWork->ssd1306 = ssd1306;
			ret = queue_delayed_work( timerWQ, (struct delayed_work *)timerDelayedWork, msecs_to_jiffies(800) );
		}
		else
		{
			destroy_workqueue( timerWQ );
			dev_err(&drv_client->dev, "ERROR: ssd1306 driver isn't loaded. timerDelayedWork == NULL\n\n");
			return -1;
		}
	}
	else
	{
		dev_err(&drv_client->dev, "ERROR: ssd1306 driver isn't loaded.  "
				"create_singlethread_workqueue() has returned NULL\n\n");
		return -1;
	}

	dev_set_drvdata(&drv_client->dev, timerDelayedWork);
	dev_info(&drv_client->dev, "ssd1306 driver successfully loaded\n\n");
	return 0;
}

static int
ssd1306_remove(struct i2c_client *drv_client)
{
	TIMERWORK *timerDelayedWork;

	timerDelayedWork = dev_get_drvdata(&drv_client->dev);
	if (timerDelayedWork)
		cancel_delayed_work_sync(&(timerDelayedWork->work));

	if (timerWQ)
	{
		flush_workqueue( timerWQ );
		destroy_workqueue( timerWQ );
	}

	if (likely(timerDelayedWork->ssd1306->sanB_LCD_class))
	{
		class_remove_file(timerDelayedWork->ssd1306->sanB_LCD_class, &timerDelayedWork->ssd1306->class_attr_run);
		class_remove_file(timerDelayedWork->ssd1306->sanB_LCD_class, &timerDelayedWork->ssd1306->class_attr_time);
		class_destroy(timerDelayedWork->ssd1306->sanB_LCD_class);
		timerDelayedWork->ssd1306->sanB_LCD_class = NULL;
	}
	else
	{
		dev_info(&drv_client->dev, "ssd1306_remove(): sanB_LCD_class is NULL.\n");
	}

	if (TURN_OFF_DISPLAY)
		ssd1306_OFF(timerDelayedWork->ssd1306);

	dev_info(&drv_client->dev, "ssd1306 driver successfully unloaded\n");
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


/* Module init */
static int __init ssd1306_init ( void )
{
	int ret;
	/*
	 * An i2c_driver is used with one or more i2c_client (device) nodes to access
	 * i2c slave chips, on a bus instance associated with some i2c_adapter.
	 */
	printk(KERN_ERR "********************************\n");
	printk(KERN_INFO "%s(): ssd1306 mod init\n", __func__);
	ret = i2c_add_driver ( &ssd1306_driver);
	if(ret)
	{
		printk(KERN_ERR "%s(): failed to add new i2c driver\n", __func__);
		return ret;
	}

	return ret;
}

/* Module exit */
static void __exit ssd1306_exit(void)
{
	i2c_del_driver(&ssd1306_driver);
	printk(KERN_INFO "%s(): ssd1306: cleanup\n\n", __func__);
}

/* -------------- kernel thread description -------------- */
module_init(ssd1306_init);
module_exit(ssd1306_exit);


MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("GL team");
MODULE_LICENSE("GPL");
