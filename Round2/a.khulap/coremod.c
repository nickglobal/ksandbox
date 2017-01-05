#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/timer.h>


#define DRIVER_AUTHOR	"an1kh"
#define DRIVER_DESC	"LED GPIO driver."
#define DRIVER_NAME	"an1kh-leds-gpio"
#define CLASS_NAME	DRIVER_NAME


/* Forward declarations */
static int gpio_led_probe(struct platform_device *pdev);
static int gpio_led_remove(struct platform_device *pdev);
static ssize_t control_show(struct class *class, struct class_attribute *attr, char *buf);
static ssize_t control_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count);


static const struct of_device_id of_gpio_leds_match[] = {
	{ .compatible = "an1kh,gpio-leds", },
	{}
};

static struct platform_driver gpio_led_driver = {
	.probe	= gpio_led_probe,
	.remove	= gpio_led_remove,
	.driver	= {
		.name   = DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(of_gpio_leds_match),
	},
};


struct gpio_led_data {
	const char *label;
	u32 on_time;
	u32 off_time;
	struct gpio_desc *gpiod;
	struct timer_list timer;
	char is_on;
};

struct gpio_leds_table {
	int			num_leds;
	struct class		*control_class;
	struct class_attribute	class_attr_control;
	struct gpio_led_data	led_data[];
};


static struct gpio_leds_table *gpio_leds_readparams(struct platform_device *pdev)
{
	struct device *dev	= &pdev->dev;
	struct fwnode_handle	*child;
	struct gpio_leds_table	*leds;
	int count;

	/* Read number of LEDs from device tree */
	count = device_get_child_node_count(dev);
	dev_info(dev, "device_get_child_node_count() = %d\n", count);
	if (count <= 0)
		return ERR_PTR(-ENODEV);

	/* Allocate memory for the table */
	leds = devm_kzalloc(dev, sizeof(struct gpio_leds_table) + sizeof(struct gpio_led_data) * count, GFP_KERNEL);
	if (!leds)
		return ERR_PTR(-ENOMEM);

	leds->num_leds = 0;
	device_for_each_child_node(dev, child) {
		struct gpio_led_data *curr_led = &(leds->led_data[leds->num_leds]);

		/* Read properties */
		fwnode_property_read_string(child, "label", &curr_led->label);
		fwnode_property_read_u32(child, "on-time", &curr_led->on_time);
		fwnode_property_read_u32(child, "off-time", &curr_led->off_time);
		curr_led->gpiod = devm_get_gpiod_from_child(dev, NULL, child);
		if (IS_ERR(curr_led->gpiod)) {
			dev_err(dev, "fail devm_get_gpiod_from_child()\n");
			return ERR_PTR(-ENODEV);
		}

		leds->num_leds++;
	}

	return leds;
}


static void gpio_leds_start_interval(struct gpio_led_data *led)
{
	int interval = led->is_on ? led->on_time : led->off_time;

	if (interval) {
		gpiod_direction_output(led->gpiod, led->is_on);
		mod_timer(&(led->timer), jiffies + msecs_to_jiffies(interval));
	}
}

static void gpio_leds_timer_handler(unsigned long param)
{
	struct gpio_led_data *curr_led = (struct gpio_led_data *)param;

	/* Change state and restart */
	curr_led->is_on ^= 1;
	gpio_leds_start_interval(curr_led);
}

static int gpio_leds_configure(struct platform_device *pdev, struct gpio_leds_table *leds)
{
	int i;

	for (i = 0; i < leds->num_leds; i++) {
		struct gpio_led_data *curr_led = &(leds->led_data[i]);

		/* Setup timer */
		setup_timer(&(curr_led->timer), gpio_leds_timer_handler, (unsigned long)curr_led);
		/* Start blink */
		curr_led->is_on = 1;
		gpio_leds_start_interval(curr_led);
	}

	return 0;
}


static int gpio_leds_release(struct platform_device *pdev, struct gpio_leds_table *leds)
{
	int i;

	for (i = 0; i < leds->num_leds; i++) {
		struct gpio_led_data *curr_led = &(leds->led_data[i]);

		/* Stop timer */
		del_timer_sync(&(curr_led->timer));
	}

	return 0;
}


static ssize_t control_show(struct class *class, struct class_attribute *attr, char *buf)
{
	int size, i;
	struct gpio_leds_table *leds = container_of(attr, struct gpio_leds_table, class_attr_control);

	size = sprintf(buf, "Total LEDs = %d\n", leds->num_leds);

	for (i = 0; i < leds->num_leds; i++)
		size += sprintf(buf + size,
			"%d: \"%s\", on-time = %d, off-time = %d\n",
			i, leds->led_data[i].label, leds->led_data[i].on_time, leds->led_data[i].off_time);

	return size;
}

static ssize_t control_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	int num, on_time, off_time;
	int ret;
	struct gpio_leds_table *leds = container_of(attr, struct gpio_leds_table, class_attr_control);

	/* Read parameters */
	ret = sscanf(buf, "%d %d %d", &num, &on_time, &off_time);
	/* Check parameters */
	if (ret != 3)
		return 0;
	if (num >= leds->num_leds)
		return 0;

	leds->led_data[num].on_time = on_time;
	leds->led_data[num].off_time = off_time;

	return count;
}


static int gpio_led_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct gpio_leds_table *leds;

	dev_info(dev, "dev_probe()");

	leds = gpio_leds_readparams(pdev);
	if (IS_ERR(leds))
		return PTR_ERR(leds);

	/* Create sysfs class to control LEDs */
	leds->control_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(leds->control_class))
		return PTR_ERR(leds->control_class);
	/* Fill the attribute structure */
	leds->class_attr_control.attr.name = __stringify(control);
	leds->class_attr_control.attr.mode = VERIFY_OCTAL_PERMISSIONS(0664);
	leds->class_attr_control.show = &control_show;
	leds->class_attr_control.store = &control_store;
	ret = class_create_file(leds->control_class, &(leds->class_attr_control));
	if (IS_ERR_VALUE(ret)) {
		class_remove_file(leds->control_class, &(leds->class_attr_control));
		class_destroy(leds->control_class);
		return ret;
	}

	gpio_leds_configure(pdev, leds);
	platform_set_drvdata(pdev, leds);

	return 0;
}

static int gpio_led_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_leds_table *leds;

	dev_info(dev, "dev_remove()");

	leds = platform_get_drvdata(pdev);

	gpio_leds_release(pdev, leds);
	class_remove_file(leds->control_class, &(leds->class_attr_control));
	class_destroy(leds->control_class);
	return 0;
}


MODULE_DEVICE_TABLE(of, of_gpio_leds_match);
module_platform_driver(gpio_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
