#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define FB_NAME_DEF	"/dev/fb0"
#define GYRO_PATH	"/sys/devices/platform/ocp/4819c000.i2c/i2c-2/2-0068/iio:device0/"

#define INVERT_COLORS	0

#define SCREEN_WIDTH	128
#define SCREEN_HEIGHT	64
#define SCREEN_SIZE	(SCREEN_WIDTH * SCREEN_HEIGHT / 8)

#define SOLID		0
#define DOTTED		1

#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))


uint8_t *g_fbp;


static void clear_screen(void)
{
	uint8_t val = 0;

#if INVERT_COLORS
	val ^= 0xff;
#endif

	memset(g_fbp, val, SCREEN_SIZE);
}

static void putpixel(int x, int y, uint8_t color)
{
	int totalpos;
	int bytepos, bitpos;

	totalpos = y * SCREEN_WIDTH + x;
	bytepos = totalpos / 8;
	bitpos = totalpos & 7;
#if INVERT_COLORS
	color ^= 1;
#endif

	g_fbp[bytepos] &= ~(1 << bitpos);
	g_fbp[bytepos] |= (color << bitpos);
}

static void line(int x1, int y1, int x2, int y2, uint16_t style)
{
	float k, b;
	int delta, x, y;

	delta = (style == DOTTED) ? 2 : 1;

	/* ToDo: add special case when deltaY > deltaX
	   this will remove vertical line case
	 */

	/* Vertical line */
	if (x1 == x2) {
		if (y1 > y2) {
			y = y2;
			y2 = y1;
			y1 = y;
		}
		for (y = y1; y <= y2; y += delta)
			putpixel(x1, y, 1);

	/* Horizontal or diagonal */
	} else {
		if (x1 > x2) {
			x = x2;
			x2 = x1;
			x1 = x;
			y = y2;
			y2 = y1;
			y1 = y;
		}
		/* Calculate y=kx+b */
		k = (float)(y2 - y1) / (x2 - x1);
		b = (float)y1 - k * x1;
		/* Draw */
		for (x = x1; x <= x2; x += delta) {
			y = (int)(k * x + b + 0.5);
			putpixel(x, y, 1);
		}
	}
}

static void draw_axes(void)
{
	line(9,  63, 119, 0,  DOTTED); /* X */
	line(9,   0, 119, 63, DOTTED); /* Y */
	line(64,  0, 64,  63, DOTTED); /* Z */
}

static void draw_pos(int gyrox, int gyroy, int gyroz, int maxval)
{
	int centerx, centery;
	float scale;
	float dx, dy, dz;
	int x[3], y[3];

	centerx = SCREEN_WIDTH / 2;
	centery = SCREEN_HEIGHT / 2;

	scale = (float)min(centerx, centery) / maxval;

	dx = scale * gyrox;
	dy = scale * gyroy;
	dz = scale * gyroz;

	/* X */
	x[0] = centerx - dx * 0.866;
	y[0] = centery + dx * 0.5;
	/* Y */
	x[1] = centerx + dy * 0.866;
	y[1] = centery + dy * 0.5;
	/* Z */
	x[2] = centerx;
	y[2] = centery + dz;

	/* Draw */
	line(x[0], y[0], x[1], y[1], SOLID);
	line(x[1], y[1], x[2], y[2], SOLID);
	line(x[0], y[0], x[2], y[2], SOLID);
} 

static void update_screen(int x, int y, int z, int maxval)
{
	clear_screen();
	draw_axes();
	draw_pos(x, y, z, maxval);
}

static void get_axes_raw(int *x, int *y, int *z)
{
	FILE *f;
	int ret;
	int val;

	/* X */
	f = fopen(GYRO_PATH"in_accel_x_raw", "r");
	if (!f)
		return;
	ret = fscanf(f, "%d", &val);
	fclose(f);
	
	if (ret == 1)
		*x = val;

	/* Y */
	f = fopen(GYRO_PATH"in_accel_y_raw", "r");
	if (!f)
		return;
	ret = fscanf(f, "%d", &val);
	fclose(f);
	
	if (ret == 1)
		*y = val;

	/* X */
	f = fopen(GYRO_PATH"in_accel_z_raw", "r");
	if (!f)
		return;
	ret = fscanf(f, "%d", &val);
	fclose(f);

	if (ret == 1)
		*z = val;
}

int main(int argc, char *argv[])
{
	char *fb_name;
	int fb_fd;
	uint8_t *fbp;

	/* Get FB name */
	fb_name = (argc >= 2) ? argv[1] : FB_NAME_DEF;
	/* Open it */
	fb_fd = open(fb_name, O_RDWR);
	if (fb_fd < 0) {
		printf("Could not oped framebuffer device %s !\n", fb_name);
		return EIO;
	}
	/* Screen information is not used so don't read it at all */

	/* Map frame buffer */
	fbp = mmap(0, SCREEN_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);
	/* Save data for global use */
	g_fbp = fbp;

	while (1) {
		static int maxval = 1;
		int x, y, z, tmax;

		get_axes_raw(&x, &y, &z);

		tmax = max(x, max(y, z));
		if (tmax > maxval) maxval = tmax; /* update maximum */

		clear_screen();
		draw_axes();
		draw_pos(x, y, z, maxval);

		usleep(10000);
	}

	/* Close fbdev */
	close(fb_fd);
	return 0;
}