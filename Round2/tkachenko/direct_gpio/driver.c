#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/delay.h>


#define DRIVER_AUTHOR  "O.Tkachenko"
#define DRIVER_DESC "A simple driver."

#define pdev_info(pdev, ...) (dev_info(&(pdev)->dev, __VA_ARGS__))
#define INTERVAL 100

#define LED1_GPIO (32+21) /* GPIO1_21 */
#define LED2_GPIO (32+22) /* GPIO1_22 */
#define LED3_GPIO (32+23) /* GPIO1_23 */
#define LED4_GPIO (32+24) /* GPIO1_24 */
#define LEDS_NUM  4

struct glbbb_timer_data {
	struct platform_device  *pdev;
	struct timer_list		timer;
	int						gpio_led[LEDS_NUM];
	int						current_idx;
	bool					switchOn;
};

static struct glbbb_timer_data timer_data;

static void blink(unsigned long __timer_data)
{
	struct glbbb_timer_data *data = (void *)__timer_data;
	unsigned int state = data->switchOn ? 1 : 0;

	gpio_set_value(data->gpio_led[data->current_idx], state);
	data->current_idx += data->switchOn ? 1 : -1;

	if (data->current_idx == LEDS_NUM || data->current_idx < 0) {
		data->switchOn = !data->switchOn;
		data->current_idx += data->switchOn ? 1 : -1;
	}
	mod_timer(&data->timer, jiffies + INTERVAL);
}

static int glbbb_probe(struct platform_device *pdev)
{
	const char         *string_prop = NULL;
	int					idx         = 0;
	struct device_node *node        = pdev->dev.of_node;

	pdev_info(pdev, "Probe!\n");

	if (node != NULL &&
		of_property_read_string(node,
					"string-property",
					&string_prop) == 0) {
		pdev_info(pdev, "string-property: %s\n", string_prop);
	}

	pdev_info(pdev, "Setup timer\n");
	timer_data.pdev = pdev;
	timer_data.gpio_led[0] = LED1_GPIO;
	timer_data.gpio_led[1] = LED2_GPIO;
	timer_data.gpio_led[2] = LED3_GPIO;
	timer_data.gpio_led[3] = LED4_GPIO;
	for (idx = 0; idx < LEDS_NUM; idx++)
		gpio_set_value(timer_data.gpio_led[idx], 0);
	timer_data.current_idx = 0;
	timer_data.switchOn = true;
	setup_timer(&timer_data.timer, blink, (unsigned long) &timer_data);
	mod_timer(&timer_data.timer, jiffies + INTERVAL);
	pdev_info(pdev, "Setup finished\n");

	return 0;
}

static int glbbb_remove(struct platform_device *pdev)
{
	int idx;

	pdev_info(pdev, "Remove!\n");
	for (idx = 0; idx < LEDS_NUM; idx++)
		gpio_set_value(timer_data.gpio_led[idx], 0);
	del_timer_sync(&timer_data.timer);
	return 0;
}

static const struct of_device_id glbbb_of_match[] = {
	{
		.compatible = "gl,console-heartbeat",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, glbbb_of_match);

static struct platform_driver glbbb_driver = {
	.probe = glbbb_probe,
	.remove = glbbb_remove,
	.driver = {
		.name = "glbbb",
		.of_match_table = of_match_ptr(glbbb_of_match),
		.owner = THIS_MODULE,
		},
};

module_platform_driver(glbbb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
