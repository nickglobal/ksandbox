#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DRIVER_AUTHOR  "SanB (O.Bobro)"
#define DRIVER_DESC "Test-san_mod.ko"

static int san_probe(struct platform_device *);
static int san_remove(struct platform_device *);

//static char *myName = NULL;


static int san_probe(struct platform_device *pdev)
{
	const char *name_prop = NULL;
	int ret;

	struct device_node *pDevNode = pdev->dev.of_node;
	
	if (unlikely(!pDevNode))
	{
		printk(KERN_ERR "Error: pDevNode is NULL\n");
		return -EINVAL;
	}

	ret = of_property_read_string(pDevNode, "myName_property", &name_prop);
	if (likely(!ret))
	{
		printk(KERN_DEBUG "myName_property is: %s\n", name_prop);
		// For some testing:
		// int prop_len = 0;
		// prop_len = strlen(name_prop);
		// myName = vmalloc(prop_len+1);
		// strcpy(myName, name_prop);
		// printk(KERN_DEBUG "@@@myName = [%s]\n", myName);
	}
	return 0;
}

static int san_remove(struct platform_device *pdev)
{
	// if ( myName ) 
	// {
	// 	printk(KERN_DEBUG "Bye, %s!\n", myName );
	//     vfree(myName);
	// }
	return 0;
}

static const struct of_device_id  ofDeviceId[] = {
	{
		.compatible = "BBB_SanB,BBB_SanB_MyName",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, ofDeviceId);

static struct platform_driver  platformDriver = {
	.probe = san_probe,
	.remove = san_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "SanB_Name_node",
		.of_match_table = of_match_ptr(ofDeviceId),
		},
};
module_platform_driver(platformDriver);



MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
