#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/of.h>
#include <linux/of_device.h>

#define DRIVER_AUTHOR  "VADYMM>"
#define DRIVER_DESC "A simple driver."

static struct of_device_id of_gl_heartbeat_match[] = {
	{
		.compatible = "gl,console-heartbeat",
		.data = "Hello from coremod.ko!",
	}, { /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, of_gl_heartbeat_match);

static int __init mod_init(void)
{
	printk(KERN_DEBUG "glbbb mod_init\n");
	return 0;
}

static void __exit mod_exit(void)
{
	printk(KERN_DEBUG "glbbb mod_exit\n");
}

static int glbbb_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id = of_match_device(of_gl_heartbeat_match, &pdev->dev);
	if (of_id) {
		/* Use of_id->data here */
		printk(KERN_DEBUG "%s\n", (const char *) of_id->data);
	}
	return 0;
}

static int glbbb_remove(struct platform_device *pdev) {
	printk(KERN_DEBUG "glbbb glbbb_remove\n");
	return 0;
}

static struct platform_driver glbbb_driver = {
	.probe = glbbb_probe,
	.remove = glbbb_remove,
	.driver = {
	.name = "glbbb",
	.of_match_table = of_gl_heartbeat_match
	}
};

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

