#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/timer.h>
#include <linux/spinlock.h>

struct pk_led {
	unsigned int on;
	unsigned int off;
	const char *name;
	struct timer_list timer;
	struct class class_led;
	struct gpio_desc *gpiod;
	spinlock_t lock;
};

static int num_leds;
static struct pk_led *leds;

/**
 * led_logic - controlls LED state
 * @my_led: pointer to led data structure
 *
 * Determines if LED should be set to permanently ON or OFF
 * (if corresponding on and off parameters equal 0
 * Rise a timer if LED is in blink mode
 *
 * RETURNS:
 * new state of LED
 */
void led_logic(struct pk_led *my_led)
{
	unsigned long exp;
	int val;

	val = gpiod_get_value(my_led->gpiod);

	/* kill timer if we are in 'always on' or 'always off' mode */
	if (my_led->on & my_led->off & timer_pending(&my_led->timer))
		del_timer_sync(&my_led->timer);

	/* find out new state */
	if (my_led->on <= 0) {
		/* off */
		val = 0;
	} else if (my_led->off <= 0) {
		/* on */
		val = 1;
	} else {
		/* blink: rise timer and invert state */
		exp = val ?
				jiffies + my_led->off * HZ / 1000 :
				jiffies + my_led->on  * HZ / 1000;
		if (timer_pending(&my_led->timer))
			mod_timer(&my_led->timer, exp); /* continue blinking */
		else {
			/* timer expired, restart it */
			my_led->timer.expires = exp;
			add_timer(&my_led->timer);
		}
		val = !val;
	}
	gpiod_set_value(my_led->gpiod, val);
}

/* Timer stuff */
static void blink(unsigned long param)
{
	struct pk_led *my_led = (struct pk_led *)param;

	spin_lock_bh(&my_led->lock);
	led_logic(my_led);
	spin_unlock_bh(&my_led->lock);
}

/* sysfs entry stuff */

/* sysfs: on */
static ssize_t led_show_on(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct pk_led *my_led = container_of(class, struct pk_led, class_led);

	spin_lock_bh(&my_led->lock);
	sprintf(buf, "%d", my_led->on);
	spin_unlock_bh(&my_led->lock);
	return strlen(buf);
}
static ssize_t led_store_on(struct class *class,
		struct class_attribute *attr, const char *buf,
		size_t count)
{
	struct pk_led *my_led = container_of(class, struct pk_led, class_led);

	spin_lock_bh(&my_led->lock);
	if (count) {
		if (kstrtoint (buf, 10, &my_led->on))
			return -EINVAL;
	} else
		my_led->on = 0;

	led_logic(my_led);
	spin_unlock_bh(&my_led->lock);

	return count;
}

/* sysfs: off */
static ssize_t led_show_off(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct pk_led *my_led = container_of(class, struct pk_led, class_led);

	spin_lock_bh(&my_led->lock);
	sprintf(buf, "%d", my_led->off);
	spin_unlock_bh(&my_led->lock);
	return strlen(buf);
}
static ssize_t led_store_off(struct class *class,
		struct class_attribute *attr, const char *buf,
		size_t count)
{
	struct pk_led *my_led = container_of(class, struct pk_led, class_led);

	spin_lock_bh(&my_led->lock);
	if (count) {
		if (kstrtoint (buf, 10, &my_led->off))
			return -EINVAL;
	} else
		my_led->off = 0;

	led_logic(my_led);
	spin_unlock_bh(&my_led->lock);

	return count;
}

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
	int res = 0;
	struct fwnode_handle *child;
	int i;

	match = of_match_device(of_match_ptr(pkbbb_of_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	num_leds = device_get_child_node_count(dev);
	if (!num_leds)
		return -ENODEV;

	leds = devm_kzalloc(dev, sizeof(struct pk_led)*num_leds, GFP_KERNEL);
	if (!leds)
		return -ENOMEM;

	dev_info(dev, "Register %d LEDs:\n", num_leds);

	/* init LEDs */
	i = -1;
	device_for_each_child_node(dev, child) {
		i++;
		/* defaults permanently ON */
		leds[i].on = 1000;
		leds[i].off = 0;
		leds[i].name = "pk_led";

		leds[i].gpiod = devm_get_gpiod_from_child(dev, NULL, child);
		if (IS_ERR(leds[i].gpiod)) {
			dev_err(dev, "fail devm_get_gpiod_from_child()\n");
			res = PTR_ERR(leds[i].gpiod);
			continue;
		}

		if (fwnode_property_present(child, "label"))
			fwnode_property_read_string(child, "label",
					&leds[i].name);

		if (fwnode_property_present(child, "on"))
			fwnode_property_read_u32(child, "on", &leds[i].on);

		if (fwnode_property_present(child, "off"))
			fwnode_property_read_u32(child, "off", &leds[i].off);


		dev_info(dev, "Register #%d \"%s\" LED with params: on %u, off %u\n",
				i, leds[i].name, leds[i].on, leds[i].off);

		/* config sysfs entries */
		leds[i].class_led.name = leds[i].name;
		leds[i].class_led.owner = THIS_MODULE;
		res = class_register(&leds[i].class_led);
		if (res) {
			dev_err(dev, "fail register class\n");
			continue;
		}

		class_attr_led_on.attr.mode = 0666;
		res = class_create_file(&leds[i].class_led, &class_attr_led_on);
		if (res < 0)
			dev_err(dev, "fail create class file state\n");
		class_attr_led_off.attr.mode = 0666;
		res = class_create_file(&leds[i].class_led,
				&class_attr_led_off);
		if (res < 0)
			dev_err(dev, "fail create class file state\n");

		/* Spinlock */
		spin_lock_init(&leds[i].lock);

		/* config GPIO */
		res = gpiod_direction_output(leds[i].gpiod, 0);
		if (res < 0) {
			dev_err(dev, "fail gpiod_direction_output()\n");
			continue;
		}

		/* rize timer */
		init_timer(&leds[i].timer);
		leds[i].timer.data = (unsigned long)&leds[i];
		leds[i].timer.expires = jiffies+1000;
		leds[i].timer.function = blink;
		led_logic(&leds[i]);
	}
	return res;
}

static int pkbbb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i;

	dev_info(dev, "remove...\n");
	for (i = 0; i < num_leds; i++) {
		class_remove_file(&leds[i].class_led, &class_attr_led_on);
		class_remove_file(&leds[i].class_led, &class_attr_led_off);
		class_unregister(&leds[i].class_led);
		if (timer_pending(&leds[i].timer))
			del_timer_sync(&leds[i].timer);
	}
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

module_platform_driver(pkbbb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Kulakov");
MODULE_DESCRIPTION("A simple Linux driver");

