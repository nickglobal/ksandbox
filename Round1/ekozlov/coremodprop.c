#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/slab.h>


#define EK_DRV_TAG	"<ek>: "

#define EK_DRV_DEBUG
#ifdef EK_DRV_DEBUG
#define FNIN	pr_info(EK_DRV_TAG "--> %s\n", __func__)
#define FNOUT	pr_info(EK_DRV_TAG "<-- %s\n", __func__)
#else
#define FNIN
#define FNOUT
#endif


#define EK_DRV_NAME	"coremodprop"

#define EK_DRV_PROPERTY_NAME		"string-property"
#define EK_DRV_PROPERTY_BUF_SIZE	256
static char ek_drv_property[EK_DRV_PROPERTY_BUF_SIZE] = {0};


static const struct of_device_id ek_drv_of_match[] = {
	{ .compatible = "gl,console-heartbeat", 0 },
	{ }
};
MODULE_DEVICE_TABLE(of, ek_drv_of_match);

static int ek_drv_platform_probe(struct platform_device *pdev);
static int ek_drv_platform_remove(struct platform_device *pdev);

static struct platform_driver ek_drv_platform_driver = {
	.driver = {
		.name = EK_DRV_NAME,
		.owner = THIS_MODULE,
		/* .of_match_table = of_match_ptr - OK */
		.of_match_table = of_match_ptr(ek_drv_of_match), /* verify */
	},
	.probe =    ek_drv_platform_probe,
	.remove =   ek_drv_platform_remove,
};


static int ek_drv_platform_probe(struct platform_device *pdev)
{
	int rc = -0;
	const struct of_device_id *of_id = NULL;
	struct property *prop = NULL;
	int proplen = 0;
	const char *pprop = NULL;
	int x = 0;

	FNIN;

	if (pdev == NULL) {
		pr_err(EK_DRV_TAG "zero parameter\n");
		rc = -ENODEV;
		goto EXIT;
	}

	of_id = of_match_device(ek_drv_of_match, &pdev->dev);
	if (of_id == NULL) {
		dev_err(&pdev->dev, EK_DRV_TAG "cannot find device matching\n");
		rc = -ENODEV;
		goto EXIT;
	} else if (pdev->dev.of_node == NULL) {
		dev_err(&pdev->dev, EK_DRV_TAG "cannot find device node\n");
		rc = -ENODEV;
		goto EXIT;
	} else if (of_node_is_initialized(pdev->dev.of_node) == 0) {
		dev_err(&pdev->dev, EK_DRV_TAG "node is not initialized\n");
		rc = -ENODEV;
		goto EXIT;
	}

	/* Reading the property using u8 buffer */
	{
		dev_info(
			&pdev->dev,
			EK_DRV_TAG "Reading the property using u8 buffer...\n");
		prop = of_find_property(
				pdev->dev.of_node,
				EK_DRV_PROPERTY_NAME,
				&proplen);
		dev_dbg(
			&pdev->dev,
			EK_DRV_TAG "prop = %p, proplen = %d\n", prop, proplen);
		if (prop == NULL)
			dev_err(
				&pdev->dev,
				EK_DRV_TAG "cannot find property\n");

		if (prop && proplen < EK_DRV_PROPERTY_BUF_SIZE) {
			x = of_property_read_u8_array(
						pdev->dev.of_node,
						EK_DRV_PROPERTY_NAME,
						ek_drv_property,
						proplen);
			dev_info(&pdev->dev,
				EK_DRV_TAG "x = %d, property = %s\n",
				x, (x == 0) ? ek_drv_property : ":(");

		}
	}

	/* Reading the property using string read */
	dev_info(&pdev->dev,
		EK_DRV_TAG "Reading the property using string read...\n");
	x = of_property_read_string(
				pdev->dev.of_node,
				EK_DRV_PROPERTY_NAME,
				&pprop);
	dev_info(&pdev->dev,
		EK_DRV_TAG "x = %d, property = %s\n",
		x, (x == 0) ? pprop : ":(");


EXIT:
	FNOUT;
	return rc;
}


static int ek_drv_platform_remove(struct platform_device *pdev)
{
	FNIN;
	FNOUT;
	return -0;
}


static int __init ek_drv_init(void)
{
	int rc = -0;

	FNIN;

	rc = platform_driver_register(&ek_drv_platform_driver);
	if (rc)
		pr_err(EK_DRV_TAG
			"cannot register platform driver, %d\n", rc);

	FNOUT;
	return rc;
}


static void __exit ek_drv_exit(void)
{
	FNIN;
	platform_driver_unregister(&ek_drv_platform_driver);
	FNOUT;
}


module_init(ek_drv_init);
module_exit(ek_drv_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eugene Kozlov");
MODULE_DESCRIPTION("Simple device-tree parsing driver");
