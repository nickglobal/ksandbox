#define DISP_W (128)
#define DISP_H (64)
#define DISP_BUFSIZE (DISP_H*DISP_W)
#define DISP_NAME ("pkdisp")

struct pkdisp
{
	dev_t dev;
	struct cdev cdev;
	struct class class;
	struct device *device;
	int open_counter;
	char *buf;
    struct i2c_client *client;
};


loff_t  pkdisp_llseek (struct file *, loff_t, int);
ssize_t pkdisp_read   (struct file *, char __user *, size_t, loff_t *);
ssize_t pkdisp_write  (struct file *, const char __user *, size_t, loff_t *);
