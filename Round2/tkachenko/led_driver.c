#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/spinlock.h>

#define DRIVER_AUTHOR  "O.Tkachenko"
#define DRIVER_DESC "A simple driver."

#define pdev_info(pdev, ...) (dev_info(&(pdev)->dev, __VA_ARGS__))
#define interval_ms(x) ((x) * HZ / 1000)

#define LED_ON_STR  "led-on"
#define LED_OFF_STR "led-off"
#define LABEL_STR   "label"

/* FWD declarations */

static ssize_t set_led_on_interval(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len);

static ssize_t get_led_on_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t set_led_off_interval(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len);

static ssize_t get_led_off_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static void krelease(struct kobject *kobj);

/* Data structures & attributes*/

struct gpio_led_data {
	const char			*name;
	struct kobject		led_kobj;
	struct gpio_desc	*gpiod;
	u32					led_on_ms;
	u32					led_off_ms;
	spinlock_t			lock;
	struct timer_list	timer;
	bool				enabled;
};

struct gpio_leds_priv {
	int num_leds;
	struct gpio_led_data leds[];
};

struct kobj_attribute kobj_attr_led_on_ms = __ATTR(
		led_on_ms,
		0664,
		get_led_on_interval,
		set_led_on_interval);

struct kobj_attribute kobj_attr_led_off_ms = __ATTR(
		led_off_ms,
		0664,
		get_led_off_interval,
		set_led_off_interval);

static struct attribute *leds_attr[] = {
	&kobj_attr_led_on_ms.attr,
	&kobj_attr_led_off_ms.attr,
	NULL,
};

static struct attribute_group led_attr_group = {
	.attrs = leds_attr,
};

static struct kobj_type led_ktype = {
	.release = krelease,
	.sysfs_ops = &kobj_sysfs_ops,
};

/* Static methods implementation */

static inline int sizeof_gpio_leds_priv(int num_leds)
{
	return sizeof(struct gpio_leds_priv) +
		(sizeof(struct gpio_led_data) * num_leds);
}

/* Set delay before turn on led  */
static ssize_t set_led_on_interval(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	struct gpio_led_data *led =
		container_of(kobj, struct gpio_led_data, led_kobj);
	int ret = len;
	u32 new_interval;

	spin_lock_bh(&led->lock);
	if (len) {
		if (kstrtoint(buf, 10, &new_interval)) {
			ret = 0;
			goto cleanup;
		}
	} else
		new_interval = 0;

	if (led->led_on_ms == 0 && new_interval > 0 && !led->enabled)
		mod_timer(&led->timer, jiffies + interval_ms(new_interval));

	led->led_on_ms = new_interval;

cleanup:
	spin_unlock_bh(&led->lock);
	return ret;
}

/* Set delay before turn off led  */
static ssize_t set_led_off_interval(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	struct gpio_led_data *led =
		container_of(kobj, struct gpio_led_data, led_kobj);
	int	ret = len;
	u32	new_interval;

	spin_lock_bh(&led->lock);

	if (len) {
		if (kstrtoint(buf, 10, &new_interval)) {
			ret = 0;
			goto cleanup;
		}
	} else
		new_interval = 0;

	if (led->led_off_ms == 0 && new_interval > 0 && led->enabled)
		mod_timer(&led->timer, jiffies + interval_ms(new_interval));

	led->led_off_ms = new_interval;

cleanup:
	spin_unlock_bh(&led->lock);
	return ret;
}

static ssize_t get_led_on_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct gpio_led_data *led =
		container_of(kobj, struct gpio_led_data, led_kobj);
	spin_lock_bh(&led->lock);
	sprintf(buf, "%d", led->led_on_ms);
	spin_unlock_bh(&led->lock);
	return strlen(buf);
}

static ssize_t get_led_off_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct gpio_led_data *led =
		container_of(kobj, struct gpio_led_data, led_kobj);

	spin_lock_bh(&led->lock);
	sprintf(buf, "%d", led->led_off_ms);
	spin_unlock_bh(&led->lock);
	return strlen(buf);
}

static void krelease(struct kobject *kobj)
{
	/* nothing to do since memory is freed by gpio_leds_remove */
}

/* Swithc on/off led */
static void trigger_led(unsigned long __data)
{
	struct gpio_led_data *led = (void *)__data;

	spin_lock_bh(&led->lock);

	led->enabled = !led->enabled;
	gpiod_set_value(led->gpiod, led->enabled ? LED_FULL : LED_OFF);
	if (led->enabled && led->led_off_ms > 0)
		mod_timer(&led->timer, jiffies + interval_ms(led->led_off_ms));
	else if (!led->enabled && led->led_on_ms > 0)
		mod_timer(&led->timer, jiffies + interval_ms(led->led_on_ms));

	spin_unlock_bh(&led->lock);
}


/* Get parameters from device description */
static int
parse_led_params(struct fwnode_handle *handle, struct gpio_led_data *led)
{
	int ret = 0;
	struct device_node *np;

	np = to_of_node(handle);

	if (fwnode_property_present(handle, LABEL_STR))
		fwnode_property_read_string(handle, LABEL_STR, &led->name);
	else if (IS_ENABLED(CONFIG_OF) && !led->name && np)
		led->name = np->name;

	if (!led->name) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (fwnode_property_present(handle, LED_ON_STR))
		ret = fwnode_property_read_u32(handle,
				LED_ON_STR,
				&led->led_on_ms);
	else
		ret = of_property_read_u32(np,
				LED_ON_STR,
				&led->led_on_ms);

	if (ret)
		goto cleanup;

	if (fwnode_property_present(handle, LED_OFF_STR))
		ret = fwnode_property_read_u32(handle,
				LED_OFF_STR,
				&led->led_off_ms);
	else
		ret = of_property_read_u32(np, LED_OFF_STR, &led->led_off_ms);

cleanup:
	return ret;
}

/* Initialize LED handlers */
static int init_led(struct platform_device *pdev, struct gpio_led_data *led)
{
	int ret = 0;

	pdev_info(pdev, "Init led: [%s]\n", led->name);
	ret = kobject_init_and_add(
			&led->led_kobj,
			&led_ktype,
			&pdev->dev.kobj,
			"%s", led->name);
	if (ret)
		goto cleanup;

	ret = sysfs_create_group(&led->led_kobj, &led_attr_group);
	if (ret)
		goto cleanup;

	gpiod_direction_output(led->gpiod, LED_OFF);
	spin_lock_init(&led->lock);
	setup_timer(&led->timer, trigger_led, (unsigned long) led);
	if (led->led_on_ms)
		mod_timer(&led->timer, jiffies + interval_ms(led->led_on_ms));
cleanup:
	return ret;
}

static void remove_led(struct platform_device *pdev, struct gpio_led_data *led)
{
	pdev_info(pdev, "Remove led [%s]\n", led->name);
	del_timer_sync(&led->timer);
	gpiod_set_value(led->gpiod, LED_OFF);
	sysfs_remove_group(&led->led_kobj, &led_attr_group);
	kobject_put(&led->led_kobj);
}

static void clean_privdata(struct platform_device *pdev,
		struct gpio_leds_priv *priv)
{
	int i;

	if (priv == NULL)
		return;

	for (i = priv->num_leds - 1; i >= 0; i--)
		remove_led(pdev, &priv->leds[i]);

	kfree(priv);
}

static int gpio_leds_probe(struct platform_device *pdev)
{
	struct gpio_leds_priv *priv;
	struct fwnode_handle *child;
	int count, ret = 0;

	pdev_info(pdev, "Probe!\n");

	count = device_get_child_node_count(&pdev->dev);

	if (!count)
		return -ENODEV;

	priv = kzalloc(sizeof_gpio_leds_priv(count), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	device_for_each_child_node(&pdev->dev, child) {
		priv->leds[priv->num_leds].gpiod =
			devm_get_gpiod_from_child(&pdev->dev, NULL, child);

		if (IS_ERR(priv->leds[priv->num_leds].gpiod)) {
			fwnode_handle_put(child);
			ret = PTR_ERR(priv->leds[priv->num_leds].gpiod);
			goto cleanup;
		}

		ret = parse_led_params(child, &priv->leds[priv->num_leds]);
		if (ret) {
			fwnode_handle_put(child);
			goto cleanup;
		}

		ret = init_led(pdev, &priv->leds[priv->num_leds]);
		if (ret)
			goto cleanup;

		priv->num_leds++;
	}
	platform_set_drvdata(pdev, priv);

cleanup:
	if (ret) {
		pdev_info(pdev, "Probe error [%d]\n", ret);
		clean_privdata(pdev, priv);
	}
	return ret;
}

static int gpio_leds_remove(struct platform_device *pdev)
{
	struct gpio_leds_priv *priv = platform_get_drvdata(pdev);

	pdev_info(pdev, "Remove driver\n");
	clean_privdata(pdev, priv);
	return 0;
}

static const struct of_device_id custom_led_of_match[] = {
	{
		.compatible = "gpio-leds-tkachenko",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, custom_led_of_match);

static struct platform_driver gpio_led_driver = {
	.probe = gpio_leds_probe,
	.remove = gpio_leds_remove,
	.driver = {
		.name = "leds-gpio-tkachenko",
		.of_match_table = of_match_ptr(custom_led_of_match),
		.owner = THIS_MODULE,
		},
};

module_platform_driver(gpio_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
