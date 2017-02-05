#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/leds.h>
#include <linux/gpio/consumer.h>

static int gpio_delete(void);

struct gpio_led_data {
    struct class *pt_led_class;
    struct gpio_desc *gpiod;
    const char *pc_name;
    u32 ui_work_time;
    bool b_enabled;
};

struct gpio_priv {
    int num_leds;
    struct gpio_led_data px_leds[];
};

struct gpio_priv *priv = NULL;
struct class_attribute t_class_attrs;

static void led_handler(struct gpio_led_data *px_led)
{
    if (NULL != px_led)
    {
        gpiod_direction_output(px_led->gpiod, px_led->b_enabled);
    }
}

static ssize_t sys_class_work_time_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int i = 0;
    int ret = 0;

    if (NULL != priv)
    {
        for (i = 0; i < priv->num_leds; i++)
        {
            if (priv->px_leds[i].pt_led_class == class)
            {
                ret = sprintf(buf, "%u\n", priv->px_leds[i].ui_work_time);
                break;
            }
        }
    }

    return ret;
}

static ssize_t sys_class_work_time_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = 0, i = 0;
    u32 ui_work_time = 0;

    if (NULL != priv)
    {
        for (i = 0; i < priv->num_leds; i++)
        {
            if (priv->px_leds[i].pt_led_class == class)
            {
	        ret = sscanf(buf, "%u", &ui_work_time);
	        if (ret != 1)
                {
                    printk("ERROR: wrong number of parameters to store! (%d)\n", ret);
                    return -1;
                }

	        priv->px_leds[i].ui_work_time = ui_work_time;
                break;
            }
        }
    }

    return count;
}


static int gpio_probe(struct platform_device *pdev)
{
    struct device *dev = &(pdev->dev);
    struct fwnode_handle *child;
    int i = 0, num = 0, ret = 0;

    num = device_get_child_node_count(dev);
    if (!num)
        return -ENODEV;
    else if (num > 4)
    {
        printk("ERROR: Too many leds descriptors\n");
        return -ENXIO;
    }
    
    printk("ARTEM NUM %d\n", num);

    priv = kzalloc(sizeof(struct gpio_priv) + sizeof(struct gpio_led_data) * num, GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    // set sys class attributes
    t_class_attrs.attr.name = "work-time";
    t_class_attrs.attr.mode = 0664;
    t_class_attrs.show = &sys_class_work_time_show;
    t_class_attrs.store = &sys_class_work_time_store;

    device_for_each_child_node(dev, child)
    {
        struct gpio_led_data *px_curr_led = &(priv->px_leds[i]);
        int i_op_ret = 0;

        i_op_ret = fwnode_property_read_string(child, "label", &(px_curr_led->pc_name));
        if (0 != i_op_ret)
        {
            printk("ERROR: CANNOT READ LABEL PROPERTY FOR LED %d: RET %d\n", i, i_op_ret);
            gpio_delete();
            return i_op_ret;
        }
printk("ARTEM: LABEL %s FOR LED %d -> %x\n", px_curr_led->pc_name, i, (unsigned int)px_curr_led);

        i_op_ret = fwnode_property_read_u32(child, "work-time", &px_curr_led->ui_work_time);
        if (0 != i_op_ret)
        {
            printk("ERROR: CANNOT READ WORK TIME PROPERTY FOR LED %d: RET %d\n", i, i_op_ret);
            gpio_delete();
            return i_op_ret;
        }
        px_curr_led->b_enabled = px_curr_led->ui_work_time > 0 ? true : false;

        px_curr_led->gpiod = devm_get_gpiod_from_child(dev, NULL, child);
        if (IS_ERR(px_curr_led->gpiod))
        {
            printk("ERROR: devm_get_gpiod_from_child() FOR LED %d: %d\n", i, (int)px_curr_led->gpiod);
            gpio_delete();
            return i_op_ret;
        }

        px_curr_led->pt_led_class = class_create(THIS_MODULE, px_curr_led->pc_name);
        if (IS_ERR(px_curr_led->pt_led_class))
        {
            printk("ERROR: class_create() FOR %s\n", px_curr_led->pc_name);
            gpio_delete();
            return PTR_ERR(px_curr_led->pt_led_class);
        }
printk("ARTEM: CLASS %x LED %s\n", (unsigned int)px_curr_led->pt_led_class, px_curr_led->pc_name);
	ret = class_create_file(px_curr_led->pt_led_class, &t_class_attrs);
	if (ret < 0)
        {
            printk("ERROR: class_create_file() FOR %s: %d\n", px_curr_led->pc_name, ret);
            gpio_delete();
            return ret;
        }

        i++;
    }
    priv->num_leds = i;

    for (i = 0; i < priv->num_leds; i++)
    {
       led_handler(&priv->px_leds[i]);
    }

    platform_set_drvdata(pdev, priv);

    return ret;
}

static void delete_gpio_led(struct gpio_led_data *px_led)
{
    class_remove_file(px_led->pt_led_class, &t_class_attrs);
    class_destroy(px_led->pt_led_class);
}

static int gpio_delete(void)
{
    int i;

    if (priv)
    {
	for (i = 0; i < priv->num_leds; i++)
	    delete_gpio_led(&priv->px_leds[i]);

        kfree(priv);
    }

    return 0;
}

static const struct of_device_id gpio_of_match[] = {
	{ .compatible = "StAndArt,dummy_mod", },
	{ },
};

static struct platform_driver gpio_driver = {
	.driver		= {
		.name	= "round2_bbb",
		.of_match_table = gpio_of_match,
	},
	.probe		= gpio_probe,
};

static int __init gpio_init(void)
{
    return platform_driver_register(&gpio_driver);
}

static void __exit gpio_exit(void)
{
    gpio_delete();
    platform_driver_unregister(&gpio_driver);
}

module_init( gpio_init );
module_exit( gpio_exit );

MODULE_DESCRIPTION("TEST BBB DRIVER - ROUND 2: GPIO");
MODULE_AUTHOR("ARTEM ANDREEV");
MODULE_LICENSE("GPL v2");

