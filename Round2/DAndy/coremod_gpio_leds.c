
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
#include <linux/timer.h>

#define DRIVER_NAME			"x_led_drv"


static struct device *dev;

static const struct of_device_id my_drvr_match[];


#define LED_MODE_OFF		0
#define LED_MODE_ON			1
#define LED_MODE_BLINK		2


#define DEFAULT_ON_TIME		500
#define DEFAULT_OFF_TIME	500


struct x_led_data {
	struct class *led_class;
	struct gpio_desc *gpiod;
	struct timer_list timer;
	u32 on_time;
	u32 off_time;
	u8	mode;
	u8	curr_state;
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




static struct x_led_data *get_led(struct class *class)
{
	struct x_led_data *led;
	u32 i;

	for (i = 0; i < priv->num_leds; i++) {
		if (priv->leds[i].led_class == class){
			led = &priv->leds[i];
			break;
		}
	}

	return led;
}



static void timer_callback(unsigned long param)
{
	struct x_led_data *led = (struct x_led_data *)param;

	if(!led)
		dev_err(dev, "timer_callback ERROR\n");

	led->curr_state ^= 1;

	gpiod_set_value(led->gpiod, led->curr_state);
	mod_timer(&led->timer, led->curr_state ?
			jiffies + led->off_time * HZ / 1000 :
			jiffies + led->on_time * HZ / 1000);	
			
}



static ssize_t sys_led_on(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;
	struct x_led_data *led = get_led(class);

	i += sprintf(buf, "LED mode = ON\n");
	dev_info(dev, "LED mode = ON\n");

	if (timer_pending(&led->timer))
		del_timer_sync(&led->timer);

	led->mode = LED_MODE_ON;
	gpiod_set_value(led->gpiod, 1);
	return i;
}

static ssize_t sys_led_off(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;

	struct x_led_data *led = get_led(class);

	i += sprintf(buf, "LED mode = OFF\n");
	dev_info(dev, "LED mode = OFF\n");

	if (timer_pending(&led->timer))
		del_timer_sync(&led->timer);

	led->mode = LED_MODE_OFF;
	gpiod_set_value(led->gpiod, 0);
	return i;
}

static ssize_t sys_led_blink(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;

	struct x_led_data *led = get_led(class);
	//struct x_led_data *led = &priv->leds[0];

	if(!led)
		dev_err(dev, "sys_led_blink ERROR\n");

	i += sprintf(buf, "LED mode = BLINK\n");
	dev_info(dev, "LED mode = BLINK\n");

	led->mode = LED_MODE_BLINK;
	mod_timer(&led->timer, jiffies + led->on_time);
	return i;
}


static ssize_t sys_set_on_time(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	dev_info(dev, "sys_set_on_time\n");
	// TODO: add setting on_time up
	return 0;
}


static ssize_t sys_set_off_time(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	dev_info(dev, "sys_set_off_time\n");
	// TODO: add setting off_time up
	return 0;
}


/* <linux/device.h>
* #define CLASS_ATTR(_name, _mode, _show, _store) \
* struct class_attribute class_attr_##_name = __ATTR(_name, _mode,
* _show, _store)
*/
CLASS_ATTR(myled_on, 0664, &sys_led_on, NULL);
CLASS_ATTR(myled_off, 0664, &sys_led_off, NULL);
CLASS_ATTR(myled_blink, 0664, &sys_led_blink, NULL);
CLASS_ATTR(myled_on_time, 0664, NULL, &sys_set_on_time);
CLASS_ATTR(myled_off_time, 0664, NULL, &sys_set_off_time);


static void get_platform_info(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct fwnode_handle *child;
	const char *name;
	const char *state;
	struct gpio_desc *gpiod;
	struct class *led_class;

	int res;
	u32 value;
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

			dev_info(dev, "added led - \"%s\"\n", name);

			if (fwnode_property_present(child, "default-state")){
				fwnode_property_read_string(child, "default-state",	&state);

				if (!strcmp(state, "on")){
					priv->leds[i].mode = LED_MODE_ON;
					priv->leds[i].curr_state = 1;
					gpiod_set_value(gpiod, 1);
				}
				else if (!strcmp(state, "blink")) {
					priv->leds[i].mode = LED_MODE_BLINK;
					priv->leds[i].curr_state = 1;
					gpiod_set_value(gpiod, 0);
				}
				else {
					priv->leds[i].curr_state = 0;
					priv->leds[i].mode = LED_MODE_OFF;
					gpiod_set_value(gpiod, 0);
				}


				dev_info(dev, "default-state - %s\n", state);
			}
			
			//priv->leds[i].timer = TIMER_INITIALIZER(timer_callback, 1000, 0);

			//init_timer(&priv->leds[i].timer);
			//priv->leds[i].timer.data = (unsigned long)priv->leds[i].led_class;


			if (!fwnode_property_read_u32(child, "on_time", &value))
				priv->leds[i].on_time = value;
			else
				priv->leds[i].on_time = DEFAULT_ON_TIME;

			if (!fwnode_property_read_u32(child, "off_time", &value))
				priv->leds[i].off_time = value;
			else
				priv->leds[i].off_time = DEFAULT_OFF_TIME;

			setup_timer(&priv->leds[i].timer, timer_callback, (unsigned long)&priv->leds[i]);

			if (priv->leds[i].mode == LED_MODE_BLINK)
				mod_timer(&priv->leds[i].timer, jiffies + priv->leds[i].on_time);

			dev_info(dev, "on_time = %d; off_time = %d;\n", priv->leds[i].on_time, priv->leds[i].off_time);

			led_class = class_create(THIS_MODULE, name);

			if (IS_ERR(led_class))
				dev_info(dev, "bad class create\n");

			res = class_create_file(led_class, &class_attr_myled_on);
			res = class_create_file(led_class, &class_attr_myled_off);
			res = class_create_file(led_class, &class_attr_myled_blink);

			res = class_create_file(led_class, &class_attr_myled_on_time);
			res = class_create_file(led_class, &class_attr_myled_off_time);			

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
	struct x_led_data *led;
	u32 i = 0;

	for (i = 0; i < priv->num_leds; i++) {
		led_class = priv->leds[i].led_class;

		led = get_led(led_class);
	
		if (timer_pending(&led->timer))
			del_timer_sync(&led->timer);

		class_remove_file(led_class, &class_attr_myled_on);
		class_remove_file(led_class, &class_attr_myled_off);
		class_remove_file(led_class, &class_attr_myled_on_time);
		class_remove_file(led_class, &class_attr_myled_off_time);
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
