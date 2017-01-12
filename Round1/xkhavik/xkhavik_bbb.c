#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/kernel.h>

#define DRIVER_AUTHOR  "Viktor Khabalevskyi"
#define DRIVER_DESC "Test driver"
#define DRIVER_NAME "xkhavik_bbb"
#define DRIVER_COMPATIBLE "gl,console-heartbeat"

static const struct of_device_id xkhavik_of_match[] = {
	{.compatible = DRIVER_COMPATIBLE,},
	{ },
};

MODULE_DEVICE_TABLE(of, xkhavik_of_match);

static int xkhavik_probe(struct platform_device *pdev)
{
	const char         *str_property;
	struct device      *dev         = &pdev->dev;
	struct device_node *pnode        = dev->of_node;

	if (dev == NULL)
		return -ENODEV;

	if (pnode == NULL) {
		dev_err(dev, "There is not such device node");
		return -EINVAL;
	}

	dev_info(dev, "started prob module");

	if (of_property_read_string(pnode, "string-property",
					&str_property) == 0) {
		dev_info(dev, "string-property: %s\n", str_property);
	} else {
		dev_err(dev, "Error of reading string-property");
		return -EINVAL;
	}
	return 0;
}

static int xkhavik_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove module");
	return 0;
}

static struct platform_driver xkhavik_driver = {
	.probe = xkhavik_probe,
	.remove = xkhavik_remove,
	.driver = {
			.name = DRIVER_NAME,
			.of_match_table = of_match_ptr(xkhavik_of_match),
			.owner = THIS_MODULE,
		  },
};
module_platform_driver(xkhavik_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
