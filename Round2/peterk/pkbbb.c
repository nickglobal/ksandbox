#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>

/* GPIO stuff */
static struct gpio_desc *gpiod;

/* sysfs entry stuff */
static ssize_t led_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	if (gpiod_get_value(gpiod)) {
		pr_info("pkbbb: read: led is ON\n");
		sprintf(buf, "1");
	} else {
		pr_info("pkbbb: read: led is OFF\n");
		sprintf(buf, "0");
	}

	return strlen(buf);
}
static ssize_t led_store(struct class *class,
		struct class_attribute *attr, const char *buf,
		size_t count)
{
	if ((count == 0) || ('0' == *buf)) {
		pr_info("pkbbb: set LED off\n");
		gpiod_set_value(gpiod, 0);
	} else {
		pr_info("pkbbb: set LED on\n");
		gpiod_set_value(gpiod, 1);
	}
	return count;
}

static struct class *class_led;
struct class_attribute class_attr_led = __ATTR(led0, 0664,
		led_show, led_store);

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

		if (fwnode_property_present(child, "label")) {
			fwnode_property_read_string(child, "label",
					&class_attr_led.attr.name);
		}

		dev_info(dev, "Register \"%s\" LED\n",
				class_attr_led.attr.name);

		err = gpiod_direction_output(gpiod, 1);
		if (err < 0) {
			dev_err(dev, "fail gpiod_direction_output()\n");
			return err;
		}

		class_led = class_create(THIS_MODULE, "pk_led");
		if (IS_ERR(class_led)) {
			dev_err(dev, "fail create class pk_led\n");
			return PTR_ERR(class_led);
		}

		class_attr_led.attr.mode = 0666;
		err = class_create_file(class_led, &class_attr_led);
		if (err < 0) {
			dev_err(dev, "fail create class file %s\n",
					class_attr_led.attr.name);
			goto err;
		}
	}

	return 0;
err:
	class_destroy(class_led);
	return err;
}

static int pkbbb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "remove...\n");
	class_remove_file(class_led, &class_attr_led);
	class_destroy(class_led);
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

