/*
 * training_leds.c
 *
 *  Created on: Jan 2, 2017
 *      Author: vvs
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/spinlock.h>

//struct led_classdev;
struct gpio_led_data {
	struct led_classdev cdev;
	struct gpio_desc *gpiod;
	struct work_struct work;
	uint8_t new_level;
	uint8_t can_sleep;
	uint8_t blinking;
	bool blink_state;
	uint32_t delay_on;
	uint32_t delay_off;
	struct device_attribute delay_attrs[3];
	struct attribute *attrs[3];
	struct attribute_group delay_attr_group;
	struct attribute_group *delay_attr_groups[2];

	struct timer_list timer;

	int (*pltfrm_gpio_blink_set)(struct gpio_desc *desc,
		int state, unsigned long *delay_on, unsigned long *delay_off);
	/* spinlock_t lock; */
};

struct gpio_leds_priv {
	int num_leds;
	struct gpio_led_data leds[];
};

static inline int sizeof_gpio_leds_priv(int num_leds)
{
	return sizeof(struct gpio_leds_priv) +
			(sizeof(struct gpio_led_data) * num_leds);
}

static struct gpio_leds_priv *gpio_leds_create(struct platform_device *pdev);
static int create_gpio_led(const struct gpio_led *template,
		struct gpio_led_data *led_data, struct device *parent,
		int (*blink_set)(struct gpio_desc *, int, unsigned long *,
						unsigned long *));
static void delete_gpio_led(struct gpio_led_data *led_data);

void led_blink_timer_callback(unsigned long p_data)
{
	struct gpio_led_data *led_data = (struct gpio_led_data *) p_data;
	unsigned long expires;

	if (led_data->blinking) {
		dev_info(led_data->cdev.dev, "%s blink_state = %d\r\n",
				led_data->cdev.name, led_data->blink_state);
		if (led_data->blink_state) {
			gpiod_direction_output(led_data->gpiod, LED_OFF);
			expires = jiffies + msecs_to_jiffies(led_data->delay_off);
		} else {
			gpiod_direction_output(led_data->gpiod, LED_FULL);
			expires = jiffies + msecs_to_jiffies(led_data->delay_on);
		}
		led_data->blink_state = !led_data->blink_state;
		/* reload timer here */
		if (timer_pending(&led_data->timer))
			mod_timer(&led_data->timer, expires);
		else {
			led_data->timer.expires = expires;
			add_timer(&led_data->timer);
		}
	}
}

int led_data_blink_set(struct gpio_desc *gpiod, int state,
					unsigned long *delay_on, unsigned long *delay_off)
{
	if (delay_on == NULL && delay_off == NULL)
		gpiod_direction_output(gpiod, state);
	return 0;
}

static ssize_t	delay_on_show(struct device *dev,
							struct device_attribute *attr, char *str)
{
	struct gpio_led_data *led_data = container_of(attr,
								struct gpio_led_data, delay_attrs[0]);

	return snprintf(str, 16, "%u", led_data->delay_on);
}

static ssize_t	delay_on_store(struct device *dev,
							struct device_attribute *attr,
							const char *str, size_t count)
{
	struct gpio_led_data *led_data = container_of(attr,
							struct gpio_led_data, delay_attrs[0]);
	unsigned long new_value;

	if (kstrtol(str, 0, &new_value) == 0) {
		dev_info(led_data->cdev.dev, "input value = %lu\r\n", new_value);
		if (new_value > 5000)
			return count;
		led_data->cdev.blink_set(&led_data->cdev, &new_value, NULL);
	} else
		dev_info(led_data->cdev.dev, "fail to parse input value\r\n");
	return count;
}

static ssize_t	delay_off_show(struct device *dev,
						struct device_attribute *attr, char *str)
{
	struct gpio_led_data *led_data = container_of(attr,
								struct gpio_led_data, delay_attrs[1]);

	return snprintf(str, 16, "%u", led_data->delay_off);
}

static ssize_t	delay_off_store(struct device *dev,
						struct device_attribute *attr,
						const char *str, size_t count)
{
	struct gpio_led_data *led_data = container_of(attr,
								struct gpio_led_data, delay_attrs[1]);
	long int new_value;

	if (kstrtol(str, 0, &new_value) == 0) {
		dev_info(led_data->cdev.dev, "input value = %ld\r\n", new_value);
		if (new_value > 5000)
			return count;
		led_data->cdev.blink_set(&led_data->cdev, NULL, &new_value);
	} else {
		dev_info(led_data->cdev.dev, "fail to parse input value\r\n");
	}
	return count;
}

int init_training_leds(struct platform_device *pdev)
{
	struct gpio_led_platform_data *pdata;
	struct gpio_leds_priv *priv;
	int i, ret = 0;

	if (pdev == NULL || &pdev->dev == NULL) {
		dev_info(&pdev->dev, "Invalid parameters\r\n");
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		if (pdata != NULL)
			dev_info(&pdev->dev, "number of leds %d", pdata->num_leds);
	}

	if (pdata != NULL) {
		if (pdata->num_leds <= 0)
			return -EIO;

		priv = devm_kzalloc(&pdev->dev,
							sizeof_gpio_leds_priv(pdata->num_leds),
							GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		priv->num_leds = pdata->num_leds;
		for (i = 0; i < priv->num_leds; i++) {
			ret = create_gpio_led(&pdata->leds[i],
								  &priv->leds[i],
								  &pdev->dev, pdata->gpio_blink_set);
			if (ret < 0) {
				/* On failure: unwind the led creations */
				for (i = i - 1; i >= 0; i--)
					delete_gpio_led(&priv->leds[i]);
				return ret;
			}
		}
	} else {
		priv = gpio_leds_create(pdev);
		if (IS_ERR(priv))
			return PTR_ERR(priv);
	}
	platform_set_drvdata(pdev, priv);
	return 0;
}

static void gpio_led_work(struct work_struct *work)
{
	struct gpio_led_data *led_data =
	container_of(work, struct gpio_led_data, work);

	if (led_data->blinking) {
		led_data->pltfrm_gpio_blink_set(led_data->gpiod,
										led_data->new_level, NULL, NULL);
		led_data->blinking = 0;
	} else
		gpiod_set_value_cansleep(led_data->gpiod, led_data->new_level);
}

static void gpio_led_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	struct gpio_led_data *led_data =
				container_of(led_cdev, struct gpio_led_data, cdev);
	int level;

	if (value == LED_OFF)
		level = 0;
	else
		level = 1;

	/* Setting GPIOs with I2C/etc requires a task context, and we don't
	 * seem to have a reliable way to know if we're already in one; so
	 * let's just assume the worst.
	 */
	if (led_data->can_sleep) {
		led_data->new_level = level;
		schedule_work(&led_data->work);
	} else {
		if (led_data->blinking && value != LED_OFF) {
			led_cdev->brightness = value;
			led_cdev->blink_set(led_cdev,
						(unsigned long *) &led_data->delay_on,
						(unsigned long *) &led_data->delay_off);
		} else {
			led_data->blinking = 0;
			gpiod_set_value(led_data->gpiod, level);
		}

	}
}

static int gpio_blink_set(struct led_classdev *led_cdev,
			unsigned long *delay_on, unsigned long *delay_off)
{
	struct gpio_led_data *led_data =
			container_of(led_cdev, struct gpio_led_data, cdev);
	int result;

	if (delay_on != NULL)
		led_data->delay_on = *delay_on;
	if (delay_off != NULL)
		led_data->delay_off = *delay_off;
	led_data->blinking = 1;

	if (timer_pending(&led_data->timer)) {
		dev_info(led_data->cdev.dev, "timer is pending %p\r\n",
												&led_data->timer);
		mod_timer(&led_data->timer, jiffies +
								msecs_to_jiffies(led_data->delay_on));
	} else {
		dev_info(led_data->cdev.dev, "timer is NOT pending\r\n");
		led_data->timer.expires = jiffies +
								msecs_to_jiffies(led_data->delay_on);
		add_timer(&led_data->timer);
	}
	if (delay_on != NULL && delay_off != NULL)
		dev_info(led_data->cdev.dev,
				"blink_set invoked: %ld / %ld\r\n",
				*delay_on, *delay_off);
	else
		dev_info(led_data->cdev.dev, "blink_set with NULL\r\n");

	result = led_data->pltfrm_gpio_blink_set(led_data->gpiod, GPIO_LED_BLINK,
											delay_on, delay_off);
	return result;
}

static int create_gpio_led(const struct gpio_led *template,
	struct gpio_led_data *led_data, struct device *parent,
	int (*blink_set)(struct gpio_desc *, int, unsigned long *,
					 unsigned long *))
{
	int ret, state;

	led_data->gpiod = template->gpiod;
	if (!led_data->gpiod) {
		/*
		 * This is the legacy code path for platform code that
		 * still uses GPIO numbers. Ultimately we would like to get
		 * rid of this block completely.
		 */
		unsigned long flags = 0;

		/* skip leds that aren't available */
		if (!gpio_is_valid(template->gpio)) {
			dev_info(parent, "Skipping unavailable LED gpio %d (%s)\n",
							template->gpio, template->name);
			return 0;
		}

		if (template->active_low)
			flags |= GPIOF_ACTIVE_LOW;

		ret = devm_gpio_request_one(parent, template->gpio, flags,
									template->name);
		if (ret < 0)
			return ret;

		led_data->gpiod = gpio_to_desc(template->gpio);
		if (IS_ERR(led_data->gpiod))
			return PTR_ERR(led_data->gpiod);
	}

	led_data->cdev.name = template->name;
	led_data->cdev.default_trigger = template->default_trigger;
	led_data->can_sleep = gpiod_cansleep(led_data->gpiod);
	led_data->blinking = 0;
	if (blink_set) {
		led_data->pltfrm_gpio_blink_set = blink_set;
		led_data->cdev.blink_set = gpio_blink_set;
	}
	led_data->cdev.brightness_set = gpio_led_set;
	if (template->default_state == LEDS_GPIO_DEFSTATE_KEEP)
		state = !!gpiod_get_value_cansleep(led_data->gpiod);
	else
		state = (template->default_state == LEDS_GPIO_DEFSTATE_ON);
	led_data->cdev.brightness = state ? LED_FULL : LED_OFF;
	if (!template->retain_state_suspended)
		led_data->cdev.flags |= LED_CORE_SUSPENDRESUME;

	ret = gpiod_direction_output(led_data->gpiod, state);
	if (ret < 0)
		return ret;
	dev_info(led_data->cdev.dev, "init timer");
	init_timer(&led_data->timer);
	led_data->timer.function = led_blink_timer_callback;
	led_data->timer.expires = jiffies +
								msecs_to_jiffies(led_data->delay_on);
	led_data->timer.data = (unsigned long)led_data;
	add_timer(&led_data->timer);

	INIT_WORK(&led_data->work, gpio_led_work);

	led_data->delay_attrs[0].attr.name = "delay_on";
	led_data->delay_attrs[0].attr.mode = 0664;
	led_data->delay_attrs[0].show = delay_on_show;
	led_data->delay_attrs[0].store = delay_on_store;

	led_data->delay_attrs[1].attr.name = "delay_off";
	led_data->delay_attrs[1].attr.mode = 0664;
	led_data->delay_attrs[1].show = delay_off_show;
	led_data->delay_attrs[1].store = delay_off_store;

	led_data->attrs[0] = &led_data->delay_attrs[0].attr;
	led_data->attrs[1] = &led_data->delay_attrs[1].attr;
	led_data->attrs[2] = NULL;

	led_data->delay_attr_group.name = "delays";
	led_data->delay_attr_group.attrs =
			(struct attribute **) &led_data->attrs;

	led_data->delay_attr_groups[0] = &led_data->delay_attr_group;
	led_data->delay_attr_groups[1] = NULL;

	led_data->cdev.groups =
			(const struct attribute_group **) &led_data->delay_attr_groups;

	return led_classdev_register(parent, &led_data->cdev);
}

static void delete_gpio_led(struct gpio_led_data *led_data)
{
	dev_info(led_data->cdev.dev, "delete_gpio_led\r\n");
	if (timer_pending(&led_data->timer))
		del_timer(&led_data->timer);

	led_classdev_unregister(&led_data->cdev);
	cancel_work_sync(&led_data->work);
}

static struct gpio_leds_priv *gpio_leds_create(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *child_node;
	struct device_node *leds_node;
	struct gpio_leds_priv *priv;
	int count, ret;
	struct device_node *np;


	leds_node = of_get_next_available_child(dev->of_node, NULL);
	if (leds_node == NULL)
		return ERR_PTR(-ENODEV);

	count = of_get_child_count(leds_node);
	if (!count)
		return ERR_PTR(-ENODEV);

	priv = devm_kzalloc(dev, sizeof_gpio_leds_priv(count), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	for_each_available_child_of_node(leds_node, child_node) {
		struct gpio_led led = {};
		const char *state = NULL;

		led.gpiod = devm_get_gpiod_from_child(dev, NULL,
												&child_node->fwnode);
		if (IS_ERR(led.gpiod)) {
			fwnode_handle_put(&child_node->fwnode);
			ret = PTR_ERR(led.gpiod);
			goto err;
		}

		np = child_node;

		if (fwnode_property_present(&child_node->fwnode, "label")) {
			fwnode_property_read_string(&child_node->fwnode,
										"label", &led.name);
			dev_info(dev, "LED name -> %s", led.name);
		} else {
			if (IS_ENABLED(CONFIG_OF) && !led.name && np)
				led.name = np->name;
			if (!led.name) {
				ret = -EINVAL;
				goto err;
			}
		}
		fwnode_property_read_string(&child_node->fwnode,
									"linux,default-trigger",
									&led.default_trigger);

		if (!fwnode_property_read_string(&child_node->fwnode,
										"default-state", &state)) {
			if (!strcmp(state, "keep"))
				led.default_state = LEDS_GPIO_DEFSTATE_KEEP;
			else if (!strcmp(state, "on"))
				led.default_state = LEDS_GPIO_DEFSTATE_ON;
			else
				led.default_state = LEDS_GPIO_DEFSTATE_OFF;
		}

		if (fwnode_property_present(&child_node->fwnode,
									"retain-state-suspended"))
		led.retain_state_suspended = 1;

		if (fwnode_property_present(&child_node->fwnode, "delay_on")) {
			fwnode_property_read_u32(&child_node->fwnode,
									"delay_on",
									&priv->leds[priv->num_leds].delay_on);
			dev_info(&pdev->dev, "DTS: delay_on = %u",
					priv->leds[priv->num_leds].delay_on);
		}

		if (fwnode_property_present(&child_node->fwnode, "delay_off")) {
			fwnode_property_read_u32(&child_node->fwnode,
									"delay_off",
									&priv->leds[priv->num_leds].delay_off);
			dev_info(&pdev->dev, "DTS: delay_off = %u",
					priv->leds[priv->num_leds].delay_off);
		}

		ret = create_gpio_led(&led, &priv->leds[priv->num_leds],
							  dev, led_data_blink_set);
		if (ret < 0) {
			fwnode_handle_put(&child_node->fwnode);
			goto err;
		}
		priv->num_leds++;
	}

	return priv;
err:
	for (count = priv->num_leds - 1; count >= 0; count--)
		delete_gpio_led(&priv->leds[count]);
	return ERR_PTR(ret);
}

int deinit_training_leds(struct platform_device *pdev)
{
	struct gpio_leds_priv *priv;
	int i;

	if (pdev == NULL)
		return 0;
	priv = platform_get_drvdata(pdev);
	if (priv != NULL && priv->num_leds > 0)
		for (i = 0; i < priv->num_leds; i++)
			delete_gpio_led(&priv->leds[i]);

	return 0;
}
