#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include "font.h"

#define MPU_DEVICE_PATH "/sys/devices/platform/ocp/4802a000.i2c/i2c-1/1-0068/iio:device0"

//byte_t

/*

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


void read_field_from_file(const char * fname, void *data)
{

}


void read_mpu6050_temp(temp_info_t *temp)
{
	FILE *f = fopen(MPU_DEVICE_PATH"/in_temp_scale", "r");
	if (!f) {
		printf("Cannot read file in_temp_scale\n");
		return;
	}

	int n = fscanf(f, "%lf", &temp->scale);
	if (n != 1) {
		printf("Cannot scanf file in_temp_scale\n");
		return;
	}
	//printf("in_temp_scale = %f\n", temp->scale);
	fclose(f);


	f = fopen(MPU_DEVICE_PATH"/in_temp_offset", "r");
	if (!f) {
		printf("Cannot read file in_temp_offset\n");
		return;
	}

	n = fscanf(f, "%d", &temp->offset);
	if (n != 1) {
		printf("Cannot scanf file in_temp_offset\n");
		return;
	}
	//printf("in_temp_offset = %d\n", temp->offset);
	fclose(f);


	f = fopen(MPU_DEVICE_PATH"/in_temp_raw", "r");
	if (!f) {
		printf("Cannot read file in_temp_raw\n");
		return;
	}

	n = fscanf(f, "%d", &temp->raw);
	if (n != 1) {
		printf("Cannot scanf file in_temp_raw\n");
		return;
	}
	//printf("in_temp_raw = %d\n", temp->raw);
	fclose(f);

	temp->val = (temp->raw + temp->offset) * temp->scale;
	//printf("temp = %lf\n", temp->val);

}


void read_mpu6050 (temp_info_t *temp, accel_info_t *accel, angvel_info_t* angvel)
{
	/*
	FILE *f = fopen(MPU_DEVICE_PATH"/in_accel_scale");
	if (!f) {
		printf("Cannot read file in_accel_scale\n");
		return;
	}

	fscanf(f, "%lf", );*/
	read_mpu6050_temp(temp);
}


enum {
	SCREEN_WIDTH = 128,
	SCREEN_HEIGHT = 64,
	SCREEN_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT / 8, /* 1024 - Screen size in bytes, assuming 1 bit per pixel */
};

uint8_t screen[SCREEN_SIZE];


void set_ssd1306_pixel(uint8_t *screen, int x, int y, int val)
{
	int lineal_bit = y * SCREEN_WIDTH + x;
	int lineal_byte = lineal_bit / 8;
	int bit_pos = lineal_bit - lineal_byte * 8;

	if (val)
		screen[lineal_byte] |= (1 << bit_pos);
	else
		screen[lineal_byte] &= ~(1 << bit_pos);
}


void set_ssd1306_line(uint8_t *screen, int x1, int y1, int x2, int y2, int val)
{
	int xs, xe, ys, ye;

	if (x1 == x2) {
		xs = xe = x1;
		if (y1 == y2) {
			set_ssd1306_pixel(screen, x1, y1, val);
			return;
		} else if (y1 < y2) {
			xs = y1, ye = y2;
		} else {
			xs = y2, ye = y1;
		}

		for(int y = ys; y <= ye; ++y) {
			set_ssd1306_pixel(screen, xs, y, val);
		}
		return;

	} else if (x1 < x2) {
		xs = x1, xe = x2;
	} else {
		xs = x2, xe = x1;
	}

	for (int x = xs; x <= xe; ++ x) {
		int y = ((x2*y1 - x1*y2) - (y1 - y2)*x)/(x2-x1);
		if (y >= 0 && y < SCREEN_WIDTH)
			set_ssd1306_pixel(screen, x, y, val);
	}
}

#if 0
void read_ssd1306(uint8_t *screen)
{
	FILE *f = fopen("/dev/fb0", "r");
	//int n = ;
	fread(screen, SCREEN_SIZE, 1, f);
	//printf("read %d bytes\n", n);
	//set_ssd1306_pixel(current_screen, 0, 0, 1);
	fclose(f);
}


void write_ssd1306(uint8_t *screen)
{
/*
	for (int i = 0; i < SCREEN_SIZE; ++ i) {
			screen[i] = 0xFF;
	}
	screen[0] = screen[1] = screen[2] = screen[3] =
	screen[SCREEN_SIZE-4] = screen[SCREEN_SIZE-3] = screen[SCREEN_SIZE-2] = screen[SCREEN_SIZE-1] = 0x00;

	set_ssd1306_pixel(screen, 0, 0, 1);
	set_ssd1306_pixel(screen, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, 1);

	set_ssd1306_pixel(screen, 64, 31, 0);
	set_ssd1306_pixel(screen, 64, 32, 0);
	set_ssd1306_pixel(screen, 64, 33, 0);

	set_ssd1306_pixel(screen, 63, 32, 0);
	set_ssd1306_pixel(screen, 65, 32, 0);

	set_ssd1306_line(screen, 10, 10, 120, 29, 0);
*/
	FILE *f = fopen("/dev/fb0", "w");
	//int n = ;
	fwrite(screen, SCREEN_SIZE, 1, f);
	//printf("written %d bytes\n", n);
	fclose(f);

}
#endif


void wake_up_ssd1306()
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


/* Private SSD1306 structure */
struct dpoz{
    u16 CurrentX;
    u16 CurrentY;
    u8  Inverted;
    u8  Initialized;
} g_poz = { .CurrentX = 0, .CurrentY = 0 };


void set_pos_ssd1306(int x, int y)
{
	g_poz.CurrentX = x;
	g_poz.CurrentY = y;
}


char putc_ssd1306(uint8_t *screen, char ch, TM_FontDef_t* Font, int color) {
    unsigned int i, b, j;
    struct dpoz *poz;
    poz = &g_poz;

    /* Check available space in LCD */
    if ( SCREEN_WIDTH <= (poz->CurrentX + Font->FontWidth) || SCREEN_HEIGHT <= (poz->CurrentY + Font->FontHeight)) {
        return 0;
    }
    /* Go through font */
    for (i = 0; i < Font->FontHeight; i++) {
        b = Font->data[(ch - 32) * Font->FontHeight + i];
        for (j = 0; j < Font->FontWidth; j++) {
            if ((b << j) & 0x8000) {
                set_ssd1306_pixel(screen, poz->CurrentX + j, poz->CurrentY + i, color);
            } else {
                set_ssd1306_pixel(screen, poz->CurrentX + j, poz->CurrentY + i, !color);
            }
        }
    }
    /* Increase pointer */
    poz->CurrentX += Font->FontWidth;
    /* Return character written */
    return ch;
}


char puts_ssd1306(uint8_t *screen, char* str, TM_FontDef_t* Font, int color) {
    while (*str) { /* Write character by character */
        if (putc_ssd1306(screen, *str, Font, color) != *str) {
            return *str; /* Return error */
        }
        str++; /* Increase string pointer */
    }
    return *str; /* Everything OK, zero should be returned */
}


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
	char st[tg_buf_size];
} temp_graph_data_t;

temp_graph_data_t g_temp_graph = {
	.x_current = tg_x_start,
	.t_avr = 0.0,
	.t_start = 0.0,
	.t_min = 0.0,
	.t_max = 0.0,
	.t_sum = 0.0,
	.n_measures = 0,
	.screen = screen
};

void screen_draw_degree_sign(uint8_t *screen, int x, int y, int val)
{
	if (x < 0 || y < 0 || x+2 > SCREEN_WIDTH || y+2 > SCREEN_HEIGHT)
		return;

	set_ssd1306_pixel(screen, x+1, y, val);
	set_ssd1306_pixel(screen, x, y+1, val);
	set_ssd1306_pixel(screen, x+2, y+1, val);
	set_ssd1306_pixel(screen, x+1, y+2, val);
}

static void ssd1306_vert_line(uint8_t *screen, int x, int y1, int y2, int val)
{
	int y_min, y_max;
	if (y1 < y2)
		y_min = y1, y_max = y2;
	else
		y_min = y2, y_max = y1;

	for (int y = y_min; y <= y_max; ++y)
		set_ssd1306_pixel(screen, x, y, val);
}

void set_temp_point_ssd1306(temp_graph_data_t *tgd, double temp)
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

	if(tgd->x_current < tg_x_end) {
		ssd1306_vert_line(tgd->screen, tgd->x_current+1, tg_y_min, tg_y_max, 0);
		if (tgd->x_current + 1 < tg_x_end)
			ssd1306_vert_line(tgd->screen, tgd->x_current+2, tg_y_min, tg_y_max, 0);
	}

	ssd1306_vert_line(tgd->screen, tgd->x_current, tg_y_min, tg_y_max , 0);
	set_ssd1306_pixel(tgd->screen, tgd->x_current, y_current, 1);

	if (tgd->x_current == tg_x_end)
		tgd->x_current = tg_x_start;
	else
		++ tgd->x_current;


	int x, y;
	const int x_font_shift = 7*8, y_font_shift = 10;

	x = 0, y = tg_y_max+1;
	set_pos_ssd1306(x, y);
	snprintf(tgd->st, tg_buf_size, "max%.2lf", tgd->t_max);
	puts_ssd1306(tgd->screen, tgd->st, &TM_Font_7x10, 1);
	screen_draw_degree_sign(tgd->screen, x+x_font_shift+1, y, 1);

	x = 0, y += y_font_shift+1;
	set_pos_ssd1306(x, y);
	snprintf(tgd->st, tg_buf_size, "min%.2lf", tgd->t_min);
	puts_ssd1306(tgd->screen, tgd->st, &TM_Font_7x10, 1);
	screen_draw_degree_sign(tgd->screen, x+x_font_shift+1, y, 1);

	x = SCREEN_WIDTH-1-x_font_shift-4, y = tg_y_max+1;
	set_pos_ssd1306(x, y);
	snprintf(tgd->st, tg_buf_size, "cur%.2lf", temp);
	puts_ssd1306(tgd->screen, tgd->st, &TM_Font_7x10, 1);
	screen_draw_degree_sign(tgd->screen, x+x_font_shift+1, y, 1);

	x = SCREEN_WIDTH-1-x_font_shift-4, y += y_font_shift+1;
	set_pos_ssd1306(x, y);
	snprintf(tgd->st, tg_buf_size, "avr%.2lf", tgd->t_avr);
	puts_ssd1306(tgd->screen, tgd->st, &TM_Font_7x10, 1);
	screen_draw_degree_sign(tgd->screen, x+x_font_shift+1, y, 1);
}

#if 0
enum {
	BUF_SIZE = 64
};


void
test_file_read()
{
	int fd;
	ssize_t n;
	int nr;
	int temp_raw;
	int rc;
	char buf[BUF_SIZE];

	if ((fd = open(MPU_DEVICE_PATH"/in_temp_raw", O_RDONLY)) == -1 ) {
		printf("cannot 'open' file\n");
		return;
	}
	printf("file was opened...\n");

	fd_set fset;
	FD_ZERO(&fset);
	FD_SET(fd, &fset);

	struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };


	while(1) {
		printf("selecting...\n");
		rc = select(1, &fset, NULL, NULL, &tv);
		if (rc == -1) {
			printf("error in select\n");
			return;
		}
		printf("operating...\n");
/*
		void FD_CLR(int fd, fd_set *set);
		int  FD_ISSET(int fd, fd_set *set);
*/




		n = read(fd, buf, BUF_SIZE);
		if (n == -1) {
			printf("cannot 'read' file\n");
			return;
		}
		lseek(fd, 0, SEEK_SET);
		printf("read %d bytes\n", n);
		if (n > BUF_SIZE - 1) {
			printf("too much data\n");
			return;
		}
		if (n == 0) {
			continue;
		}
		buf[n] = '\0';

		nr = sscanf(buf, "%d", &temp_raw);
		/*
		if (nr != 1) {
			printf("cannot parse data\n");
			return;
		}
		*/

		printf("raw_temp %d\n", temp_raw);
	}


	close(fd);
}
#endif


static FILE *g_fb;
volatile static int g_display_mode;

/*
static inline void close_files(void)
{
	printf("closing files...\n");
	fclose(g_fb);
}
*/
sem_t g_sem;

static inline void screen_need_update_set()
{
	sem_post(&g_sem);
}

static void *screen_update_thread(void *arg)
{
	g_fb = fopen("/dev/fb0", "w");
	if (!g_fb)
		return NULL;

	while (1) {
		sem_wait(&g_sem);

		fwrite(screen, SCREEN_SIZE, 1, g_fb);
		rewind(g_fb);
	}

	fclose(g_fb);
	return NULL;
}


enum {
	DISPLAY_MODE_TEMP,
	DISPLAY_MODE_GYRO
};


static void *mpu6050_read_thread(void *arg)
{
	uint64_t c = 0;
	temp_info_t temp;

	while (1) {
		if (g_display_mode == DISPLAY_MODE_TEMP) {
			read_mpu6050_temp(&temp);
			set_temp_point_ssd1306(&g_temp_graph, temp.val);

			if (c % 1000 == 0)
				printf("%llu measurements done\n", c);
			++ c;
			screen_need_update_set();
		} else if (g_display_mode == DISPLAY_MODE_GYRO) {

		}

		usleep(200000);
	}

}


int main ()
{
	pthread_t tid_screen;
	pthread_t tid_gyro;

	//float prev_temp, e_temp = 0.05f;


	//int display_mode = DISPLAY_MODE_TEMP;

	//accel_info_t accel; //prev_accel,
	//angvel_info_t angvel; //prev_angvel,

	//printf("%u\n", sizeof (matrix_t));

	//atexit(close_files);

	wake_up_ssd1306();
	//set_ssd1306_line(screen, tg_x_start, tg_y_max+1, tg_x_end, tg_y_max+1, 1);

	//set_ssd1306_line(screen, 0, 40, 100, 50, 1);
	//set_ssd1306_line(screen, 0, 40, 0, 50, 1);

	//test_file_read();

	//read_mpu6050(&temp, &accel, &angvel);
	//prev_temp = temp.val;
	printf("temp threshold = %f\n", g_temp_min_delta);

	sem_init(&g_sem, 0, 1);
	sem_wait(&g_sem);

	pthread_create(&tid_screen, NULL, screen_update_thread, NULL);
	pthread_create(&tid_gyro, NULL, mpu6050_read_thread, NULL);

	while (1) {
		printf("Current display mode is '%s'; 't' - for temp, 'g' - for gyro, 'e' - for exit\n",
				g_display_mode == DISPLAY_MODE_TEMP ? "TEMP" : "GYRO");
		int c = getchar();
		printf("%c", c);

		if 	(c == 't')
			g_display_mode = DISPLAY_MODE_TEMP;
		else if (c == 'g')
			g_display_mode = DISPLAY_MODE_GYRO;
		else if (c == 'e')
			break;

	}

	pthread_cancel(tid_screen);
	pthread_cancel(tid_gyro);
	sem_close(&g_sem);

	return EXIT_SUCCESS;
}
