#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>


static uint8_t reverse_bits(uint8_t b)
{
	return ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16; 
}


int main(int argc, char *argv[])
{
	char *fb_name, *bmp_name;
	int fb_fd, bmp_fd;
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	long screensize, bmpsize;
	uint8_t *fbp, *bmpp, *pdata;
	struct stat st;
	uint32_t bmp_data_offset;
	int x, y, xmax, ymax;

	/* Check parameters */
	if (argc != 3) {
		printf("Usage: bmp2fb framebuffer_device bmp_filename\n");
		return EINVAL;
	}
	fb_name = argv[1];
	bmp_name = argv[2];

	/* Open framebuffer */
	fb_fd = open(fb_name, O_RDWR);
	if (fb_fd < 0) {
		printf("Could not oped framebuffer device %s !\n", fb_name);
		return EIO;
	}

	/* Get variable screen information */
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	/* Print some parameters */
	printf("vinfo.bits_per_pixel = %d\n", vinfo.bits_per_pixel);
	printf("vinfo.yres_virtual = %d\n", vinfo.yres_virtual);
	printf("vinfo.xres_virtual = %d\n", vinfo.xres_virtual);
	/* Caluculate screen size */
	screensize = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
	printf("screensize = %d\n", screensize);
	/* Map frame buffer */
	fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);
	
	/* Read  BMP file */
	bmp_fd = open (bmp_name, O_RDONLY);
	if (bmp_fd < 0) {
		printf("Could not oped bmp file %s !\n", bmp_name);
		close(fb_fd);
		return EIO;	
	}
	/* Get bitmap size */
	fstat(bmp_fd, &st);
	bmpsize = st.st_size;
	printf("bmpsize = %d\n", bmpsize);
	/* Allocate memory */
	bmpp = (uint8_t *)malloc(bmpsize);
	if (!bmpp) {
		printf("Could not allocate %d bytes memory for bitmap!\n", bmpsize);
		close(bmp_fd);
		close(fb_fd);
		return ENOMEM;	
	}
	/* Read and close */
	read(bmp_fd, bmpp, bmpsize);
	close (bmp_fd);

	/* Get actual data offset */
	bmp_data_offset = bmpp[10] | (bmpp[11] << 8) | (bmpp[12] << 16) | (bmpp[13] << 24);
	printf("bmp_data_offset = %d\n", bmp_data_offset);
	pdata = bmpp + bmp_data_offset;

	/* Copy data
	   Bitmaps are started in the low left corner */
	xmax = vinfo.xres_virtual / 8;
	ymax = vinfo.yres_virtual;

	for (x = 0; x < xmax; x++) {
		for (y = 0; y < ymax; y++) {
			uint8_t val;

			val = pdata[x + (ymax - y - 1) * xmax];	
			val = reverse_bits(val);
			fbp[x + y * xmax] = val;

		}
	}

	/* Free memory */
	free(bmpp);

	/* Close fbdev */
	close(fb_fd);

	return 0;
}