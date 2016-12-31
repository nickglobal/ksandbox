#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>


static const struct of_device_id pkbbb_of_match[] = {
	{ .compatible = "pk,echo-name", },
	{}
};

MODULE_DEVICE_TABLE(of, pkbbb_of_match);

static int pkbbb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	const char *str = NULL;
	struct gpio_desc *gpiod;
	int err;
	struct fwnode_handle *child;

	match = of_match_device(of_match_ptr(pkbbb_of_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	/* read str property fro device tree */
	err = of_property_read_string(pdev->dev.of_node, "string-property",
									&str);
	if (err >= 0)
		dev_info(dev, "read str property: \"%s\"\n", str);
	else
		dev_err(dev, "fail read str property\n");


	/* init LED */
	device_for_each_child_node(dev, child) {
		gpiod = devm_get_gpiod_from_child(dev, NULL, child);
		if (IS_ERR(gpiod)) {
			dev_err(dev, "fail devm_get_gpiod_from_child()\n");
			return PTR_ERR(gpiod);
		}

		err = gpiod_direction_output(gpiod, 1);
		if (err < 0) {
			dev_err(dev, "fail gpiod_direction_output()\n");
			return err;
		}
	}

	return 0;
}

static int pkbbb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "remove...\n");
	return 0;
}

static struct platform_driver pkbbb_driver = {
	.probe		= pkbbb_probe,
	.remove		= pkbbb_remove,
	.driver		= {
		.name	= "pkbbb",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(pkbbb_of_match),
	},
};

static int __init mod_init(void)
{
	return platform_driver_register(&pkbbb_driver);
}
module_init(mod_init);

static void __exit mod_exit(void)
{
	platform_driver_unregister(&pkbbb_driver);
}
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Kulakov");
MODULE_DESCRIPTION("A simple Linux driver");

