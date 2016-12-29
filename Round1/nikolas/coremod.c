#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>

#define DRIVER_AUTHOR  "Nick>"
#define DRIVER_DESC "A simple driver."

static int __init mod_init(void)
{
	printk(KERN_DEBUG "Hello World!\n");

    return 0;
}

static void __exit mod_exit(void)
{

}


module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
