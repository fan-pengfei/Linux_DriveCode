#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include "oled.h"

static struct i2c_board_info info = {
	I2C_BOARD_INFO("oled_0.96", 0x3C),
};

static const struct i2c_device_id oled_id[] = {
	{"oled_0.96", 0},
	{},
};

static struct i2c_driver driver = {
	.driver = {
		.name = "oled_0.96",
		.owner = THIS_MODULE,
	},
	.probe = OLED_Probe,
	.remove = OLED_Remove,
	.id_table = oled_id,
};

static struct i2c_client *client;

static ssize_t dev_write(struct file *file, const char __user *buf, size_t count, loff_t *fpos)
{
	char *str = vmalloc(count);
	if (copy_from_user(str, buf, count))
	{
		vfree(str);
		return -EFAULT;
	}
	*(str + count) = '\0';
	OLED_Set(client, str);
	vfree(str);
	return count;
}

static struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.write = &dev_write,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "oled",
	.fops = &dev_fops,
};

static __init int Module_Init(void)
{
	struct i2c_adapter *adapter;
	int ret;
	adapter = i2c_get_adapter(1);
	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	ret = i2c_add_driver(&driver);
	if (ret) {
		printk(KERN_ERR "I2C error\n");
		return ret;
	}
	ret = misc_register(&mdev);
	if (ret)
	{
		printk(KERN_ERR "Mdev error\n");
	}
	return ret;
}

static __exit void Module_Exit(void)
{
	misc_deregister(&mdev);
	i2c_unregister_device(client);
	i2c_del_driver(&driver);
}

module_init(Module_Init);
module_exit(Module_Exit);

MODULE_LICENSE("GPL");
