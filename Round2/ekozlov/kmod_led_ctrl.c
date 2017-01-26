#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h> /* kstr*, sscanf */
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/printk.h>

#include "kmod_debug.h"


#define KMOD_NAME	"kmod_led_ctrl"
#define KMOD_COMPAT	"ekc-gpio-leds" /* former "gpio-leds" */


static const struct of_device_id of_match_leds[] = {
	{ .compatible = KMOD_COMPAT, NULL },
	{ }
};
MODULE_DEVICE_TABLE(of, of_match_leds);

#define KMOD_DEFAULT_BLINK_PERIOD	500	/* ms */
#define KMOD_DEFAULT_BLINK_INTERVAL	1000	/* ms */


typedef struct {
	bool	enabled;
	u32	period;
	u32	interval;
} led_config_t;

typedef struct {
	struct timer_list	period_tmr;
	struct timer_list	interval_tmr;
} blink_timer_t;

enum {
	LED_NAME_SZ = 5,
	LED_ID_OFFSET = 3
};

typedef struct {
    	char 			name[LED_NAME_SZ];
	struct gpio_desc *	gpiodesc;
        const char *		label;
        led_config_t 		config;
        blink_timer_t		timers;
        struct class_attribute  cattr;
} led_t;

static struct {
	struct class *	leds_class;
	led_config_t	led_default_cfg;
	u32 		nleds;
	led_t *		leds;
	spinlock_t 	lock;
} g_kmod_data = {
	.leds_class = NULL,
	.led_default_cfg = {
		.enabled = 1,
		.period = KMOD_DEFAULT_BLINK_PERIOD,
		.interval = KMOD_DEFAULT_BLINK_INTERVAL },
	.nleds = 0,
	.leds = NULL
	};

module_param_named(interval, (g_kmod_data.led_default_cfg.interval), uint, 0);
module_param_named(period, (g_kmod_data.led_default_cfg.period), uint, 0);


int parse_new_config(
	/* in */ const char *cfg_buf,
	/* in */ size_t count,
	/* out */ led_config_t *led_cfg)
{
	int rc = 0;
	u32 enabled = 0;

	rc = sscanf(cfg_buf, "%u %u %u", &enabled, &led_cfg->period, &led_cfg->interval);
	pr_info(EK_DRV_TAG "using sscanf: enabled = %u, period = %u, interval = %u, r = %d",
		enabled, led_cfg->period, led_cfg->interval, rc);
	if (rc != 3) { /* all 3 parameters must be retrieved */
		pr_err(EK_DRV_TAG "invalid arguments");
		return -EINVAL;
	} else if (enabled > 1) {
		pr_err(EK_DRV_TAG "invalid 'enable' argument");
		return -EINVAL;
	}

	led_cfg->enabled = enabled ? true : false;

	return 0;
}


void enable_led_tick(unsigned long id)
{
	int rc = 0;
	unsigned long exp;

	if (!g_kmod_data.leds[id].config.enabled)
		return;

	gpiod_direction_output(g_kmod_data.leds[id].gpiodesc, 1);

	exp = jiffies + msecs_to_jiffies(g_kmod_data.leds[id].config.period);
	rc = mod_timer(&g_kmod_data.leds[id].timers.interval_tmr, exp);
	if(rc)
		pr_err(EK_DRV_TAG "cannot enable interval timer for led #%lu", id);

}


void disable_led_tick(unsigned long id)
{
	int rc = 0;
	unsigned long exp;

	if (!g_kmod_data.leds[id].config.enabled)
		return;

	gpiod_direction_output(g_kmod_data.leds[id].gpiodesc, 0);

	exp = jiffies + msecs_to_jiffies(g_kmod_data.leds[id].config.interval);
	rc = mod_timer(&g_kmod_data.leds[id].timers.period_tmr, exp);
	if(rc)
		pr_err(EK_DRV_TAG "cannot enable period timer for led #%lu", id);
}


static int setup_led_blink(u32 id, bool enabled, u32 period, u32 interval)
{
	int rc = 0;
	if (enabled) {
		rc = mod_timer(&g_kmod_data.leds[id].timers.period_tmr, jiffies);
		if(rc)
			pr_err(EK_DRV_TAG "cannot enable period timer for led #%u", id);
	} else
		gpiod_direction_output(g_kmod_data.leds[id].gpiodesc, 0);

	return rc;
}


static ssize_t kmod_store_led_by_id (
	u32 led_id,
	struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	FNIN;

	if (parse_new_config(buf, count, &g_kmod_data.leds[led_id].config)) {
		pr_err(EK_DRV_TAG "Invalid string of config parameters\n");
		goto EXIT;
	}

#define	CFG_	g_kmod_data.leds[led_id].config
	setup_led_blink(led_id, CFG_.enabled, CFG_.period, CFG_.interval);
#undef	CFG_

EXIT:
	FNOUT;
	return count;

}


int get_led_id_by_name(const char *name)
{
	int res = 0;
	int id = -1;

	res = sscanf(name + LED_ID_OFFSET, "%d", &id);
	if (res != 1) {
		pr_err(EK_DRV_TAG "cannot get_led_id_by_name for name %s", name);
		id = -1;
	} else if (id < 0 || id >= g_kmod_data.nleds) {
		pr_err(EK_DRV_TAG "led id (%d)is out of acceptable range %s", id, name);
		id = -1;
	}

	return id;
}


/*
 * led configuration string
 *  enabled period interval
 *  e.g. 1 1000 5000
 *  on=0..1 - 0 - led is off, 1 - is blinking
 *  period (in ms) - duration of flash
 *  interval (in ms), time gap between led flashes
 */
static ssize_t kmod_show_led(
	struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int id = get_led_id_by_name(attr->attr.name);
	if (id == -1)
		return 0;
	else {
		sprintf(buf, "%u %u %u",
			g_kmod_data.leds[id].config.enabled,
			g_kmod_data.leds[id].config.period,
			g_kmod_data.leds[id].config.interval);
		return strlen(buf);
	}
}


static ssize_t kmod_store_led(
	struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int id = get_led_id_by_name(attr->attr.name);
	if (id == -1)
		return 0;
	else
		return kmod_store_led_by_id(id, class, attr, buf, count);
}


static int create_led_class(void)
{
	int rc = 0;
	int i = 0;

	FNIN;

	g_kmod_data.leds_class = class_create(THIS_MODULE, KMOD_NAME);
	if (IS_ERR(g_kmod_data.leds_class)) {
		pr_err(EK_DRV_TAG "class was not created");
		rc = PTR_ERR(g_kmod_data.leds_class);
		goto EXIT;
	}

	for (i = 0; i < g_kmod_data.nleds; ++ i) {
		rc = class_create_file(g_kmod_data.leds_class, &g_kmod_data.leds[i].cattr);
		if (rc) {
			pr_err(EK_DRV_TAG "class file was not created (led0)");
			goto EXIT_FILE;
		}
	}

	goto EXIT; /* on success */

EXIT_FILE:
	for (i -= 1; i >= 0; --i)
		class_remove_file(g_kmod_data.leds_class, &g_kmod_data.leds[i].cattr);

	class_destroy(g_kmod_data.leds_class);

EXIT:
	FNOUTRC;
	return rc;
}


static void remove_led_class(void)
{
	int i = 0;

	FNIN;

	for (i = 0; i < g_kmod_data.nleds; ++ i)
		class_remove_file(g_kmod_data.leds_class, &g_kmod_data.leds[i].cattr);

	class_destroy(g_kmod_data.leds_class);
	FNOUT;
}


static int create_led_timers(void)
{
	int i = 0;

	FNIN;

#define PT_	g_kmod_data.leds[i].timers.period_tmr
#define IT_	g_kmod_data.leds[i].timers.interval_tmr

	for (i = 0; i < g_kmod_data.nleds; ++ i) {
		setup_timer(&PT_, enable_led_tick, i);
		setup_timer(&IT_, disable_led_tick, i);
	}

#undef PT_
#undef IT_

	FNOUT;

	return 0;
}


static int remove_led_timers(void)
{
	int i = 0;
	int rc = 0;

	FNIN;

#define PT_	g_kmod_data.leds[i].timers.period_tmr
#define IT_	g_kmod_data.leds[i].timers.interval_tmr

	for (i = 0; i < g_kmod_data.nleds; ++ i) {
		g_kmod_data.leds[i].config.enabled = false;
		del_timer(&PT_);
		del_timer(&IT_);
	}

#undef PT_
#undef IT_

	FNOUT;

	return rc;
}


void update_led_state(void)
{
	int rc = 0;
	int i = 0;


	for (i = 0; i < g_kmod_data.nleds; ++ i) {
		led_config_t* cfg = &g_kmod_data.leds[i].config;
		rc = setup_led_blink(i, cfg->enabled, cfg->period, cfg->interval);
		if (rc)
			pr_err(EK_DRV_TAG "%s: cannot setup_led_#%d blink", __func__, i);
	}
}

static const struct class_attribute g_class_attr_default = {
	.attr = {.mode = 0600 },
	.show = kmod_show_led,
	.store = kmod_store_led
};


static void init_led(int id)
{
#define LED	g_kmod_data.leds[id]
	sprintf(LED.name, "led%d", id);
	LED.config.enabled = g_kmod_data.led_default_cfg.enabled;
	LED.config.period = g_kmod_data.led_default_cfg.period;
	LED.config.interval = g_kmod_data.led_default_cfg.interval;
	LED.cattr = g_class_attr_default;
	LED.cattr.attr.name = LED.name;
#undef LED
}


static int kmod_platform_probe(struct platform_device *pldev)
{
	int 	rc = 0,
		rct = 0; /* return code temporary */

	struct fwnode_handle *child = NULL;
	int ichild = 0;
	struct device_node *devnode = NULL;

	FNIN;

	if(pldev == NULL) {
		pr_err(EK_DRV_TAG "platform_device is NULL\n");
		rc = -ENODEV;
		goto EXIT;
	}

	g_kmod_data.nleds = device_get_child_node_count(&pldev->dev);
	if (g_kmod_data.nleds == 0) {
		pr_err(EK_DRV_TAG "device_get_child_node_count is 0\n");
		rc = -ENODEV;
		goto EXIT;
	}
	pr_warn(EK_DRV_TAG "%d leds found\n", g_kmod_data.nleds);

	g_kmod_data.leds = vzalloc(g_kmod_data.nleds * sizeof (led_t));
	if (IS_ERR(g_kmod_data.leds)) {
		pr_err(EK_DRV_TAG "cannot allocate memory for detected leds\n");
		rc = -ENOMEM;
		goto EXIT;
	}

#define LED	g_kmod_data.leds[ichild]
	device_for_each_child_node(&pldev->dev, child) {

		LED.gpiodesc = devm_get_gpiod_from_child(&pldev->dev, NULL, child);
		if (IS_ERR(LED.gpiodesc)) {
			fwnode_handle_put(child);
			pr_err(EK_DRV_TAG "gpio_desc is NULL for #%d\n", ichild);
			rc = PTR_ERR(LED.gpiodesc);
			goto EXIT_FREE;
		}

		devnode = to_of_node(child);

		if (fwnode_property_present(child, "label")) {
			pr_info(EK_DRV_TAG "label fwnode_property_present");
			rct = fwnode_property_read_string(child, "label", &LED.label);
			if (rct) {
				fwnode_handle_put(child);
				pr_err(EK_DRV_TAG "cannot set led label from devnode for #%d\n", ichild);
				rc = -EINVAL;
				goto EXIT_FREE;
			}
			pr_info(EK_DRV_TAG "label is %s", LED.label);
		} else {
			pr_info(EK_DRV_TAG "no label fwnode_property_present");
			if (IS_ENABLED(CONFIG_OF) && !LED.label && devnode) {
				pr_info(EK_DRV_TAG "IS_ENABLED(CONFIG_OF) ... && devnode condition");
				LED.label = devnode->name;
			}

			if (LED.label == NULL) {
				fwnode_handle_put(child);
				pr_err(EK_DRV_TAG "cannot set led label from devnode for #%d\n", ichild);
				rc = -EINVAL;
				goto EXIT_FREE;
			}
			pr_info(EK_DRV_TAG "name got from devnode: %s", LED.label);
		}

		init_led(ichild);

		pr_info(EK_DRV_TAG "led #%d config: enabled %d, period %u, interval %u",
			ichild, LED.config.enabled, LED.config.period, LED.config.interval);

		++ ichild;
	} /* device_for_each_child_node */
#undef LED


	rc = create_led_timers();
	if (rc) {
		pr_err(EK_DRV_TAG "cannot create led timers, %d", rc);
		goto EXIT_FREE;
	}

	rc = create_led_class();
	if (rc) {
		pr_err(EK_DRV_TAG "cannot create led class, %d", rc);
		remove_led_timers();
		goto EXIT_FREE;
	}


	update_led_state();


EXIT_FREE:
	if (rc)
		vfree(g_kmod_data.leds);
EXIT:
	FNOUTRC;
	return rc;
}


static int kmod_platform_remove(struct platform_device *pdev)
{
	int i, rc;

	FNIN;

	remove_led_timers();

	#define CGPIODESC_ g_kmod_data.leds[i].gpiodesc /* current gpio_desc */
	for(i = 0; i < g_kmod_data.nleds; ++ i) {
		rc = gpiod_direction_output(CGPIODESC_, 0);
		pr_info(EK_DRV_TAG "gpiod_direction_output rc %d", rc);
		gpiod_put(CGPIODESC_);
	}
	#undef CGPIODESC_

	remove_led_class();

	vfree(g_kmod_data.leds);

	FNOUTRC;
	return rc;
}


static struct platform_driver kmod_platform_driver = {
	.driver = {
		.name = KMOD_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_match_leds)
	},
	.probe =  kmod_platform_probe,
	.remove = kmod_platform_remove
};


static int __init kmod_init(void)
{
	int rc = 0;
	FNIN;

	rc = platform_driver_register(&kmod_platform_driver);
	if (rc)
		pr_err(EK_DRV_TAG "cannot register platform driver\n");

	FNOUTRC;
	return rc;
}


static void __exit kmod_exit(void)
{
	FNIN;

	platform_driver_unregister(&kmod_platform_driver);

	FNOUT;
}


module_init(kmod_init);
module_exit(kmod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eugene Kozlov");
MODULE_DESCRIPTION("Simple bbb led-managing driver");
