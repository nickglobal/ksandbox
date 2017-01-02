#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DRIVER_AUTHOR "Alina Minska"
#define DRIVER_DESC "Read bbb's dts and parse string"


int minsali_probe(struct platform_device *pdev)
{
	const char *result = NULL;
	struct device_node *of_node = pdev->dev.of_node;

	if (NULL == of_node)
		return -EFAULT;

	printk(KERN_DEBUG"minsali: Going to receive string from dts\n");
	of_property_read_string(of_node, "string-property", &result);
	if (NULL != result)
		printk(KERN_INFO"minsali: my string from dts: %s\n", result);
	else
		printk(KERN_ERR"minsali: failed to get string from dts\n");

	return 0;
}

int minsali_remove(struct platform_device *pdev)
{
	printk(KERN_DEBUG"minsali: onRemove");
	return 0;
}

static const struct of_device_id minsali_devids[] = {
	{.compatible = "gl,minsali"},
	{}
};
MODULE_DEVICE_TABLE(of, minsali_devids);

static struct platform_driver minsali_pd = {
	.probe = minsali_probe,
	.remove = minsali_remove,
	.driver = {
		.name = "minsali",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(minsali_devids),
	}
};
module_platform_driver(minsali_pd)

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
