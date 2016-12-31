
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/input.h>

#include <linux/stat.h>
#include <linux/fs.h>

#define DRIVER_NAME			"coremod_my"





static void get_platform_info(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const char *name;
	u32 val;

	if (np) {

	if (!of_property_read_string(np, "my_name", &name))
		dev_info(dev, "my name is %s\n", name);

	if (!of_property_read_u32(np, "tmp_var", &val))
		dev_info(dev, "tmp_var = %d\n", val);
	}
}





static ssize_t x_show(struct class *class, struct class_attribute *attr,
	char *buf)
{
	/*printk("x_show");*/

	return 0;
}

static ssize_t x_store(struct class *class, struct class_attribute *attr,
	const char *buf, size_t count)
{
	/*printk("x_store");*/

	return 0;
}



static ssize_t sys_reset(struct class *class, struct class_attribute *attr,
	const char *buf, size_t count)
{
	/*printk("sys_reset");*/

	return 0;
}



/* <linux/device.h>
* #define CLASS_ATTR(_name, _mode, _show, _store) \
* struct class_attribute class_attr_##_name = __ATTR(_name, _mode,
* _show, _store)
*/
CLASS_ATTR(reset, 0664, NULL, &sys_reset);

static struct class *x_class;


static int hello_init(struct platform_device *pdev)
{
	int res;

	struct device *dev = &pdev->dev;

	match = of_match_device(of_match_ptr(my_drvr_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}

	dev_info(dev, "Hello, world!\n");

	get_platform_info(pdev);




	x_class = class_create(THIS_MODULE, "x-class");
	if (IS_ERR(x_class))
		dev_info(dev, "bad class create\n");

	res = class_create_file(x_class, &class_attr_reset);

	dev_info(dev, "'xxx' module initialized\n");
	return 0;
}



static int hello_exit(struct platform_device *pdev)
{
		struct device *dev = &pdev->dev;

		class_remove_file(x_class, &class_attr_reset);
		class_destroy(x_class);

		dev_info(dev, "Goodbye, world!\n");
		return 0;
}




static const struct of_device_id my_drvr_match[] = {
	{ .compatible = "DAndy,coremod_my", },
	{ },
};
MODULE_DEVICE_TABLE(of, my_drvr_match);


static struct platform_driver my_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = my_drvr_match,
	},
	.probe = hello_init,
	.remove = hello_exit,
};
module_platform_driver(my_driver);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Devyatov Andrey <devyatov.andey@litech.com>");
