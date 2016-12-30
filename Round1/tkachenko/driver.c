#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DRIVER_AUTHOR  "O.Tkachenko"
#define DRIVER_DESC "A simple driver."

static int my_probe(struct platform_device *pdev)
{
	const char *string_prop = NULL;
	struct device_node *pnode = pdev->dev.of_node;

	if (pnode != NULL &&
		of_property_read_string(pnode,
					"string-property",
					&string_prop) == 0) {
	printk(KERN_DEBUG "string-property: %s\n", string_prop);
	}
	return 0;
}

static const struct of_device_id my_of_match[] = {
	{
		.compatible = "gl,console-heartbeat",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
	.probe = my_probe,
	.driver = {
		.name = "glbbb",
		.of_match_table = of_match_ptr(my_of_match),
		.owner = THIS_MODULE,
		},
};

module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
