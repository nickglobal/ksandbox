#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#define DRIVER_AUTHOR "Alina Minska"
#define DRIVER_DESC "Control LEDs"

#define MAX_BUF_LENGTH 256
#define MAX_TOKEN_LENGTH 20
#define DEFAULT_DELAY_ON 1000
#define DEFAULT_DELAY_OFF 1000


struct led_context {
	unsigned int delay_on;
	unsigned int delay_off;
	unsigned int mode;
	struct gpio_led led;
	struct task_struct *ts;
};

struct leds_collection {
	int size;
	struct led_context *leds;
};

enum mode {
	MODE_OFF = 0,
	MODE_ON,
	MODE_BLINK,
	MODE_MAX
};

static struct class *sysfs_class;
static struct leds_collection leds_collection;


static int thread_task(void *data)
{
	struct led_context *d = (struct led_context)(*data);
	int ret;
	int flag = 1;

	while (!kthread_should_stop()) {
		ret = gpiod_direction_output(d->led.gpiod, flag);
		if (ret < 0)
			return ret;
		flag = !flag;
		msleep(flag ? d->delay_off : d->delay_on);
	}
	return 0;
}

int get_gpio_state(struct fwnode_handle *node)
{
	const char *state;

	if (!fwnode_property_read_string(node, "default-state", &state)) {
		if (!strcmp(state, "keep"))
			return LEDS_GPIO_DEFSTATE_KEEP;
		else if (!strcmp(state, "on"))
			return LEDS_GPIO_DEFSTATE_ON;
		else
			return LEDS_GPIO_DEFSTATE_OFF;
	}
	return LEDS_GPIO_DEFSTATE_OFF;
}

void print_led_info(struct gpio_led *led)
{
	printk(KERN_DEBUG"led name: %s\n", led->name);
	printk(KERN_DEBUG"led def state: %d\n", led->default_state);
	printk(KERN_DEBUG"led def trigger: %s\n", led->default_trigger);
}

int prepare_and_request_gpio(struct device *parent,
							 struct fwnode_handle *child,
							 struct gpio_led *led)
{
	int ret = 0;

	led->gpiod = devm_get_gpiod_from_child(parent, NULL, child);
	if (IS_ERR(led->gpiod)) {
		fwnode_handle_put(child);
		ret = PTR_ERR(led->gpiod);
		return ret;
    }

	fwnode_property_read_string(child, "label", &led->name);
	fwnode_property_read_string(child, "linux,default-trigger", &led->default_trigger);
	led->default_state = get_gpio_state(child);

	print_led_info(led);

	return 0;
}


static ssize_t mode_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	int counter;
	int ret;
	int flag;
	struct led_context *cntx;

	if (sscanf(buf, "%u %u", &counter, &flag) != 2) {
		printk("Wrong number of arguments\n");
		return 0;
	}

	if (counter < leds_collection.size && flag < MODE_MAX) {
		cntx = &leds_collection.leds[counter];
		cntx->mode = flag;

		if (flag != MODE_BLINK) { /* 0 == off, 1 == on */
			if (cntx->ts) {
				kthread_stop(cntx->ts);
			}

			ret = gpiod_direction_output(cntx->led.gpiod, cntx->mode);
			if (ret < 0)
				return ret;
		} else /* blink */ {
			cntx->delay_on = DEFAULT_DELAY_ON;
			cntx->delay_off = DEFAULT_DELAY_OFF;
			cntx->ts = kthread_run(thread_task, (void)*cntx, "my_blink");
		}
	}
	return count;
}

static ssize_t mode_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	char str[MAX_BUF_LENGTH];
	int counter;

	memset(str, 0, sizeof(str));
	str[MAX_BUF_LENGTH - 1] = '\0';

	for (counter = 0; counter < leds_collection.size; counter++) {
		char token[MAX_TOKEN_LENGTH];

		memset(token, 0, sizeof(token));
		token[MAX_TOKEN_LENGTH - 1] = '\0';

		sprintf(token, "Led %d: %s\n", counter,
			leds_collection.leds[counter].mode ?
			(leds_collection.leds[counter].mode == 1 ? "on" : "blink")
			: "off");
		strlcat(str, token, sizeof(str) - strlen(str) - 1);
	}
	return snprintf(buf, strlen(str) + 1, "%s\n", str);
}

static ssize_t delays_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	int counter;
	int d_on;
	int d_off;
	struct led_context *cntx;

	if (sscanf(buf, "%u %u %u", &counter, &d_on, &d_off) != 3) {
		printk("Wrong number of arguments\n");
		return 0;
	}

	if (counter < leds_collection.size) {
		cntx = &leds_collection.leds[counter];
		cntx->delay_on = d_on;
		cntx->delay_off = d_off;
	}

	return count;
}

static int delays_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	char str[MAX_BUF_LENGTH];
	int counter;

	memset(str, 0, sizeof(str));
	str[MAX_BUF_LENGTH - 1] = '\0';

	for (counter = 0; counter < leds_collection.size; counter++) {
		char token[MAX_TOKEN_LENGTH];
		memset(token, 0, sizeof(token));
		token[MAX_TOKEN_LENGTH - 1] = '\0';

		sprintf(token, "Led %u: %u %u\n", counter,
			leds_collection.leds[counter].delay_on,
			leds_collection.leds[counter].delay_off);
		strlcat(str, token, sizeof(str) - strlen(str) - 1);
	}
	return snprintf(buf, strlen(str) + 1, "%s\n", str);
}

static CLASS_ATTR_RW(mode);
static CLASS_ATTR_RW(delays);

void sysfs_class_create(void)
{
	int ret;

	sysfs_class = class_create(THIS_MODULE, "minsali_leds");
	ret = class_create_file(sysfs_class, &class_attr_mode);
	ret |= class_create_file(sysfs_class, &class_attr_delays);

	if (ret < 0)
		printk("Failed to create /sys/class/minsali_leds\n");
}

int minsali_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct fwnode_handle *child;
	int count;
	struct gpio_led *led;
	int ret;

	dev = &pdev->dev;

	count = device_get_child_node_count(dev);
	if (!count) {
		printk("failed to find leds in dts\n");
		return -ENODEV;
	}

	printk("found %d leds in dts\n", count);

	sysfs_class_create();

	leds_collection.size = count;
	leds_collection.leds = devm_kzalloc(dev, sizeof(struct led_context)*count, GFP_KERNEL);
	if (!leds_collection.leds)
		return -ENOMEM;
	memset(leds_collection.leds, 0, sizeof(leds_collection.leds));

	count = 0;
	device_for_each_child_node(dev, child) {
		led = &leds_collection.leds[count].led;
		ret = prepare_and_request_gpio(&pdev->dev, child, led);
		if (ret < 0)
			return ret;
		leds_collection.leds[count].mode = led->default_state;
		ret = gpiod_direction_output(led->gpiod, led->default_state);
		if (ret < 0)
			return ret;

		count++;
	}

	return 0;
}

int minsali_remove(struct platform_device *pdev)
{
	int i;

	class_destroy(sysfs_class);

	for (i = 0; i < leds_collection.size; i++) {
		if (leds_collection.leds[i].ts) {
			kthread_stop(leds_collection.leds[i].ts);
		}
	}
	return 0;
}

static const struct of_device_id minsali_devids[] = {
	{.compatible = "gl,minsali"},
	{}
};
MODULE_DEVICE_TABLE(of, minsali_devids);

static struct platform_driver minsali_pd = {
	.probe = minsali_probe,
	.remove = minsali_remove,
	.driver = {
		.name = "minsali",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(minsali_devids),
	}
};
module_platform_driver(minsali_pd)

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
