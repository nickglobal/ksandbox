#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>

//#include <delay.h>

#include <math.h>
#include <unistd.h>

typedef unsigned char 	u8;
typedef unsigned short 	u16;

#define SSD1306_WIDTH	128
#define SSD1306_HEIGHT	64




typedef enum {
        SSD1306_COLOR_BLACK = 0x00,   /*!< Black color, no pixel */
        SSD1306_COLOR_WHITE = 0x01    /*!< Pixel is set. Color depends on LCD */
} ssd1306_COLOR_t;


typedef struct {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    int width;
    int height;
    long int screensize;
    char *fbp;

}lcd_type;

static lcd_type lcd;


static u8 ssd1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];




#include "Point.h"
#include "Font.h"
#include "Font_Arial_Black_6x8.c"

typedef enum{
	_1bpp	= 1	,
	_2bpp		,
	_8bpp		,
}bpp_enum;


typedef struct{
	_Point DispPos;
	u16 BKColor;
	u16 FGColor;
	FONT_INFO *pFont;
}GraphContext_type;


GraphContext_type GraphContext;

static u08 BitsPerPixel = 2;


void 	Graphic_SetForeground(u16 _color);
u16 	Graphic_GetForeground(void);
void 	Graphic_SetBackground(u16 _color);
u16 	Graphic_GetBackground(void);

void Graphic_SetPos(_Point _pos);
_Point Graphic_GetPos(void);


FONT_INFO *Graphic_GetFont();
void Graphic_SetFont(FONT_INFO *_pFont);


void Graphic_SetForeground(u16 _color);

void Graphic_setPoint_(const u16 X, const u16 Y, const u16 _color);

#include "Font.c"



static int ssd1306_DrawPixel(u16 x, u16 y, ssd1306_COLOR_t color) {

    if ( x > SSD1306_WIDTH || y > SSD1306_HEIGHT ) {
        return -1;
    }

    /* Set color */
    if (color == SSD1306_COLOR_WHITE) {
        ssd1306_Buffer[x/8 + (y * (SSD1306_WIDTH / 8))] |= (1 << (x % 8));
    }
    else {
        ssd1306_Buffer[x/8 + (y * (SSD1306_WIDTH / 8))] &= ~(1 << (x % 8));
    }

    return 0;
}


void Graphic_setPoint_(const u16 X, const u16 Y, const u16 _color){

	if (X >= SSD1306_WIDTH || Y >= SSD1306_HEIGHT){
		return;
    }
    //char ch = ' ';
	ssd1306_DrawPixel(X, Y, _color);
}


void Graphic_SetForeground(u16 _color){
	
	switch(BitsPerPixel){
	case _2bpp:
		_color &= 0x03;
		break;
	case _8bpp:
		_color &= 0xFFFF;
		break;
	}
	
	GraphContext.FGColor = _color;
}

u16 Graphic_GetForeground(void){
	return 1;//GraphContext.FGColor;
}


void Graphic_SetBackground(u16 _color){
	
	switch(BitsPerPixel){
	case _2bpp:
		_color &= 0x03;
		break;
	case _8bpp:
		_color &= 0xFFFF;
		break;
	}
	
	GraphContext.BKColor = _color;
}


u16 Graphic_GetBackground(void){
	return 0;//GraphContext.BKColor;
}



void Graphic_SetPos(_Point _pos){
	GraphContext.DispPos = _pos;
}

_Point Graphic_GetPos(void){
	return GraphContext.DispPos;
}


static void Graphic_setPoint(const u16 X, const u16 Y)
{
	ssd1306_DrawPixel(X, Y, SSD1306_COLOR_WHITE);
}



FONT_INFO *Graphic_GetFont(){
	return GraphContext.pFont;
}

void Graphic_SetFont(FONT_INFO *_pFont){
	if(_pFont){
		GraphContext.pFont = _pFont;
	}
}


void Graphic_drawString(char *s){
	if(s)
		Font_DispString(s);
}


static void Graphic_drawLine(_Point p1, _Point p2){
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


static void Graphic_drawLine_(u16 x1, u16 y1, u16 x2, u16 y2){
	_Point p1 = {0}, p2 = {0};
	p1.X = x1;
	p1.Y = y1;
	p2.X = x2;
	p2.Y = y2;

	Graphic_drawLine(p1, p2);
}
// ---------------------------------------------------------------------------


void Graphic_drawFilledRectangle(u16 x, u16 y, u16 w, u16 h){

	_Point p1 = {0}, p2 = {0};
	u16 cnt = h;

	p1.X = x;
	p1.Y = y;
	p2.X = x + w;
	p2.Y = y;

	while(cnt--){
		Graphic_drawLine(p1, p2);
		p1.Y +=1;
		p2.Y +=1;
	}
}

static void Graphic_ClearScreen(void){
	memset(ssd1306_Buffer, 0, sizeof(ssd1306_Buffer));
}


static void Graphic_UpdateScreen(void){

	int i = 0;

	for (i=0; i < lcd.screensize; i++) {

        *(lcd.fbp+i) = ssd1306_Buffer[i];

    } 
}

 


int fb;
struct fb_var_screeninfo fb_var;
struct fb_var_screeninfo fb_screen;
struct fb_fix_screeninfo fb_fix;
unsigned char *fb_mem = NULL;
unsigned char *fb_screenMem = NULL;



#ifndef PAGE_SHIFT
	#define PAGE_SHIFT 12
#endif
#ifndef PAGE_SIZE
	#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif
#ifndef PAGE_MASK
	#define PAGE_MASK (~(PAGE_SIZE - 1))
#endif

int fb_init(char *device) {
 
	int fb_mem_offset;
 
	// get current settings (which we have to restore)
	if (-1 == (fb = open(device, O_RDWR))) {
		fprintf(stderr, "Open %s error\n", device);
		return 0;
	}
	if (-1 == ioctl(fb, FBIOGET_VSCREENINFO, &fb_var)) {
		fprintf(stderr, "Ioctl FBIOGET_VSCREENINFO error.\n");
		return 0;
	}
	if (-1 == ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix)) {
		fprintf(stderr, "Ioctl FBIOGET_FSCREENINFO error.\n");
		return 0;
	}
	if (fb_fix.type != FB_TYPE_PACKED_PIXELS) {
		fprintf(stderr, "Can handle only packed pixel frame buffers.\n");
		goto err;
	}
 
	fb_mem_offset = (unsigned long)(fb_fix.smem_start) & (~PAGE_MASK);
	fb_mem = mmap(NULL, fb_fix.smem_len + fb_mem_offset, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
	if (-1L == (long)fb_mem) {
		fprintf(stderr, "Mmap error.\n");
		goto err;
	}
	// move viewport to upper left corner
	if (fb_var.xoffset != 0 || fb_var.yoffset != 0) {
		fb_var.xoffset = 0;
		fb_var.yoffset = 0;
		if (-1 == ioctl(fb, FBIOPAN_DISPLAY, &fb_var)) {
			fprintf(stderr, "Ioctl FBIOPAN_DISPLAY error.\n");
			munmap(fb_mem, fb_fix.smem_len);
			goto err;
		}
	}
 

    lcd.width 		= fb_var.xres;
    lcd.height 		= fb_var.yres;
    lcd.screensize 	= fb_var.xres * fb_var.yres * fb_var.bits_per_pixel / 8;
    lcd.fbp 		= fb_mem;

	return 1;
 
err:
	if (-1 == ioctl(fb, FBIOPUT_VSCREENINFO, &fb_var))
		fprintf(stderr, "Ioctl FBIOPUT_VSCREENINFO error.\n");
	if (-1 == ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix))
		fprintf(stderr, "Ioctl FBIOGET_FSCREENINFO.\n");
	return 0;
}

 
void fb_cleanup(void) {
 
	if (-1 == ioctl(fb, FBIOPUT_VSCREENINFO, &fb_var))
		fprintf(stderr, "Ioctl FBIOPUT_VSCREENINFO error.\n");
	if (-1 == ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix))
		fprintf(stderr, "Ioctl FBIOGET_FSCREENINFO.\n");
	munmap(fb_mem, fb_fix.smem_len);
	close(fb);
}


FILE* fx;
FILE* fy;
FILE* fz;
const char *fd_accel_x = "/sys/bus/iio/devices/iio:device0/in_accel_x_raw";
const char *fd_accel_y = "/sys/bus/iio/devices/iio:device0/in_accel_y_raw";
const char *fd_accel_z = "/sys/bus/iio/devices/iio:device0/in_accel_z_raw";


FILE *open_accel_file(const char *path){
	FILE *f;
	f = fopen(path, "r");

	return f;
}


int read_raw_data(FILE *file){
	int data = 0;	
	int cnt = 0;
	char tmpStr[16] = {0};
	
	fseek(file, 0, SEEK_SET);
	cnt = fread(tmpStr, sizeof(tmpStr), 1, file);
	
	data = 0;
	sscanf(tmpStr, "%d", &data);
	return data;
}



int normilize_accel_data(int data)
{
	long _data = (data * 60)/9000;

	return (int)_data;
}



void draw_dotted_line(int y)
{
	memset(&ssd1306_Buffer[16*y], 0x11, lcd.width/8);
}


void draw_accel_line(int x, int y, int raw_data)
{
	int w = normilize_accel_data(raw_data);
	if(w < 0){
		w = -w;
		x -= w;
	}
	Graphic_drawFilledRectangle(x, y - 4, w, 4);
}




int main(int argc, char **argv)
{
    int x = 0, w = 0, i = 0;
    long int location = 0;
    

	char tmpStr[16] = {0};
	_Point pos = {10, 10};
	FONT_INFO *pFont = 0;


	int raw_x = 0;
	int raw_y = 0;
	int raw_z = 0;
	int cnt = 0;


	if (!fb_init("/dev/fb0"))
		exit(1);


	fx = open_accel_file(fd_accel_x);
	if (fx == NULL) {
		printf("Open %s error\n", fd_accel_x);
		return 1;
	}
	fy = open_accel_file(fd_accel_y);
	if (fx == NULL) {
		printf("Open %s error\n", fd_accel_y);
		return 1;
	}
	fz = open_accel_file(fd_accel_z);
	if (fx == NULL) {
		printf("Open %s error\n", fd_accel_z);
		return 1;
	}


	printf("accel files opened\n");

	GraphContext.pFont 		= &Font_Arial_Black_6x8;
	pFont = Graphic_GetFont();

	Graphic_ClearScreen();


	Graphic_SetPos(pos);
	Graphic_drawString("Hello, World!");

	pos.Y += pFont->Height;
	Graphic_SetPos(pos);
	Graphic_drawString("DAndy");

	Graphic_UpdateScreen();
	sleep(2);




	for(cnt = 0; cnt <= 1000; cnt++){

		raw_x = read_raw_data(fx);
		raw_y = read_raw_data(fy);
		raw_z = read_raw_data(fz);
		printf("raw_x = %d; raw_y = %d; raw_z = %d\n", raw_x, raw_y, raw_z);

		Graphic_ClearScreen();

		pos.X = 0; pos.Y = 0;
		Graphic_SetPos(pos);
		Graphic_drawString("X:");

		pos.Y += pFont->Height + 6;
		draw_dotted_line(pos.Y);

		x = lcd.width/2;
		draw_accel_line(x, pos.Y, raw_x);



		pos.Y += pFont->Height;
		Graphic_SetPos(pos);
		Graphic_drawString("Y:");

		pos.Y += pFont->Height + 6;
		draw_dotted_line(pos.Y);

		x = lcd.width/2;
		draw_accel_line(x, pos.Y, raw_y);


		pos.Y += pFont->Height;
		Graphic_SetPos(pos);
		Graphic_drawString("Z:");

		pos.Y += pFont->Height + 6;
		draw_dotted_line(pos.Y);

		x = lcd.width/2;
		draw_accel_line(x, pos.Y, raw_z);


		Graphic_drawLine_(lcd.width/2, 0, lcd.width/2, lcd.height-1);

		Graphic_UpdateScreen();
		//sleep(1);
		usleep(100*1000);
	}


	fclose(fx);
	fclose(fy);
	fclose(fz);    

	fb_cleanup();
	return(0);
}