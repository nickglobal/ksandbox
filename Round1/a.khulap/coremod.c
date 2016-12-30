#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>


#define DRIVER_AUTHOR	"an1kh"
#define DRIVER_DESC	"A simple driver to test device tree parameters."


static int dev_probe(struct platform_device *pdev);
static int dev_remove(struct platform_device *pdev);


static const struct of_device_id dev_of_match[] = {
	{ .compatible = "an1kh,console-dtbparamtest", },
	{}
};

static struct platform_driver dev_driver = {
	.probe	= dev_probe,
	.remove	= dev_remove,
	.driver	= {
		.name	= "an1khbbb",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dev_of_match),
	},
};


static int dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *pnode = pdev->dev.of_node;
	const char *str = NULL;
	int ret = 0;

	dev_info(dev, "dev_probe()");

	if (of_property_read_string(pnode, "string-property", &str) == 0) {
		dev_info(dev, "string-property = %s\n", str);
	} else {
		dev_err(dev, "failed to read sting-property");
		ret = -EINVAL;
	}

	return ret;
}

static int dev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "dev_remove()");

	return 0;
}


static int __init mod_init(void)
{
	return platform_driver_register(&dev_driver);
}

static void __exit mod_exit(void)
{
	platform_driver_unregister(&dev_driver);
}


module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
