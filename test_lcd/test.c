
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fb.h>

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

#include <linux/spi/spi.h>
#include "ecspi_tft.h"
int tft_control_pin_number;			   // 保存oled D/C控制引脚的引脚号（从对应设备树节点中获取）
int tft_res_pin_number;			   // 保存oled D/C控制引脚的引脚号（从对应设备树节点中获取）

extern int myfb_new(struct spi_device *);
extern void myfb_del(struct spi_device *);
void write_command(struct spi_device *spi_device, u8 command)
{
	int error = 0;
	u8 tx_data = command;
	struct spi_message *message;   //定义发送的消息
	struct spi_transfer *transfer; //定义传输结构体

	/*设置 D/C引脚为低电平*/
	gpio_direction_output(tft_control_pin_number, 0);

	/*申请空间*/
	message = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

	/*填充message和transfer结构体*/
	transfer->tx_buf = &tx_data;
	transfer->len = 1;
	spi_message_init(message);
	spi_message_add_tail(transfer, message);

	error = spi_sync(spi_device, message);
	kfree(message);
	kfree(transfer);
	if (error != 0)
	{
		printk("spi_sync error! \n");
	}
	gpio_direction_output(tft_control_pin_number, 1);
}

void write_data(struct spi_device *spi_device, u8 data)
{
	int error = 0;
	u8 tx_data = data;
	struct spi_message *message;   //定义发送的消息
	struct spi_transfer *transfer; //定义传输结构体

	/*设置 D/C引脚为高电平*/
	gpio_direction_output(tft_control_pin_number, 1);

	/*申请空间*/
	message = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

	/*填充message和transfer结构体*/
	transfer->tx_buf = &tx_data;
	transfer->len = 1;
	spi_message_init(message);
	spi_message_add_tail(transfer, message);

	error = spi_sync(spi_device, message);
	kfree(message);
	kfree(transfer);
	if (error != 0)
	{
		printk("spi_sync error! \n");
	}
}

//初始化spi_lcd
void spi_lcd_init(struct spi_device *spi)
{
 	/*初始化oled*/
	gpio_direction_output(tft_res_pin_number, 0);
	msleep(100);
	gpio_direction_output(tft_res_pin_number, 1);
	msleep(100);
	write_command(spi,0x36);
	write_data(spi,0x70);

	write_command(spi,0x3A);
	write_data(spi,0x05);

	write_command(spi,0xB2);
	write_data(spi,0x0C);
	write_data(spi,0x0C);
	write_data(spi,0x00);
	write_data(spi,0x33);
	write_data(spi,0x33); 

	write_command(spi,0xB7); 
	write_data(spi,0x35);  

	write_command(spi,0xBB);
	write_data(spi,0x19);

	write_command(spi,0xC0);
	write_data(spi,0x2C);

	write_command(spi,0xC2);
	write_data(spi,0x01);

	write_command(spi,0xC3);
	write_data(spi,0x12);   

	write_command(spi,0xC4);
	write_data(spi,0x20);  

	write_command(spi,0xC6); 
	write_data(spi,0x0F);    

	write_command(spi,0xD0); 
	write_data(spi,0xA4);
	write_data(spi,0xA1);

	write_command(spi,0xE0);
	write_data(spi,0xD0);
	write_data(spi,0x04);
	write_data(spi,0x0D);
	write_data(spi,0x11);
	write_data(spi,0x13);
	write_data(spi,0x2B);
	write_data(spi,0x3F);
	write_data(spi,0x54);
	write_data(spi,0x4C);
	write_data(spi,0x18);
	write_data(spi,0x0D);
	write_data(spi,0x0B);
	write_data(spi,0x1F);
	write_data(spi,0x23);

	write_command(spi,0xE1);
	write_data(spi,0xD0);
	write_data(spi,0x04);
	write_data(spi,0x0C);
	write_data(spi,0x11);
	write_data(spi,0x13);
	write_data(spi,0x2C);
	write_data(spi,0x3F);
	write_data(spi,0x44);
	write_data(spi,0x51);
	write_data(spi,0x2F);
	write_data(spi,0x1F);
	write_data(spi,0x1F);
	write_data(spi,0x20);
	write_data(spi,0x23);

	write_command(spi,0x21);

	write_command(spi,0x11); 
	msleep(120); 

	write_command(spi,0x29); 
}
static void LCD_WR_DATA(struct spi_device *spi,unsigned int dat)
{
	write_data(spi,dat>>8);
	write_data(spi,dat);
}
//设置要刷屏的开始坐标
void addset(struct spi_device *spi, unsigned int x,unsigned int y)
{
		write_command(spi,0x2a);//列地址设置
		LCD_WR_DATA(spi,x+40);
		LCD_WR_DATA(spi,239+40);
		write_command(spi,0x2b);//行地址设置
		LCD_WR_DATA(spi,y+53);
		LCD_WR_DATA(spi,134+53);
		write_command(spi,0x2c);//储存器写
}

void show_fb(struct fb_info *fbi, struct spi_device *spi)
{
    int x, y;
    u32 k;
    u32 *p = (u32 *)(fbi->screen_base);
    u16 c;
    u8 *pp;

    addset(spi, 0, 0); //从屏的0,0坐标开始刷
//  gpio_direction_output(pdata->dc_io, 1); 
    for (y = 0; y < fbi->var.yres; y++)
    {
        for (x = 0; x < fbi->var.xres; x++)
        {
            k = p[y*fbi->var.xres+x];//取出一个像素点的32位数据
            // rgb8888 --> rgb565       
            pp = (u8 *)&k;  
            c = pp[0] >> 3; //蓝色
            c |= (pp[1]>>2)<<5; //绿色
            c |= (pp[2]>>3)<<11; //红色

            //发出像素数据的rgb565
            write_data(spi, c >> 8);
            write_data(spi, c & 0xff);
        }
    }

}


int myprobe(struct spi_device *spi)
{   
  	int ret = -1; //保存错误状态码
	printk(KERN_EMERG "\t  match successed  \n");
	struct device_node *tft_device_node;	   //ecspi_oled的设备树节点结构体
	/*获取 ecspi_oled 设备树节点*/
	tft_device_node = of_find_node_by_path("/soc/aips-bus@2000000/spba-bus@2000000/ecspi@2008000/ecspi_oled@0");
	if (tft_device_node == NULL)
	{
		printk(KERN_EMERG "\t  get ecspi_oled@0 failed!  \n");
	}

	/*获取 oled 的 D/C 控制引脚并设置为输出，默认高电平*/
	tft_control_pin_number = of_get_named_gpio(tft_device_node, "d_c_control_pin", 0);
	printk("tft_control_pin_number = %d,\n ", tft_control_pin_number);
	gpio_direction_output(tft_control_pin_number, 1);

	tft_res_pin_number = of_get_named_gpio(tft_device_node, "res_pin", 0);
	printk("tft_control_pin_number = %d,\n ", tft_res_pin_number);
	gpio_direction_output(tft_res_pin_number, 1);
	spi->mode = SPI_MODE_2;
	spi_setup(spi);
        spi_lcd_init(spi); //初始化屏

    printk("probe ...%s\n", spi->modalias);
    return myfb_new(spi); //fb设备初始化
err1:
    gpio_free(tft_res_pin_number);
err0:
    return ret;
}

int myremove(struct spi_device *spi)
{
    struct myspi_lcd_pdata *pdata = spi->dev.platform_data;

    myfb_del(spi); //fb设备回收

    gpio_free(tft_control_pin_number);
    gpio_free(tft_res_pin_number);
    printk("%s remove\n", spi->modalias);

    return 0;
}


struct spi_device_id ids[] = {
    {"myspi_lcd"},
    {},
};

struct spi_driver myspi_drv = {
    .driver = {
        .owner = THIS_MODULE,
        .name = "myspi_drv",
    },
    .probe = myprobe,
    .remove = myremove,
    .id_table = ids,
};

module_spi_driver(myspi_drv);
MODULE_LICENSE("GPL");


