#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>

#define DRIVER_AUTHOR  "MIS"
#define DRIVER_DESC "Nameprinting module"

static const struct of_device_id misprint_of_match[] = {
  { .compatible = "ms,test-module", },
  {}
};

static int testmod_probe(struct platform_device *pdev)
{
    struct device *mydev;
    const struct of_device_id *matched_id; 
    const char *str;
    if ( pdev == NULL)
    {
	return -EINVAL;
    }

    mydev = &pdev->dev;
    dev_info(mydev,"Loaded");

    if ((matched_id = of_match_device(of_match_ptr(misprint_of_match),mydev)) == NULL )
    {
	dev_err(mydev,"No match for device in DT");
	return -EINVAL;
    }

    if ( of_property_read_string(pdev->dev.of_node,"string-property",&str) == 0)
    {
	dev_info(mydev,"String read : %s\n",str);
    }
    else
    {
	dev_err(mydev,"String read error");
    }
    
    return 0;
}


static int testmod_remove(struct platform_device *pdev)
{
    struct device *mydev;
    if ( pdev != NULL )
    {
	mydev = &pdev->dev;
    }
    dev_info(mydev,"Unloading");
    return 0;
    
}


static struct platform_driver testmod_device =
{
	.probe = testmod_probe,
	.remove = testmod_remove,
	.driver = {
	    .name = "test-module",
	    .owner = THIS_MODULE,
	    .of_match_table = of_match_ptr(misprint_of_match)
         }
};



static int __init mod_init(void)
{
    return platform_driver_register(&testmod_device);
}

static void __exit mod_exit(void)
{
    platform_driver_unregister(&testmod_device);
}


module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_DEVICE_TABLE(of, misprint_of_match);

