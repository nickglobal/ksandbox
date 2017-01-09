#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/user.h>
#include <asm-generic/errno-base.h>
#include <linux/device.h>

#include "training_leds.h"

#define DRIVER_LIC		"GPL"
#define DRIVER_AUTHOR	"Vladimir Svistelnikov <svistelnikoff@gmail.com>"
#define DRIVER_DESC		"Simple DTS tree parser"
#define DRIVER_NAME		"GL BBB Driver"

/* LinuxWS/module_dbg/KERNEL/arch/arm/boot/dts/am335x-bone-common.dtsi */

static int number;
module_param(number, int, 0644);
MODULE_PARM_DESC(number, "Number to be printed on startup");

static int module_probe(struct platform_device *pdev);
static int module_remove(struct platform_device *pdev);

static const struct of_device_id glbbb_driver_match[] = {
		{ .compatible = "gl,trainings", },
		{ },
};
MODULE_DEVICE_TABLE(of, glbbb_driver_match);

static struct platform_driver glbbb_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = glbbb_driver_match,
	},
	.probe = module_probe,
	.remove = module_remove,
};
/* module_platform_driver() - Helper macro for drivers that don't do
 * anything special in module init/exit.  This eliminates a lot of
 * boilerplate.  Each module may only use this macro once, and
 * calling it replaces module_init() and module_exit()
 */
/* use this or __init/__exit scheme */
/* module_platform_driver(glbbb_driver); */

static int module_probe(struct platform_device *pdev)
{
	const void *matched;
	struct device *dev = &pdev->dev;
	struct device_node *child;
	const char *string;
	int child_count;

	matched = of_match_device(of_match_ptr(glbbb_driver_match), dev);
	if (matched == NULL) {
		dev_err(dev, "of_match_device() failed");
		return -EINVAL;
	}

	dev_info(dev, "practice_1 module loaded at 0x%p\r\n", module_probe);
	if (number != 0)
		dev_info(dev, "number value initialized during insmod -> %d",
				 number);

	if (dev->of_node != NULL) {
		of_property_read_string(dev->of_node, "string-property",
								&string);
		dev_info(dev, "/string-property value -> %s", string);
	}

	child_count = of_get_available_child_count(dev->of_node);
	dev_info(dev, "child_count -> %d", child_count);

	child = of_get_next_available_child(dev->of_node, NULL);

	if (child != NULL &&
		of_property_match_string(child, "compatible", "gpio-leds") >= 0) {

		int leds_count = of_get_child_count(child);

		dev_info(dev, "LEDs count = %d", leds_count);

		init_training_leds(pdev);
	}

	return 0;
}

static int module_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	deinit_training_leds(pdev);

	dev_info(dev, "practice_1 module removed");
	return 0;
}


static int __init glbbb_driver_init(void)
{
	/* Registering with kernel */
	return platform_driver_register(&glbbb_driver);
}

static void __exit glbbb_driver_exit(void)
{
	/* Unregistering with kernel */
	platform_driver_unregister(&glbbb_driver);
}

module_init(glbbb_driver_init);
module_exit(glbbb_driver_exit);

MODULE_LICENSE(DRIVER_LIC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
