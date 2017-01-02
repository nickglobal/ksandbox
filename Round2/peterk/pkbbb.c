#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/timer.h>

/* params */
static int time_on  = 500; /* time LED ON in ms */
static int time_off = 500; /* time LED ON in ms */
module_param_named(on,  time_on,  int, S_IRUGO);
module_param_named(off, time_off, int, S_IRUGO);

/* GPIO stuff */
static struct gpio_desc *gpiod;

/* Timer stuff */
static void blink(unsigned long param);
static struct timer_list pk_timer = TIMER_INITIALIZER(blink, 1000, 0);
static void blink(unsigned long param)
{
	int val = gpiod_get_value(gpiod);

	gpiod_set_value(gpiod, !val);
	mod_timer(&pk_timer, val ?
			jiffies + time_off * HZ / 1000 :
			jiffies + time_on * HZ / 1000);
}

/* set LED mode:
 *  0 - off
 *  1 - on
 *  2 - blink according time_on, time_off
 */
void set_pk_led(int mode)
{
	if (timer_pending(&pk_timer))
		del_timer_sync(&pk_timer);
	if ((mode == 0) || (time_on <= 0)) {
		pr_info("pkbbb: set led off.\n");
		gpiod_set_value(gpiod, 0);
	} else if ((mode == 1) || (time_off <= 0)) {
		pr_info("pkbbb: set led off.\n");
		gpiod_set_value(gpiod, 1);
	} else {
		/* blink */
		pr_info("pkbbb: set led blink: %d ms ON, %d ms off\n",
				time_on, time_off);
		gpiod_set_value(gpiod, 1);
		pk_timer.expires = jiffies + time_off * HZ / 1000;
		add_timer(&pk_timer);
	}
}

/* sysfs entry stuff */

/* sysfs: state */
static ssize_t led_show_state(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (timer_pending(&pk_timer)) {
		pr_info("pkbbb: read: led is blinking\n");
		sprintf(buf, "2");
	} else if (gpiod_get_value(gpiod)) {
		pr_info("pkbbb: read: led is ON\n");
		sprintf(buf, "1");
	} else {
		pr_info("pkbbb: read: led is OFF\n");
		sprintf(buf, "0");
	}

	return strlen(buf);
}

static ssize_t led_store_state(struct class *class,
		struct class_attribute *attr, const char *buf,
		size_t count)
{
	int mode = 0;

	if (count == 0)
		set_pk_led(0);
	else {
		if (kstrtoint (buf, 10, &mode))
			return -EINVAL;
		set_pk_led(mode);
	}
	return count;
}

/* sysfs: on */
static ssize_t led_show_on(struct class *class,
		struct class_attribute *attr, char *buf)
{
	sprintf(buf, "%d", time_on);
	return strlen(buf);
}
static ssize_t led_store_on(struct class *class,
		struct class_attribute *attr, const char *buf,
		size_t count)
{
	if (count) {
		if (kstrtoint (buf, 10, &time_on))
			return -EINVAL;
	} else
		time_on = 0;
	set_pk_led(2);
	return count;
}

/* sysfs: off */
static ssize_t led_show_off(struct class *class,
		struct class_attribute *attr, char *buf)
{
	sprintf(buf, "%d", time_off);
	return strlen(buf);
}
static ssize_t led_store_off(struct class *class,
		struct class_attribute *attr, const char *buf,
		size_t count)
{
	if (count) {
		if (kstrtoint (buf, 10, &time_off))
			return -EINVAL;
	} else
		time_off = 0;
	set_pk_led(2);
	return count;
}

static struct class *class_led;
struct class_attribute class_attr_led_state = __ATTR(state, 0664,
		led_show_state, led_store_state);
struct class_attribute class_attr_led_on = __ATTR(on, 0664,
		led_show_on, led_store_on);
struct class_attribute class_attr_led_off = __ATTR(off, 0664,
		led_show_off, led_store_off);

/* device tree stuff */
static const struct of_device_id pkbbb_of_match[] = {
	{ .compatible = "pk,echo-name", },
	{}
};

MODULE_DEVICE_TABLE(of, pkbbb_of_match);

static int pkbbb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	int err;
	struct fwnode_handle *child;
	const char *name = "pk_led";

	match = of_match_device(of_match_ptr(pkbbb_of_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	/* init LED */
	device_for_each_child_node(dev, child) {
		gpiod = devm_get_gpiod_from_child(dev, NULL, child);
		if (IS_ERR(gpiod)) {
			dev_err(dev, "fail devm_get_gpiod_from_child()\n");
			return PTR_ERR(gpiod);
		}

		if (fwnode_property_present(child, "label"))
			fwnode_property_read_string(child, "label",	&name);

		dev_info(dev, "Register \"%s\" LED\n", name);

		err = gpiod_direction_output(gpiod, 1);
		if (err < 0) {
			dev_err(dev, "fail gpiod_direction_output()\n");
			return err;
		}

		class_led = class_create(THIS_MODULE, name);
		if (IS_ERR(class_led)) {
			dev_err(dev, "fail create class pk_led\n");
			return PTR_ERR(class_led);
		}

		class_attr_led_state.attr.mode = 0666;
		err = class_create_file(class_led, &class_attr_led_state);
		if (err < 0)
			dev_err(dev, "fail create class file state\n");
		class_attr_led_on.attr.mode = 0666;
		err = class_create_file(class_led, &class_attr_led_on);
		if (err < 0)
			dev_err(dev, "fail create class file state\n");
		class_attr_led_off.attr.mode = 0666;
		err = class_create_file(class_led, &class_attr_led_off);
		if (err < 0)
			dev_err(dev, "fail create class file state\n");

		set_pk_led(2);
	}

	return 0;
}

static int pkbbb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "remove...\n");
	class_remove_file(class_led, &class_attr_led_state);
	class_destroy(class_led);
	if (timer_pending(&pk_timer))
		del_timer_sync(&pk_timer);
	return 0;
}

static struct platform_driver pkbbb_driver = {
	.probe		= pkbbb_probe,
	.remove		= pkbbb_remove,
	.driver		= {
		.name	= "pkbbb",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(pkbbb_of_match),
	},
};

static int __init mod_init(void)
{
	return platform_driver_register(&pkbbb_driver);
}
module_init(mod_init);

static void __exit mod_exit(void)
{
	platform_driver_unregister(&pkbbb_driver);
}
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Kulakov");
MODULE_DESCRIPTION("A simple Linux driver");

