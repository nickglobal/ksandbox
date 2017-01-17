/*
 * led_driver.c
 *
 *  Created on: 15.01.2017
 *      Author: Viktor Khabalevskyi
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/spinlock.h>

#define DRIVER_AUTHOR  "Viktor Khabalevskyi"
#define DRIVER_DESC "Test driver"
#define DRIVER_NAME "xkhavik_bbb"
#define DRIVER_COMPATIBLE "gl,console-heartbeat"

struct xkh_gpio_led {
	unsigned int on;
	unsigned int off;
	const char *name;
	const char *default_state;
	struct timer_list timer;
	struct class class_led;
	struct gpio_desc *gpiod;
	spinlock_t lock;
	bool state;
};
static struct xkh_gpio_led *leds;
static int number_leds;
static int index;
struct timer_list timer_leds;

static void timer_leds_handle(unsigned long param)
{
	struct xkh_gpio_led *led = (struct xkh_gpio_led *)param;
	unsigned long exp;
	spin_lock_bh(&led->lock);
    gpiod_set_value(led->gpiod, led->state ? LED_OFF : LED_FULL);
	spin_unlock_bh(&led->lock);
	if (led->state == LED_OFF) {
		exp = jiffies + msecs_to_jiffies(led->on);
		led->state = LED_FULL;

	}
	else {
		exp = jiffies + msecs_to_jiffies(led->on*(number_leds-1));
	    led->state = LED_OFF;
	}

	mod_timer(&led->timer, exp);
}


static const struct of_device_id xkhavik_of_match[] = {
	{.compatible = DRIVER_COMPATIBLE,},
	{ },
};

MODULE_DEVICE_TABLE(of, xkhavik_of_match);

static int xkhavik_probe(struct platform_device *pdev)
{
	const char         *str_property;
	struct device      *dev         = &pdev->dev;
	struct device_node *pnode        = dev->of_node;
	int i = 0;
	struct fwnode_handle *child;
	int res = 0;

	if (dev == NULL)
		return -ENODEV;

	if (pnode == NULL) {
		dev_err(dev, "There is not such device node");
		return -EINVAL;
	}

	dev_info(dev, "started prob module");

	if (of_property_read_string(pnode, "string-property",
					&str_property) == 0) {
		dev_info(dev, "string-property: %s\n", str_property);
	} else {
		dev_err(dev, "Error of reading string-property");
		return -EINVAL;
	}
	number_leds = device_get_child_node_count(dev);
	if (!number_leds) {
		dev_info(dev, "the leds are absent");
		return -ENODEV;
	}
	else
		dev_info(dev, "Number of leds is %d", number_leds);


	leds = devm_kzalloc(dev, sizeof(struct xkh_gpio_led) * number_leds, GFP_KERNEL);
	if (!leds) {
		dev_err(dev, "Error of memory allocation");
		return -ENOMEM;

	}

	device_for_each_child_node(dev, child) {

		leds[i].gpiod = devm_get_gpiod_from_child(dev, NULL, child);
		if (IS_ERR(leds[index].gpiod)) {
			dev_err(dev, "Gettig error of gpiod \n");
			res = -ENOENT;
			continue;
		}

		if (fwnode_property_present(child, "label"))
			fwnode_property_read_string(child, "label", &leds[i].name);
		else {
			dev_err(dev, "Error reading led %d name\n", i);
			res = -ENOENT;
			continue;
		}

		if (fwnode_property_present(child, "on")) {
			fwnode_property_read_u32(child, "on", &leds[i].on);
		}
		else {
			dev_err(dev, "Error reading led %d on time\n", i);
			res = -ENOENT;
			continue;
		}

		if (fwnode_property_present(child, "off")){
			fwnode_property_read_u32(child, "off", &leds[i].off);
		}
		else {
			dev_err(dev, "Error reading led %d off time\n", i);
			res = -ENOENT;
			continue;
		}

		if (fwnode_property_present(child, "default-state"))
			fwnode_property_read_string(child, "default-state", &leds[i].default_state);
		else {
			dev_err(dev, "Error reading led %d default-state\n", i);
			res = -ENOENT;
			continue;
		}

		res = gpiod_direction_output(leds[i].gpiod, 0);
		if (res < 0) {
			dev_err(dev, "Error gpiod direction output()\n");
			res = -ENOENT;
			continue;
		}
		dev_info(dev,"gpiod number %d is registred", index);
		gpiod_set_value(leds[i].gpiod, 0);

		/* init timer */
		init_timer(&leds[i].timer);
		leds[i].timer.data = (unsigned long)&leds[i];
		if (!strcmp(leds[i].default_state, "on")) {
			leds[i].timer.expires = jiffies+msecs_to_jiffies(leds[i].on);
			leds[i].state = LED_FULL;
		}
		else {
			leds[i].timer.expires = jiffies+msecs_to_jiffies(leds[i].off);
			leds[i].state = LED_OFF;
		}

		leds[i].timer.function = timer_leds_handle;
		add_timer(&leds[i].timer);
		i++;
	}

	return res;
}

static int xkhavik_remove(struct platform_device *pdev)
{
	int i;
	for (i = 0; i < number_leds; i++) {
		if (timer_pending(&leds[i].timer)) {
			del_timer_sync(&leds[i].timer);
		}
	}
	dev_info(&pdev->dev, "Remove module");
	return 0;
}

static struct platform_driver xkhavik_driver = {
	.probe = xkhavik_probe,
	.remove = xkhavik_remove,
	.driver = {
			.name = DRIVER_NAME,
			.of_match_table = of_match_ptr(xkhavik_of_match),
			.owner = THIS_MODULE,
		  },
};
module_platform_driver(xkhavik_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
