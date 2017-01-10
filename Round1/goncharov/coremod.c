#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/of_device.h>

//------------------------------------------------------------------------------
#define DRIVER_AUTHOR  "Stanislav Goncharov"
#define DRIVER_DESC    "A Round1 Test Driver"

//------------------------------------------------------------------------------
static const struct of_device_id sgobbb_of_match[] = {
	{ .compatible = "sgo,round1_test"},
	{}
};
MODULE_DEVICE_TABLE(of, sgobbb_of_match);

//------------------------------------------------------------------------------
static int sgobbb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	const char *str = NULL;
	int err;

	match = of_match_device(of_match_ptr(sgobbb_of_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	err = of_property_read_string(pdev->dev.of_node, "string-property",
									&str);
	if (err >= 0) {
		dev_info(dev, "read str property: \"%s\"\n", str);
	}
	else {
		dev_err(dev, "fail read str property\n");
	}
	return 0;
}

//------------------------------------------------------------------------------
static int sgobbb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	dev_info(dev, "remove...\n");
	return 0;
}

//------------------------------------------------------------------------------
static struct platform_driver sgobbb_driver = {
	.probe		= sgobbb_probe,
	.remove		= sgobbb_remove,
	.driver		= {
		.name	= "sgobbb",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(sgobbb_of_match),
	},
};

//------------------------------------------------------------------------------
module_platform_driver(sgobbb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

//------------------------------------------------------------------------------

