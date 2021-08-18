
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/io.h>
#include <linux/device.h>

#include <linux/platform_device.h>

#include "i2c_mpu6050.h"
#include "oled_code_table.c"
/*------------------字符设备内容----------------------*/
#define DEV_NAME "I2C1_mpu6050"
#define DEV_CNT (1)
#define WriteCmd(client, cmd) i2c_write_mpu6050(client, 0x00, cmd)
#define WriteDat(client, data) i2c_write_mpu6050(client, 0x40, data)
/*定义 led 资源结构体，保存获取得到的节点信息以及转换后的虚拟寄存器地址*/
static dev_t mpu6050_devno;				 //定义字符设备的设备号
static struct cdev mpu6050_chr_dev;		 //定义字符设备结构体chr_dev
struct class *class_mpu6050;			 //保存创建的类
struct device *device_mpu6050;			 // 保存创建的设备
struct device_node *mpu6050_device_node; //rgb_led的设备树节点结构体

/*------------------IIC设备内容----------------------*/
struct i2c_client *mpu6050_client = NULL; //保存mpu6050设备对应的i2c_client结构体，匹配成功后由.prob函数带回。

/*通过i2c 向mpu6050写入数据
*mpu6050_client：mpu6050的i2c_client结构体。
*address, 数据要写入的地址，
*data, 要写入的数据
*返回值，错误，-1。成功，0  
*/
static int i2c_write_mpu6050(struct i2c_client *mpu6050_client, u8 address, u8 data)
{
	int error = 0;
	u8 write_data[2];
	struct i2c_msg send_msg; //要发送的数据结构体

	/*设置要发送的数据*/
	write_data[0] = address;
	write_data[1] = data;

	/*发送 iic要写入的地址 reg*/
	send_msg.addr = mpu6050_client->addr; //mpu6050在 iic 总线上的地址
	send_msg.flags = 0;					  //标记为发送数据
	send_msg.buf = write_data;			  //写入的首地址
	send_msg.len = 2;					  //reg长度

	/*执行发送*/
	error = i2c_transfer(mpu6050_client->adapter, &send_msg, 1);
	if (error != 1)
	{
		printk(KERN_DEBUG "\n i2c_transfer error \n");
		return -1;
	}
	return 0;
}

/*通过i2c 向mpu6050写入数据
*mpu6050_client：mpu6050的i2c_client结构体。
*address, 要读取的地址，
*data，保存读取得到的数据
*length，读长度
*返回值，错误，-1。成功，0
*/
static int i2c_read_mpu6050(struct i2c_client *mpu6050_client, u8 address, void *data, u32 length)
{
	int error = 0;
	u8 address_data = address;
	struct i2c_msg mpu6050_msg[2];
	/*设置读取位置msg*/
	mpu6050_msg[0].addr = mpu6050_client->addr; //mpu6050在 iic 总线上的地址
	mpu6050_msg[0].flags = 0;					//标记为发送数据
	mpu6050_msg[0].buf = &address_data;			//写入的首地址
	mpu6050_msg[0].len = 1;						//写入长度

	/*设置读取位置msg*/
	mpu6050_msg[1].addr = mpu6050_client->addr; //mpu6050在 iic 总线上的地址
	mpu6050_msg[1].flags = I2C_M_RD;			//标记为读取数据
	mpu6050_msg[1].buf = data;					//读取得到的数据保存位置
	mpu6050_msg[1].len = length;				//读取长度

	error = i2c_transfer(mpu6050_client->adapter, mpu6050_msg, 2);

	if (error != 2)
	{
		printk(KERN_DEBUG "\n i2c_read_mpu6050 error \n");
		return -1;
	}
	return 0;
}
/**
  * @brief  OLED_Fill，填充整个屏幕
  * @param  fill_Data:要填充的数据
	* @retval 无
  */
static void OLED_Fill(unsigned char fill_Data) //全屏填充
{
    unsigned char m, n;
    for (m = 0; m < 8; m++)
    {
        WriteCmd(mpu6050_client,  0xb0 + m); //page0-page1
        WriteCmd(mpu6050_client,  0x00);     //low column start address
        WriteCmd(mpu6050_client,  0x10);     //high column start address

        for (n = 0; n < 128; n++)
        {
            // WriteDat(fill_Data);
            WriteDat(mpu6050_client , fill_Data); //high column start address
        }
    }
}


 /**
  * @brief  OLED_SetPos，设置光标
  * @param  x,光标x位置  y，光标y位置
  *					
  * @retval 无
  */
static void oled_set_Pos(unsigned char x, unsigned char y) //设置起始点坐标
{ 
	WriteCmd(mpu6050_client, 0xb0+y);
	WriteCmd(mpu6050_client, ((x&0xf0)>>4)|0x10);
	WriteCmd(mpu6050_client, (x&0x0f)|0x01);
}
/**
  * @brief  OLED_CLS，清屏
  * @param  无
	* @retval 无
  */
static void OLED_CLS(void)//清屏
{
	OLED_Fill(0x00);
}


 /**
  * @brief  OLED_ON，将OLED从休眠中唤醒
  * @param  无
	* @retval 无
  */
static void OLED_ON(void)
{
	WriteCmd(mpu6050_client,0X8D);  //设置电荷泵
	WriteCmd(mpu6050_client,0X14);  //开启电荷泵
	WriteCmd(mpu6050_client,0XAF);  //OLED唤醒
}


 /**
  * @brief  OLED_OFF，让OLED休眠 -- 休眠模式下,OLED功耗不到10uA
  * @param  无
	* @retval 无
  */
static void OLED_OFF(void)
{
	WriteCmd(mpu6050_client,0X8D);  //设置电荷泵
	WriteCmd(mpu6050_client,0X10);  //关闭电荷泵
	WriteCmd(mpu6050_client,0XAE);  //OLED休眠
}
static void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize)
{
	unsigned char c = 0,i = 0,j = 0;
	switch(TextSize)
	{
		case 1:
		{
			while(ch[j] != '\0')
			{
				c = ch[j] - 32;
				if(x > 126)
				{
					x = 0;
					y++;
				}
				oled_set_Pos(x,y);
				for(i=0;i<6;i++)
					WriteDat(mpu6050_client,F6x8[c][i]);
				x += 6;
				j++;
			}
		}break;
		case 2:
		{
			while(ch[j] != '\0')
			{
				c = ch[j] - 32;
				if(x > 120)
				{
					x = 0;
					y++;
				}  
				oled_set_Pos(x,y);
				for(i=0;i<8;i++)
					WriteDat(mpu6050_client,F8X16[c*16+i]);
				oled_set_Pos(x,y+1);
				for(i=0;i<8;i++)
					WriteDat(mpu6050_client,F8X16[c*16+i+8]);
				x += 8;
				j++;
			}
		}break;
	}
}
/*初始化i2c
*返回值，成功，返回0。失败，返回 -1
*/
static int mpu6050_init(void)
{
	int error = 0;
	/*配置mpu6050*/
	error += WriteCmd(mpu6050_client,0xae);
	error += WriteCmd(mpu6050_client,0x20);
	error += WriteCmd(mpu6050_client,0x10);
	error += WriteCmd(mpu6050_client,0xb0);
	error += WriteCmd(mpu6050_client,0xc8);
	error += WriteCmd(mpu6050_client,0x00);
	error += WriteCmd(mpu6050_client,0x10);
	error += WriteCmd(mpu6050_client,0x40);
	error += WriteCmd(mpu6050_client,0x81);
	error += WriteCmd(mpu6050_client,0xff);
	error += WriteCmd(mpu6050_client,0xa1);
	error += WriteCmd(mpu6050_client,0xa6);
	error += WriteCmd(mpu6050_client,0xa8);
	error += WriteCmd(mpu6050_client,0x3f);
	error += WriteCmd(mpu6050_client,0xa4);
	error += WriteCmd(mpu6050_client,0xd3);
	error += WriteCmd(mpu6050_client,0x00);
	error += WriteCmd(mpu6050_client,0xd5);
	error += WriteCmd(mpu6050_client,0xf0);
	error += WriteCmd(mpu6050_client,0xd9);
	error += WriteCmd(mpu6050_client,0x22);
	error += WriteCmd(mpu6050_client,0xda);	
	error += WriteCmd(mpu6050_client,0x12);
	error += WriteCmd(mpu6050_client,0xdb);
	error += WriteCmd(mpu6050_client,0x20);
	error += WriteCmd(mpu6050_client,0x8d);
	error += WriteCmd(mpu6050_client,0x14);	
	error += WriteCmd(mpu6050_client,0xaf);
	
	if (error < 0)
	{
		/*初始化错误*/
		printk(KERN_DEBUG "\n mpu6050_init error \n");
		return -1;
	}
printk(KERN_DEBUG "\n mpu6050_init error \n");
	OLED_CLS();
	OLED_ShowStr(0,0,"Hello,xiaofei!",2);
	return 0;
}

/*字符设备操作函数集，open函数实现*/
static int mpu6050_open(struct inode *inode, struct file *filp)
{
	// printk("\n mpu6050_open \n");

	/*向 mpu6050 发送配置数据，让mpu6050处于正常工作状态*/
	mpu6050_init();
	return 0;
}

/*字符设备操作函数集，.read函数实现*/
static ssize_t mpu6050_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	char data_H;
	char data_L;
	int error;
	short mpu6050_result[6]; //保存mpu6050转换得到的原始数据

	// printk("\n mpu6050_read \n");
	
	i2c_read_mpu6050(mpu6050_client, ACCEL_XOUT_H, &data_H, 1);
	i2c_read_mpu6050(mpu6050_client, ACCEL_XOUT_L, &data_L, 1);
	mpu6050_result[0] = data_H << 8;
	mpu6050_result[0] += data_L;

	i2c_read_mpu6050(mpu6050_client, ACCEL_YOUT_H, &data_H, 1);
	i2c_read_mpu6050(mpu6050_client, ACCEL_YOUT_L, &data_L, 1);
	mpu6050_result[1] = data_H << 8;
    mpu6050_result[1] += data_L;

	i2c_read_mpu6050(mpu6050_client, ACCEL_ZOUT_H, &data_H, 1);
	i2c_read_mpu6050(mpu6050_client, ACCEL_ZOUT_L, &data_L, 1);
	mpu6050_result[2] = data_H << 8;
	mpu6050_result[2] += data_L;

	i2c_read_mpu6050(mpu6050_client, GYRO_XOUT_H, &data_H, 1);
	i2c_read_mpu6050(mpu6050_client, GYRO_XOUT_L, &data_L, 1);
	mpu6050_result[3] = data_H << 8;
	mpu6050_result[3] += data_L;

	i2c_read_mpu6050(mpu6050_client, GYRO_YOUT_H, &data_H, 1);
	i2c_read_mpu6050(mpu6050_client, GYRO_YOUT_L, &data_L, 1);
	mpu6050_result[4] = data_H << 8;
	mpu6050_result[4] += data_L;

	i2c_read_mpu6050(mpu6050_client, GYRO_ZOUT_H, &data_H, 1);
	i2c_read_mpu6050(mpu6050_client, GYRO_ZOUT_L, &data_L, 1);
	mpu6050_result[5] = data_H << 8;
	mpu6050_result[5] += data_L;


	// printk("AX=%d, AY=%d, AZ=%d \n",(int)mpu6050_result[0],(int)mpu6050_result[1],(int)mpu6050_result[2]);
	// printk("GX=%d, GY=%d, GZ=%d \n \n",(int)mpu6050_result[3],(int)mpu6050_result[4],(int)mpu6050_result[5]);

	/*将读取得到的数据拷贝到用户空间*/
	error = copy_to_user(buf, mpu6050_result, cnt);

	if(error != 0)
	{
		printk("copy_to_user error!");
		return -1;
	}
	return 0;
}

/*字符设备操作函数集，.release函数实现*/
static int mpu6050_release(struct inode *inode, struct file *filp)
{
	// printk("\n mpu6050_release \n");
	
	/*向mpu6050发送命令，使mpu6050进入关机状态*/
	return 0;
}

/*字符设备操作函数集*/
static struct file_operations mpu6050_chr_dev_fops =
	{
		.owner = THIS_MODULE,
		.open = mpu6050_open,
		.read = mpu6050_read,
		.release = mpu6050_release,
};

/*----------------平台驱动函数集-----------------*/
static int mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int ret = -1; //保存错误状态码

	printk(KERN_EMERG "\t  match successed  \n");
	/*---------------------注册 字符设备部分-----------------*/

	//采用动态分配的方式，获取设备编号，次设备号为0，
	//设备名称为rgb-leds，可通过命令cat  /proc/devices查看
	//DEV_CNT为1，当前只申请一个设备编号
	ret = alloc_chrdev_region(&mpu6050_devno, 0, DEV_CNT, DEV_NAME);
	if (ret < 0)
	{
		printk("fail to alloc mpu6050_devno\n");
		goto alloc_err;
	}

	//关联字符设备结构体cdev与文件操作结构体file_operations
	mpu6050_chr_dev.owner = THIS_MODULE;
	cdev_init(&mpu6050_chr_dev, &mpu6050_chr_dev_fops);

	// 添加设备至cdev_map散列表中
	ret = cdev_add(&mpu6050_chr_dev, mpu6050_devno, DEV_CNT);
	if (ret < 0)
	{
		printk("fail to add cdev\n");
		goto add_err;
	}

	/*创建类 */
	class_mpu6050 = class_create(THIS_MODULE, DEV_NAME);

	/*创建设备 DEV_NAME 指定设备名，*/
	device_mpu6050 = device_create(class_mpu6050, NULL, mpu6050_devno, NULL, DEV_NAME);
	mpu6050_client = client;
	return 0;

add_err:
	// 添加设备失败时，需要注销设备号
	unregister_chrdev_region(mpu6050_devno, DEV_CNT);
	printk("\n error! \n");
alloc_err:

	return -1;
}


static int mpu6050_remove(struct i2c_client *client)
{
	/*删除设备*/
	device_destroy(class_mpu6050, mpu6050_devno);	  //清除设备
	class_destroy(class_mpu6050);					  //清除类
	cdev_del(&mpu6050_chr_dev);						  //清除设备号
	unregister_chrdev_region(mpu6050_devno, DEV_CNT); //取消注册字符设备
	return 0;
}



/*定义ID 匹配表*/
static const struct i2c_device_id gtp_device_id[] = {
	{"fire,i2c_mpu6050", 0},
	{}};

/*定义设备树匹配表*/
static const struct of_device_id mpu6050_of_match_table[] = {
	{.compatible = "fire,i2c_mpu6050"},
	{/* sentinel */}};

/*定义i2c总线设备结构体*/
struct i2c_driver mpu6050_driver = {
	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.id_table = gtp_device_id,
	.driver = {
		.name = "fire,i2c_mpu6050",
		.owner = THIS_MODULE,
		.of_match_table = mpu6050_of_match_table,
	},
};

/*
*驱动初始化函数
*/
static int __init mpu6050_driver_init(void)
{
	int ret;
	pr_info("mpu6050_driver_init\n");
	ret = i2c_add_driver(&mpu6050_driver);
	return ret;
}

/*
*驱动注销函数
*/
static void __exit mpu6050_driver_exit(void)
{
	pr_info("mpu6050_driver_exit\n");
	i2c_del_driver(&mpu6050_driver);
}

module_init(mpu6050_driver_init);
module_exit(mpu6050_driver_exit);

MODULE_LICENSE("GPL");
