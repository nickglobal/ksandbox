
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/stat.h>
#include <linux/fs.h>

#define DRIVER_NAME			"x_led_drv"


static struct device *dev;

static const struct of_device_id my_drvr_match[];


#define LED_MODE_OFF		0
#define LED_MODE_ON			1
#define LED_MODE_BLINK		2


struct x_led_data {
	struct class *led_class;
	struct gpio_desc *gpiod;
	u32 time_on;
	u32 time_off;
	u8	mode;
};

struct gpio_leds_priv {
	int num_leds;
	struct x_led_data leds[];
};

static struct gpio_leds_priv *priv;


static inline int sizeof_gpio_leds_priv(int num_leds)
{
	return sizeof(struct gpio_leds_priv) +
	(sizeof(struct x_led_data) * num_leds);
}


static struct gpio_desc *get_gpiod(struct class *class)
{
	struct gpio_desc *gpiod;
	u32 i;

	for (i = 0; i < priv->num_leds; i++) {
		if (priv->leds[i].led_class == class)
			gpiod = priv->leds[i].gpiod;
	}

	return gpiod;
}



static ssize_t sys_led_on(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;

	i += sprintf(buf, "LED is ON\n");

	dev_info(dev, "LED is ON\n");
	gpiod_set_value(get_gpiod(class), 1);
	return i;
}

static ssize_t sys_led_off(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;

	i += sprintf(buf, "LED is OFF\n");

	dev_info(dev, "LED is OFF\n");
	gpiod_set_value(get_gpiod(class), 0);
	return i;
}


/* <linux/device.h>
* #define CLASS_ATTR(_name, _mode, _show, _store) \
* struct class_attribute class_attr_##_name = __ATTR(_name, _mode,
* _show, _store)
*/
CLASS_ATTR(myled_on, 0664, &sys_led_on, NULL);
CLASS_ATTR(myled_off, 0664, &sys_led_off, NULL);


static void get_platform_info(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct fwnode_handle *child;
	const char *name;
	const char *state;
	struct gpio_desc *gpiod;
	struct class *led_class;

	int res;
	u32 i = 0;

	if (np) {

		int childcount = of_get_child_count(np);
		dev_info(dev, "childcount = %d\n", childcount);

		if (!of_property_read_string(np, "my_name", &name))
			dev_info(dev, "my name is %s\n", name);

		priv = devm_kzalloc(dev, sizeof_gpio_leds_priv(childcount), GFP_KERNEL);
		if (!priv) {
			return;/* ERR_PTR(-ENOMEM);*/
		}
		priv->num_leds = childcount;

		device_for_each_child_node(dev, child) {
		if (fwnode_property_present(child, "label"))
			fwnode_property_read_string(child, "label",	&name);

			gpiod = devm_get_gpiod_from_child(dev, NULL, child);

			if (IS_ERR(gpiod)) {
				dev_err(dev, "fail devm_get_gpiod_from_child()\n");
				return;/* ERR_PTR(gpiod);*/
			}
			gpiod_direction_output(gpiod, 1);

			if (fwnode_property_present(child, "default-state")){
				fwnode_property_read_string(child, "default-state",	&state);

				if (!strcmp(state, "on")){
					priv->leds[i].mode = LED_MODE_ON;
					gpiod_set_value(gpiod, 1);
				}
				else if (!strcmp(state, "blink")) {
					gpiod_set_value(gpiod, 0);
					priv->leds[i].mode = LED_MODE_BLINK;
				}
				else {
					gpiod_set_value(gpiod, 0);
					priv->leds[i].mode = LED_MODE_OFF;
				}
			}

			led_class = class_create(THIS_MODULE, name);

			if (IS_ERR(led_class))
				dev_info(dev, "bad class create\n");

			res = class_create_file(led_class, &class_attr_myled_on);
			res = class_create_file(led_class, &class_attr_myled_off);

			priv->leds[i].led_class = led_class;

			priv->leds[i].gpiod = gpiod;
			i++;
		}
	}
}







static int x_led_init(struct platform_device *pdev)
{
	const struct of_device_id *match;

	dev = &pdev->dev;

	match = of_match_device(of_match_ptr(my_drvr_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	dev_info(dev, "Hello, world!\n");

	get_platform_info(pdev);


	dev_info(dev, "'x_led_drv' module initialized\n");
	return 0;
}



static int x_led_exit(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct class *led_class;
	u32 i = 0;

	for (i = 0; i < priv->num_leds; i++) {
		led_class = priv->leds[i].led_class;

		class_remove_file(led_class, &class_attr_myled_on);
		class_remove_file(led_class, &class_attr_myled_off);
		class_destroy(led_class);
	}

	dev_info(dev, "Goodbye, world!\n");
	return 0;
}




static const struct of_device_id my_drvr_match[] = {
	{ .compatible = "DAndy,coremod_gpio_leds", },
	{ },
};
MODULE_DEVICE_TABLE(of, my_drvr_match);


static struct platform_driver my_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = my_drvr_match,
	},
	.probe = x_led_init,
	.remove = x_led_exit,
};
module_platform_driver(my_driver);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Devyatov Andrey <devyatov.andey@litech.com>");
