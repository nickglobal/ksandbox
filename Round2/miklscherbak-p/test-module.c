#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <linux/gpio/consumer.h>

#define DRIVER_AUTHOR  "MIS"
#define DRIVER_DESC "Ledblinking module"
#define CLASS_NAME "MisLed"

struct led_config
{
	const char *Label;
	uint32_t On;
	uint32_t Off;
	struct gpio_desc *gpiod;
	struct timer_list timer;
	bool OnState;
	struct class_attribute control_class;

};

struct leds_table_t
{
	unsigned num_leds;
	struct class *control_class;
	struct led_config leds[];
};

static const struct of_device_id misprint_of_match[] = {
  { .compatible = "ms,test-module" },
  {}
};


static struct leds_table_t *Leds;
static struct device *mydev;

static void Start_Interval_and_control_led(struct led_config *led)
{
	int next = (led->OnState) ? led->On : led->Off;
	gpiod_direction_output(led->gpiod,led->OnState);
	mod_timer(&led->timer,jiffies+msecs_to_jiffies(next));
}

static void timer_handle(unsigned long param)
{
	struct led_config *Curr = (struct led_config *)param;
	Curr->OnState = !Curr->OnState;
	Start_Interval_and_control_led(Curr);
}

static void Start_Timers(struct leds_table_t *Leds)
{
	int i;
	for (i = 0; i < Leds->num_leds; i++)
	{
		struct led_config *Curr = &(Leds->leds[i]);
		Curr->OnState = true;
		setup_timer(&(Curr->timer),timer_handle,(unsigned long)Curr);
		Start_Interval_and_control_led(Curr);
	}
}

static ssize_t led_sys_show(struct class *class, struct class_attribute *attr, char *buf)
{
        int size;
        struct led_config *Curr = container_of(attr, struct led_config, control_class);

        size = sprintf(buf, "%d %d\n", Curr->On, Curr->Off);
        return size;
}

static ssize_t led_sys_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
        int ret;
        int On;
		int Off;
        struct led_config *Curr = container_of(attr, struct led_config, control_class);

        ret = sscanf(buf, "%d %d", &On, &Off);
        if (ret != 2)
        {
        	return 0;
        }

        Curr->On = On;
        Curr->Off = Off;

        return count;
}

static int Register_Leds_SysFs(struct platform_device *pdev,struct leds_table_t *Leds)
{
	int i;
	int RetVal;
	Leds->control_class = class_create(THIS_MODULE,CLASS_NAME);
	if (IS_ERR(Leds->control_class))
	{
		return PTR_ERR(Leds->control_class);
	}
	for ( i = 0; i < Leds->num_leds; i++)
	{
		Leds->leds[i].control_class.attr.name = Leds->leds[i].Label;
		Leds->leds[i].control_class.attr.mode = 0666;
		Leds->leds[i].control_class.show = &led_sys_show;
		Leds->leds[i].control_class.store = &led_sys_store;

		RetVal = class_create_file(Leds->control_class, &(Leds->leds[i].control_class));
		if (IS_ERR_VALUE(RetVal)) {
			class_destroy(Leds->control_class);
			return RetVal;
		}
	}
	return 0;
}

static int testmod_probe(struct platform_device *pdev)
{

    const struct of_device_id *matched_id; 

    int Count;
    int RetVal;
    struct fwnode_handle *Child;

    const char *str;
    if ( pdev == NULL)
    {
	return -EINVAL;
    }

    mydev = &pdev->dev;
    dev_info(mydev,"Loaded");

    if ((matched_id = of_match_device(of_match_ptr(misprint_of_match),mydev)) == NULL )
    {
	dev_err(mydev,"No match for device in DT");
	return -EINVAL;
    }


    if ( of_property_read_string(pdev->dev.of_node,"test-string",&str) == 0)
    {
	dev_info(mydev,"String read : %s\n",str);
    }
    else
    {
	dev_err(mydev,"String read error");
    }


    Count = device_get_child_node_count(mydev);
    dev_info(mydev,"Childern %d\n",Count);
    Leds = devm_kzalloc(mydev,sizeof(struct led_config) * Count + sizeof(struct leds_table_t), GFP_KERNEL);
    if ( Leds == NULL )
    {
    	dev_err(mydev,"Memory allocation error\n");
    	return -ENOMEM;
    }
    device_for_each_child_node(mydev,Child)
    {
    	struct led_config *Currled = &(Leds->leds[Leds->num_leds]);
    	Currled->Label="Ledx";
    	Currled->On = 500;
    	Currled->Off = 500;
    	Currled->gpiod = devm_get_gpiod_from_child(mydev, NULL , Child);
    	if (IS_ERR( Currled->gpiod ) )
    	{
    		dev_err(mydev,"Invalid gpio desc %ld\n",PTR_ERR(Currled->gpiod));
    		return -ENODEV;
    	}

    	if (fwnode_property_present(Child,"on") )
    	{
    		fwnode_property_read_u32(Child,"on",&Currled->On);
    	}

    	if (fwnode_property_present(Child,"off") )
    	{
    		fwnode_property_read_u32(Child,"off",&Currled->Off);
    	}

    	if (fwnode_property_present(Child,"label") )
    	{
    		fwnode_property_read_string(Child,"label",&Currled->Label);
    	}
    	dev_info(mydev,"Led %s On = %d Off = %d\n",Currled->Label,Currled->On,Currled->Off);
    	Leds->num_leds++;
    	gpiod_direction_output(Currled->gpiod,0);
    }
    RetVal = Register_Leds_SysFs(pdev,Leds);
    if ( IS_ERR_VALUE(RetVal))
    {
    	return RetVal;
    }
    Start_Timers(Leds);

    return 0;
}

static void Timers_Release(struct leds_table_t *Leds)
{
	int i;
	for ( i = 0; i < Leds->num_leds; i++)
	{
		struct led_config *Curr = &Leds->leds[i];
		del_timer_sync(&Curr->timer);
	}
}

static void Sysfs_Unregister(struct leds_table_t *Leds)
{
	int i;
	for ( i = 0; i < Leds->num_leds; i++)
	{
		struct led_config *Curr = &Leds->leds[i];
		class_remove_file(Leds->control_class, &Curr->control_class);
	}
	class_destroy(Leds->control_class);
}

static int testmod_remove(struct platform_device *pdev)
{
	struct device *mydev;
	if ( pdev != NULL )
	{
		mydev = &pdev->dev;
	}
	dev_info(mydev,"Unloading\n");
	Timers_Release(Leds);
	dev_info(mydev,"Timers released\n");
	Sysfs_Unregister(Leds);
	dev_info(mydev,"Sysfs class and files destroy\n");
	return 0;
    
}


static struct platform_driver testmod_device =
{
	.probe = testmod_probe,
	.remove = testmod_remove,
	.driver = {
	    .name = "test-module",
	    .owner = THIS_MODULE,
	    .of_match_table = of_match_ptr(misprint_of_match)
         }
};



static int __init mod_init(void)
{
    return platform_driver_register(&testmod_device);
}

static void __exit mod_exit(void)
{
    platform_driver_unregister(&testmod_device);
}


module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_DEVICE_TABLE(of, misprint_of_match);

