#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/stringify.h>
//#include <linux/delay.h>
//#include <linux/leds.h>
#include <linux/spinlock.h>


#define DRIVER_AUTHOR  "SanB (O.Bobro)"
#define DRIVER_DESC "Test-san_leds.ko"
#define CLASS_NAME "sanB_leds"


#define ATTR_ON_OFF_NAME 		"timeOnOff"
#define ATTR_FREQUENCY_NAME 	"frequency"

#define LED_OFF	0
#define LED_ON	1

typedef struct {
	struct gpio_desc *gpiod;
	union {
		struct {
			unsigned int timeOff;
			unsigned int timeOn;
		};
		unsigned int states[2];
	};
	struct timer_list timer;
	spinlock_t spinLock;
	u8	currentState;
	struct class_attribute	classAttrOnOff;
	struct class_attribute	classAttrFreq;
} SANB_LEDS;
//__attribute__ ((__packed__));

static ssize_t timeOnOff_show(struct class *, struct class_attribute *, char *);
static ssize_t timeOnOff_store(struct class *, struct class_attribute *, const char *, size_t );
static ssize_t frequency_show(struct class *, struct class_attribute *, char *);
static ssize_t frequency_store(struct class *, struct class_attribute *, const char *, size_t );

static void reRunTimer(SANB_LEDS *);
static void timerCallback( unsigned long );

static int san_led_probe(struct platform_device *);
static int san_led_remove(struct platform_device *);

static struct class *sanB_leds_class;
static int	numOfLeds;

static ssize_t timeOnOff_show(struct class *class, struct class_attribute *classAttr, char *buf)
{
	int size;
	SANB_LEDS *led;

	printk(KERN_INFO "@@@ timeOnOff_show(): read from [%s]\n", classAttr->attr.name);

	led = container_of(classAttr, SANB_LEDS, classAttrOnOff);

	size = sprintf(buf, "%u:%u\n", led->timeOn, led->timeOff);
	return size;
}

static ssize_t timeOnOff_store(struct class *class, struct class_attribute *classAttr, const char *buf, size_t count)
{
	int ret;
	unsigned int timeOn, timeOff;
	SANB_LEDS *led;
	led = container_of(classAttr, SANB_LEDS, classAttrOnOff);

	printk(KERN_INFO "@@@ timeOnOff_store(): store to [%s], buf = [%s], count = %u\n",
							classAttr->attr.name, buf, count);

	ret = sscanf(buf, "%u:%u", &timeOn, &timeOff);
	if (unlikely(ret != 2))
	{
		printk(KERN_ERR "@@@ classAttr->attr.name = [%s]: can not recognize the string \"%s\"\n",
					classAttr->attr.name, buf);
		return 0;
	}

	led->timeOn = timeOn;
	led->timeOff = timeOff;
	reRunTimer(led);
//	printk(KERN_INFO "msecs_to_jiffies(timeOn)=%lu, msecs_to_jiffies(timeOff)=%lu\n",
//			msecs_to_jiffies(timeOn), msecs_to_jiffies(timeOff));
	return count;
}


static ssize_t frequency_show(struct class *class, struct class_attribute *classAttr, char *buf)
{
	int size;
	SANB_LEDS *led;
	unsigned int f, fraction;

	printk(KERN_INFO "@@@ frequency_show(): read from [%s]\n", classAttr->attr.name);

	led = container_of(classAttr, SANB_LEDS, classAttrFreq);
	f = 1000/(led->timeOn + led->timeOff);
	fraction = 1000%(led->timeOn + led->timeOff)*1000/(led->timeOn + led->timeOff);
	size = sprintf(buf, (fraction!=0 ? "~%u.%.3u\n" : "%u\n"), f, fraction );
	return size;
}

static ssize_t frequency_store(struct class *class, struct class_attribute *classAttr, const char *buf, size_t count)
{
	int ret;
	unsigned int frequency;
	SANB_LEDS *led;
	led = container_of(classAttr, SANB_LEDS, classAttrFreq);

	printk(KERN_INFO "@@@ frequency_store(): store to [%s], buf = [%s], count = %u\n",
							classAttr->attr.name, buf, count);

	ret = sscanf(buf, "%u", &frequency);
	if (unlikely(ret != 1))
	{
		printk(KERN_ERR "@@@ classAttr->attr.name = [%s]: can not recognize the string \"%s\"\n",
					classAttr->attr.name, buf);
		return 0;
	}

	//Set meander:
	led->timeOn = led->timeOff = 500 / frequency; //( 1000 / frequency ) / 2;
	reRunTimer(led);

	return count;
}

static void reRunTimer(SANB_LEDS *led)
{
	//del_timer_sync(&led->timer);
	timerCallback((unsigned long)led);
}

static void timerCallback( unsigned long data )
{
	SANB_LEDS *currLed = (SANB_LEDS*)data;

	spin_lock_bh(&currLed->spinLock);
	//led->lastState = led->lastState==LED_ON ? LED_OFF : LED_ON;
	currLed->currentState = currLed->currentState ^ LED_ON;
	if (likely( msecs_to_jiffies(currLed->states[currLed->currentState]) > 0))
	{
		gpiod_set_value(currLed->gpiod, currLed->currentState);
		mod_timer(&(currLed->timer), jiffies + msecs_to_jiffies(currLed->states[currLed->currentState]));
	}
	else
	{
		currLed->currentState = currLed->currentState ^ LED_ON; //return to previous state.
		if (msecs_to_jiffies(currLed->states[currLed->currentState]) == 0)
		{
			gpiod_set_value(currLed->gpiod, LED_OFF);
			currLed->currentState = LED_OFF;
		}
	}
	spin_unlock_bh(&currLed->spinLock);
}


static int san_led_probe(struct platform_device *pdev)
{
	long ret;
	SANB_LEDS *leds;
	int ledsCount;
	struct fwnode_handle *ledNode;
	const char *label;

	struct device *pDevice = &pdev->dev;
	if (unlikely(!pDevice))
	{
		printk(KERN_ERR "Error: pDevice is NULL\n");
		return -EINVAL;
	}

	// Detect LEDs count in Device Tree
	ledsCount = device_get_child_node_count(pDevice);
	if (unlikely(ledsCount <= 0))
	{
		dev_err(pDevice, "@@@ ERROR: device_get_child_node_count() has returned: %d\n", ledsCount);
		return -ENOENT;
	}

	dev_info(pDevice, "@@@ ledsCount = %d\n", ledsCount);

	leds = devm_kzalloc(pDevice, sizeof(SANB_LEDS) * ledsCount,	GFP_KERNEL);
	if (unlikely(!leds))
	{
		dev_err(pDevice, "@@@ ERROR: devm_kzalloc(1) has returned NULL\n");
		return -ENOMEM;
	}


	// Create class:
	sanB_leds_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(sanB_leds_class))
	{
		ret = PTR_ERR(sanB_leds_class);
		dev_err(pDevice, "@@@ ERROR: class_create("CLASS_NAME") has returned error: %ld\n", ret);
		sanB_leds_class = NULL;
		return (int)ret;
	}

	numOfLeds = 0;
	device_for_each_child_node(pDevice, ledNode)
	{
		/* Read properties */
		leds[numOfLeds].gpiod = devm_get_gpiod_from_child(pDevice, NULL, ledNode);
		if ( unlikely(IS_ERR(leds[numOfLeds].gpiod)) )
		{
			ret = PTR_ERR(leds[numOfLeds].gpiod);
			dev_err(pDevice, "@@@ ERROR: LED #%d: devm_get_gpiod_from_child() has returned error: %ld\n",
					numOfLeds, ret );
			goto RESTORE_SYSFS_1;
		}

		fwnode_property_read_u32(ledNode, "timeOn", &leds[numOfLeds].timeOn);
		fwnode_property_read_u32(ledNode, "timeOff", &leds[numOfLeds].timeOff);
		fwnode_property_read_string(ledNode, "label", &label);

		leds[numOfLeds].classAttrOnOff.attr.name = devm_kzalloc(pDevice,
							strlen(label)*2 + sizeof(ATTR_ON_OFF_NAME) + 2 + sizeof(ATTR_FREQUENCY_NAME) + 2,
							GFP_KERNEL);
		if (unlikely( !(leds[numOfLeds].classAttrOnOff.attr.name) ))
		{
			dev_err(pDevice, "@@@ ERROR: devm_kzalloc(2) has returned NULL\n");
			return -ENOMEM;
		}
		leds[numOfLeds].classAttrFreq.attr.name = leds[numOfLeds].classAttrOnOff.attr.name +
												strlen(label) + sizeof(ATTR_ON_OFF_NAME) + 2;

		dev_info(pDevice, "LED #%d:\tlabel = %s,\ttimeOn = [%d],\ttimeOff = [%d]\n",
				numOfLeds, label, leds[numOfLeds].timeOn, leds[numOfLeds].timeOff);

		// Create files/attributes in the CLASS_NAME directory:
		strcpy((char*)leds[numOfLeds].classAttrOnOff.attr.name, label);
		strcat((char*)leds[numOfLeds].classAttrOnOff.attr.name, "_");
		strcat((char*)leds[numOfLeds].classAttrOnOff.attr.name, ATTR_ON_OFF_NAME);
		leds[numOfLeds].classAttrOnOff.attr.mode = 0666;//VERIFY_OCTAL_PERMISSIONS(0664);
		leds[numOfLeds].classAttrOnOff.show = timeOnOff_show;
		leds[numOfLeds].classAttrOnOff.store = timeOnOff_store;

		strcpy((char*)leds[numOfLeds].classAttrFreq.attr.name, label);
		strcat((char*)leds[numOfLeds].classAttrFreq.attr.name, "_");
		strcat((char*)leds[numOfLeds].classAttrFreq.attr.name, ATTR_FREQUENCY_NAME);
		leds[numOfLeds].classAttrFreq.attr.mode = 0666;//VERIFY_OCTAL_PERMISSIONS(0664);
		leds[numOfLeds].classAttrFreq.show = frequency_show;
		leds[numOfLeds].classAttrFreq.store = frequency_store;

		ret = class_create_file(sanB_leds_class, &(leds[numOfLeds].classAttrOnOff));
		if (unlikely(IS_ERR_VALUE(ret)))
		{
			dev_err(pDevice, "@@@ ERROR: LED #%d: class_create_file(\"%s\") has returned error: %ld\n",
					numOfLeds, leds[numOfLeds].classAttrOnOff.attr.name, ret);
			goto RESTORE_SYSFS_1;
		}

		ret = class_create_file(sanB_leds_class, &(leds[numOfLeds].classAttrFreq));
		if (unlikely(IS_ERR_VALUE(ret)))
		{
			dev_err(pDevice, "@@@ ERROR: LED #%d: class_create_file(\"%s\") has returned error: %ld\n",
					numOfLeds, leds[numOfLeds].classAttrFreq.attr.name, ret);
			goto RESTORE_SYSFS_2;
		}

		// Init of the spinlock:
		spin_lock_init(&leds[numOfLeds].spinLock);

		//Init of the timer:
		leds[numOfLeds].currentState = LED_OFF;
		gpiod_direction_output(leds[numOfLeds].gpiod, LED_OFF);
		setup_timer( &leds[numOfLeds].timer, timerCallback, (unsigned long)(&leds[numOfLeds]) );
		ret = mod_timer( &leds[numOfLeds].timer, jiffies + msecs_to_jiffies(200*(numOfLeds+1)) );

		numOfLeds++;
		continue;

	RESTORE_SYSFS_2:
		class_remove_file(sanB_leds_class, &(leds[numOfLeds].classAttrOnOff));
	RESTORE_SYSFS_1:
		while (numOfLeds > 0)
		{
			class_remove_file(sanB_leds_class, &(leds[numOfLeds-1].classAttrOnOff));
			class_remove_file(sanB_leds_class, &(leds[numOfLeds-1].classAttrFreq));
			del_timer_sync(&leds[numOfLeds-1].timer);
			numOfLeds--;
		}
		if (likely(sanB_leds_class))
			class_destroy(sanB_leds_class);
		sanB_leds_class = NULL;
		return (int)ret;
	}

	platform_set_drvdata(pdev, leds);
	dev_info(pDevice, "@@@ san_led_probe() is executed successfully!\n\n");
	return 0;
}

static int san_led_remove(struct platform_device *pdev)
{
	SANB_LEDS *leds;
	struct device *pDevice = &pdev->dev;

	if (likely(sanB_leds_class))
	{
		leds = platform_get_drvdata(pdev);

		if (!leds)
			return -EFAULT;

		while (numOfLeds > 0)
		{
			dev_info(pDevice, "@@@ san_led_remove(): remove files: %s, %s\n",
								leds[numOfLeds-1].classAttrOnOff.attr.name,
								leds[numOfLeds-1].classAttrFreq.attr.name);
			class_remove_file(sanB_leds_class, &(leds[numOfLeds-1].classAttrOnOff));
			class_remove_file(sanB_leds_class, &(leds[numOfLeds-1].classAttrFreq));
			del_timer_sync(&leds[numOfLeds-1].timer);
			numOfLeds--;
		}
		class_destroy(sanB_leds_class);
		sanB_leds_class = NULL;
	}
	else
	{
		dev_info(pDevice, "@@@ san_led_remove(): sanB_leds_class is NULL.\n");
	}

	dev_info(pDevice, "@@@ san_led_remove() is executed successfully!\n\n");
	return 0;
}

static const struct of_device_id  ofDeviceId[] = {
	{
		.compatible = "BBB_SanB,BBB_SanB_leds",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, ofDeviceId);

static struct platform_driver  platformDriver = {
	.probe = san_led_probe,
	.remove = san_led_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "SanB_leds_driver",
		.of_match_table = of_match_ptr(ofDeviceId),
		},
};
module_platform_driver(platformDriver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
