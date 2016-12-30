#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>


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
	int err;

	match = of_match_device(of_match_ptr(pkbbb_of_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	err = of_property_read_string(pdev->dev.of_node, "string-property",
									&str);
	if (err >= 0)
		dev_info(dev, "read str property: \"%s\"\n", str);
	else
		dev_err(dev, "fail read str property\n");
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

