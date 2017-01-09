#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

static int dev_tree_probe(struct platform_device *pdev)
{
    const char *pc_text = NULL;
    int i_result = 0, i_op_err = 0;

    if (!pdev)
    {
        printk("pdev pointer is NULL, error\n");
        return -EFAULT;
    }

    i_op_err = of_property_read_string(pdev->dev.of_node, "string-property", &pc_text);
    if (0 <= i_op_err)
    {
        printk("Test string from dev tree is %s\n", pc_text);
    }
    else
    {
        printk("Test string reading error: %d\n", i_op_err);
        i_result = i_op_err;
    }

    return i_result;
}

static const struct of_device_id dev_tree_of_match[] = {
	{ .compatible = "gl,console-heartbeat", },
	{ },
};

static struct platform_driver dev_tree_driver = {
	.driver		= {
		.name	= "glbbb",
		.of_match_table = dev_tree_of_match,
	},
	.probe		= dev_tree_probe,
};

static int __init dev_tree_init(void)
{
    printk("INIT module\n");
    return platform_driver_register(&dev_tree_driver);
}

static void __exit dev_tree_exit(void)
{
    printk("EXIT module\n");
    platform_driver_unregister(&dev_tree_driver);
}

module_init( dev_tree_init );
module_exit( dev_tree_exit );

MODULE_DESCRIPTION("TEST BBB DRIVER - ROUND 1: READ STRING PROPERTY TEXT");
MODULE_AUTHOR("ARTEM ANDREEV");
MODULE_LICENSE("GPL v2");

