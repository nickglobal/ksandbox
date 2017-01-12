#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
//#include <linux/init.h>
//#include <linux/stat.h>
//#include <linux/kernel.h>
//#include <linux/of_device.h>
//#include <linux/mod_devicetable.h>


#define DRIVER_AUTHOR  "VADYMM"
#define DRIVER_DESC    "A simple driver."

static int vadymm_probe(struct platform_device *);
static int vadymm_remove(struct platform_device *);


static int vadymm_probe(struct platform_device *pdev)
{
	const char *str = NULL;
	struct device_node *of_node = pdev->dev.of_node;

//	dev_info(dev, "vadymm_probe()");

	if (!of_node)
		return -EFAULT;

	// The out_string pointer is modified only if a valid string can be decoded
	of_property_read_string(of_node, "string-property", &str);

	if (str)
//		dev_info(dev, "DTS string-property = %s", str);
		printk(KERN_DEBUG "DTS string-property = %s", str);
	else
//		dev_info(dev, "string-property failed");
		printk(KERN_DEBUG "string-property failed");

	return 0;
}

static int vadymm_remove(struct platform_device *pdev) {
	printk(KERN_DEBUG "vadymm_remove\n");
	return 0;
}

static struct of_device_id of_vadymm_match[] = {
	{
		.compatible = "gl,vadymm",
	}, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_vadymm_match);

 static struct platform_driver vadymm_driver = {
	.probe = vadymm_probe,
	.remove = vadymm_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "vadymm",
		.of_match_table = of_match_ptr(of_vadymm_match)
	}
};
module_platform_driver(vadymm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
