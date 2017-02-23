#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <string.h>

#include "font.h"

#define MPU6050_DEVICE_PATH	"/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0"
#define SSD1306_FB_PATH		"/dev/fb0"

/* file structure of the mpu6050 device representation

root@beaglebone:/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0# ls -l
total 0
drwxr-xr-x 2 root root    0 Feb 14 23:01 buffer
-rw-r--r-- 1 root root 4096 Feb 14 23:01 current_timestamp_clock
-r--r--r-- 1 root root 4096 Feb 14 23:01 dev
-r--r--r-- 1 root root 4096 Feb 14 23:01 in_accel_matrix
-r--r--r-- 1 root root 4096 Feb 14 23:01 in_accel_mount_matrix
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_scale
-r--r--r-- 1 root root 4096 Feb 14 23:01 in_accel_scale_available
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_x_calibbias
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_x_raw
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_y_calibbias
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_y_raw
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_z_calibbias
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_accel_z_raw
-r--r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_mount_matrix
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_scale
-r--r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_scale_available
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_x_calibbias
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_x_raw
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_y_calibbias
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_y_raw
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_z_calibbias
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_anglvel_z_raw
-r--r--r-- 1 root root 4096 Feb 14 23:01 in_gyro_matrix
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_temp_offset
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_temp_raw
-rw-r--r-- 1 root root 4096 Feb 14 23:01 in_temp_scale
-r--r--r-- 1 root root 4096 Feb 14 23:01 name
lrwxrwxrwx 1 root root    0 Feb 14 23:01 of_node -> ../../../../../../../firmware/devicetree/base/ocp/i2c@4802a000/glgyro@68
drwxr-xr-x 2 root root    0 Feb 14 23:01 power
-rw-r--r-- 1 root root 4096 Feb 14 23:01 sampling_frequency
-r--r--r-- 1 root root 4096 Feb 14 23:01 sampling_frequency_available
drwxr-xr-x 2 root root    0 Feb 14 23:01 scan_elements
lrwxrwxrwx 1 root root    0 Feb 14 23:01 subsystem -> ../../../../../../../bus/iio
drwxr-xr-x 2 root root    0 Feb 14 23:01 trigger
-rw-r--r-- 1 root root 4096 Feb 14 23:01 uevent

*/

enum {
	MATRIX_SIZE = 3
};

typedef double matrix_t[MATRIX_SIZE][MATRIX_SIZE];

typedef struct {
	double scale;
	matrix_t matr;
	matrix_t mount_matr;
	int x_raw;
	int x_calibbias;
	int y_raw;
	int y_calibbias;
	int z_raw;
	int z_calibbias;
} accel_info_t;

typedef struct {
	double scale;
	matrix_t mount_matr;
	int x_raw;
	int x_calibbias;
	int y_raw;
	int y_calibbias;
	int z_raw;
	int z_calibbias;
} angvel_info_t;

typedef struct {
	double scale;
	int raw;
	int offset;
	double val;
} temp_info_t;


static void mpu6050_temp_read(temp_info_t *temp)
{
	FILE *f = fopen(MPU6050_DEVICE_PATH"/in_temp_scale", "r");
	if (!f) {
		printf("Cannot read file in_temp_scale\n");
		return;
	}

	int n = fscanf(f, "%lf", &temp->scale);
	if (n != 1) {
		printf("Cannot scanf file in_temp_scale\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_temp_offset", "r");
	if (!f) {
		printf("Cannot read file in_temp_offset\n");
		return;
	}

	n = fscanf(f, "%d", &temp->offset);
	if (n != 1) {
		printf("Cannot scanf file in_temp_offset\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_temp_raw", "r");
	if (!f) {
		printf("Cannot read file in_temp_raw\n");
		return;
	}

	n = fscanf(f, "%d", &temp->raw);
	if (n != 1) {
		printf("Cannot scanf file in_temp_raw\n");
		fclose(f);
		return;
	}
	fclose(f);

	temp->val = (temp->raw + temp->offset) * temp->scale;
}


static void mpu6050_accel_read(accel_info_t *accel)
{
	FILE *f;
	int n;

	f = fopen(MPU6050_DEVICE_PATH"/in_accel_scale", "r");
	if (!f) {
		printf("Cannot read file in_accel_scale\n");
		return;
	}

	n = fscanf(f, "%lf", &accel->scale);
	if (n != 1) {
		printf("Cannot scanf file in_accel_scale\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_accel_x_raw", "r");
	if (!f) {
		printf("Cannot read file in_accel_x_raw\n");
		return;
	}

	n = fscanf(f, "%d", &accel->x_raw);
	if (n != 1) {
		printf("Cannot scanf file in_accel_x_raw\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_accel_y_raw", "r");
	if (!f) {
		printf("Cannot read file in_accel_y_raw\n");
		return;
	}

	n = fscanf(f, "%d", &accel->y_raw);
	if (n != 1) {
		printf("Cannot scanf file in_accel_y_raw\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_accel_z_raw", "r");
	if (!f) {
		printf("Cannot read file in_accel_z_raw\n");
		return;
	}

	n = fscanf(f, "%d", &accel->z_raw);
	if (n != 1) {
		printf("Cannot scanf file in_accel_z_raw\n");
		fclose(f);
		return;
	}
	fclose(f);
}


static void mpu6050_angvel_read(angvel_info_t *angvel)
{
	FILE *f;
	int n;

	f = fopen(MPU6050_DEVICE_PATH"/in_anglvel_scale", "r");
	if (!f) {
		printf("Cannot read file in_accel_scale\n");
		return;
	}

	n = fscanf(f, "%lf", &angvel->scale);
	if (n != 1) {
		printf("Cannot scanf file in_accel_scale\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_anglvel_x_raw", "r");
	if (!f) {
		printf("Cannot read file in_anglvel_x_raw\n");
		return;
	}

	n = fscanf(f, "%d", &angvel->x_raw);
	if (n != 1) {
		printf("Cannot scanf file in_anglvel_x_raw\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_anglvel_y_raw", "r");
	if (!f) {
		printf("Cannot read file in_anglvel_y_raw\n");
		return;
	}

	n = fscanf(f, "%d", &angvel->y_raw);
	if (n != 1) {
		printf("Cannot scanf file in_anglvel_y_raw\n");
		fclose(f);
		return;
	}
	fclose(f);


	f = fopen(MPU6050_DEVICE_PATH"/in_anglvel_z_raw", "r");
	if (!f) {
		printf("Cannot read file in_anglvel_z_raw\n");
		return;
	}

	n = fscanf(f, "%d", &angvel->z_raw);
	if (n != 1) {
		printf("Cannot scanf file in_anglvel_z_raw\n");
		fclose(f);
		return;
	}
	fclose(f);
}


enum {
	SCREEN_WIDTH = 128,
	SCREEN_HEIGHT = 64,
	SCREEN_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT / 8, /* 1024 - Screen size in bytes, assuming 1 bit per pixel */
	SCREEN_COLOR_BLACK = 0,
	SCREEN_COLOR_WHITE = 1,
};

static uint8_t g_screen[SCREEN_SIZE];


static void ssd1306_pixel_set(uint8_t *screen, int x, int y, int val)
{
	int lineal_bit = y * SCREEN_WIDTH + x;
	int lineal_byte = lineal_bit / 8;
	int bit_pos = lineal_bit - lineal_byte * 8;

	if (val)
		screen[lineal_byte] |= (1 << bit_pos);
	else
		screen[lineal_byte] &= ~(1 << bit_pos);
}


static void ssd1306_wake_up()
{
	FILE *f = fopen("/sys/class/graphics/fb0/blank", "w");
	if (!f) {
		printf("cannot open blank\n");
		return;
	}

	fputs("0", f);
	fclose(f);


	f = fopen("/sys/class/vtconsole/vtcon1/bind", "w");
	if (!f) {
		printf("cannot open bind\n");
		return;
	}

	fputs("0", f);
	fclose(f);
}

/* Adapted GL functions for font usage from ssd1306 driver */
static char ssd1306_putc_at(uint8_t *screen, TM_FontDef_t* font, char ch, int x, int y, int color) {
    unsigned int i, b, j;

    /* Check available space in LCD */
    if ( SCREEN_WIDTH <= (x + font->FontWidth) || SCREEN_HEIGHT <= (y + font->FontHeight)) {
        return 0;
    }
    /* Go through font */
    for (i = 0; i < font->FontHeight; i++) {
        b = font->data[(ch - 32) * font->FontHeight + i];
        for (j = 0; j < font->FontWidth; j++) {
            if ((b << j) & 0x8000) {
                ssd1306_pixel_set(screen, x + j, y + i, color);
            } else {
                ssd1306_pixel_set(screen, x + j, y + i, !color);
            }
        }
    }

    /* Return character written */
    return ch;
}


static char ssd1306_puts_at(uint8_t *screen, TM_FontDef_t* font, char* str, int x, int y, int color) {
    while (*str) { /* Write character by character */
        if (ssd1306_putc_at(screen, font, *str, x, y, color) != *str) {
            return *str; /* Return error */
        }
        /* Increase pointer */
        x += font->FontWidth;
        str++; /* Increase string pointer */
    }
    return *str; /* Everything OK, zero should be returned */
}

static void ssd1306_degree_sign_draw(uint8_t *screen, int x, int y, int val)
{
	if (x < 0 || y < 0 || x+2 > SCREEN_WIDTH || y+2 > SCREEN_HEIGHT)
		return;

	ssd1306_pixel_set(screen, x+1, y, val);
	ssd1306_pixel_set(screen, x, y+1, val);
	ssd1306_pixel_set(screen, x+2, y+1, val);
	ssd1306_pixel_set(screen, x+1, y+2, val);
}

static void ssd1306_vert_line(uint8_t *screen, int x, int y1, int y2, int val)
{
	int y_min, y_max;
	if (y1 < y2)
		y_min = y1, y_max = y2;
	else
		y_min = y2, y_max = y1;

	for (int y = y_min; y <= y_max; ++y)
		ssd1306_pixel_set(screen, x, y, val);
}





/* temp graph */
enum {
	tg_x_start = 0,
	tg_x_end = SCREEN_WIDTH - 1,
	tg_y_min = 0,
	tg_y_max = 41,
	tg_y_median = (tg_y_max - tg_y_min) / 2,
	tg_buf_size = 16
};

static const double g_temp_min_delta = 0.05;

typedef struct {
	uint8_t x_current;
	double t_avr;
	double t_start;
	double t_min;
	double t_max;
	long double t_sum;
	uint64_t n_measures;
	uint8_t *screen;
	char str[tg_buf_size];
} temp_graph_data_t;

temp_graph_data_t g_temp_graph = {
	.x_current = tg_x_start,
	.t_avr = 0.0,
	.t_start = 0.0,
	.t_min = 0.0,
	.t_max = 0.0,
	.t_sum = 0.0,
	.n_measures = 0,
	.screen = g_screen
};


static void ssd1306_temp_point_set(temp_graph_data_t *tgd, double temp)
{
	int y_current;

	if (tgd->n_measures == 0) {
		tgd->t_start = tgd->t_min = tgd->t_max = temp;
		tgd->t_sum = 0;
	}

	if      (temp < tgd->t_min)
		tgd->t_min = temp;
	else if (temp > tgd->t_max)
		tgd->t_max = temp;

	++ tgd->n_measures;
	tgd->t_sum += temp;
	tgd->t_avr = tgd->t_sum / tgd->n_measures;

	y_current = tg_y_median - (int)((temp - tgd->t_avr)/g_temp_min_delta);
	if (y_current < tg_y_min)
		y_current = tg_y_min;
	else if (y_current > tg_y_max)
		y_current = tg_y_max;

	if (tgd->x_current < tg_x_end) {
		ssd1306_vert_line(tgd->screen, tgd->x_current+1, tg_y_min, tg_y_max, SCREEN_COLOR_WHITE);
		if (tgd->x_current + 1 < tg_x_end)
			ssd1306_vert_line(tgd->screen, tgd->x_current+2, tg_y_min, tg_y_max, SCREEN_COLOR_BLACK);
	}

	ssd1306_vert_line(tgd->screen, tgd->x_current, tg_y_min, tg_y_max , 0);
	ssd1306_pixel_set(tgd->screen, tgd->x_current, y_current, 1);

	if (tgd->x_current == tg_x_end)
		tgd->x_current = tg_x_start;
	else
		++ tgd->x_current;


	int x, y;
	int 	x_font_shift = TM_Font_7x10.FontWidth*8,
		y_font_shift = TM_Font_7x10.FontHeight;

	x = 0, y = tg_y_max+1;
	snprintf(tgd->str, tg_buf_size, "max%.2lf", tgd->t_max);
	ssd1306_puts_at(tgd->screen, &TM_Font_7x10, tgd->str, x, y, 1);
	ssd1306_degree_sign_draw(tgd->screen, x+x_font_shift+1, y, 1);

	x = 0, y += y_font_shift+1;
	snprintf(tgd->str, tg_buf_size, "min%.2lf", tgd->t_min);
	ssd1306_puts_at(tgd->screen, &TM_Font_7x10, tgd->str, x, y, 1);
	ssd1306_degree_sign_draw(tgd->screen, x+x_font_shift+1, y, 1);

	x = SCREEN_WIDTH-1-x_font_shift-4, y = tg_y_max+1;
	snprintf(tgd->str, tg_buf_size, "cur%.2lf", temp);
	ssd1306_puts_at(tgd->screen, &TM_Font_7x10, tgd->str, x, y, 1);
	ssd1306_degree_sign_draw(tgd->screen, x+x_font_shift+1, y, 1);

	x = SCREEN_WIDTH-1-x_font_shift-4, y += y_font_shift+1;
	snprintf(tgd->str, tg_buf_size, "avr%.2lf", tgd->t_avr);
	ssd1306_puts_at(tgd->screen, &TM_Font_7x10, tgd->str, x, y, 1);
	ssd1306_degree_sign_draw(tgd->screen, x+x_font_shift+1, y, 1);
}



static void ssd1306_accell_data_display(accel_info_t *accel, angvel_info_t *angvel)
{
	enum { OUT_STR_SZ = 18 };

	int x, y;
	static char str[OUT_STR_SZ];

	static const char *fmt = "%6s: %+7d";

	x = 4, y = 0;
	snprintf(str, OUT_STR_SZ, fmt, "ACC X", accel->x_raw);
	ssd1306_puts_at(g_screen, &TM_Font_7x10, str, x, y, 1);

	y += TM_Font_7x10.FontHeight;
	snprintf(str, OUT_STR_SZ, fmt, "ACC Y", accel->y_raw);
	ssd1306_puts_at(g_screen, &TM_Font_7x10, str, x, y, 1);

	y += TM_Font_7x10.FontHeight;
	snprintf(str, OUT_STR_SZ, fmt, "ACC Z", accel->z_raw);
	ssd1306_puts_at(g_screen, &TM_Font_7x10, str, x, y, 1);


	y += TM_Font_7x10.FontHeight;
	snprintf(str, OUT_STR_SZ, fmt, "AVEL X", angvel->x_raw);
	ssd1306_puts_at(g_screen, &TM_Font_7x10, str, x, y, 1);

	y += TM_Font_7x10.FontHeight;
	snprintf(str, OUT_STR_SZ, fmt, "AVEL Y", angvel->y_raw);
	ssd1306_puts_at(g_screen, &TM_Font_7x10, str, x, y, 1);

	y += TM_Font_7x10.FontHeight;
	snprintf(str, OUT_STR_SZ, fmt, "AVEL Z", angvel->z_raw);
	ssd1306_puts_at(g_screen, &TM_Font_7x10, str, x, y, 1);
}


enum {
	DISPLAY_MODE_TEMP,
	DISPLAY_MODE_GYRO,
};

volatile static int g_display_mode = DISPLAY_MODE_TEMP;
volatile static int g_display_clear = 0;
sem_t g_sem;

static inline void ssd1306_need_update_set()
{
	sem_post(&g_sem);
}


static void *ssd1306_update_thread(void *arg)
{
	FILE *g_fb = fopen(SSD1306_FB_PATH, "w");
	if (!g_fb)
		return NULL;

	while (1) {
		sem_wait(&g_sem);

		if (g_display_clear) {
			memset(g_screen, 0, SCREEN_SIZE);
			g_display_clear = 0;
		}

		fwrite(g_screen, SCREEN_SIZE, 1, g_fb);
		rewind(g_fb);
	}

	fclose(g_fb);
	return NULL;
}


static void *mpu6050_read_thread(void *arg)
{
	temp_info_t temp;
	accel_info_t accel;
	angvel_info_t angvel;

	while (1) {
		if (g_display_mode == DISPLAY_MODE_TEMP) {
			mpu6050_temp_read(&temp);
			ssd1306_temp_point_set(&g_temp_graph, temp.val);

			ssd1306_need_update_set();
			usleep(200000);

		} else if (g_display_mode == DISPLAY_MODE_GYRO) {
			mpu6050_accel_read(&accel);
			mpu6050_angvel_read(&angvel);

			ssd1306_accell_data_display(&accel, &angvel);

			ssd1306_need_update_set();
			usleep(200000);
		}
	}

	return NULL;
}


static void ssd1306_clscr(uint8_t *screen)
{
	g_display_clear = 1;
}


int main ()
{
	pthread_t tid_screen;
	pthread_t tid_read;

	ssd1306_clscr(g_screen);
	ssd1306_wake_up();
	printf("temp threshold = %lf\n", g_temp_min_delta);

	sem_init(&g_sem, 0, 1);
	sem_wait(&g_sem);

	pthread_create(&tid_screen, NULL, ssd1306_update_thread, NULL);
	pthread_create(&tid_read, NULL, mpu6050_read_thread, NULL);

	static const char *fmt = "Current display mode is '%s'; 't' - for temp, 'g' - for gyro, 'e' - for exit, 'h' - this message\n";
	printf(fmt, g_display_mode == DISPLAY_MODE_TEMP ? "TEMP" : "GYRO");

	while (1) {

		int c = getchar();

		if 	(c == 't') {
			g_display_mode = DISPLAY_MODE_TEMP;
			printf(fmt, g_display_mode == DISPLAY_MODE_TEMP ? "TEMP" : "GYRO");
			ssd1306_clscr(g_screen);
		} else if (c == 'g') {
			g_display_mode = DISPLAY_MODE_GYRO;
			printf(fmt, g_display_mode == DISPLAY_MODE_TEMP ? "TEMP" : "GYRO");
			ssd1306_clscr(g_screen);
		} else if (c == 'h') {
			printf(fmt, g_display_mode == DISPLAY_MODE_TEMP ? "TEMP" : "GYRO");
		} else if (c == 'e')
			break;
	}

	pthread_cancel(tid_screen);
	pthread_cancel(tid_read);
	sem_close(&g_sem);

	return EXIT_SUCCESS;
}
