#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/fs.h> 
#include <linux/module.h> 
#include <linux/uaccess.h> 
#include "lcdfont.h"
#include "ecspi_tft.h"
#define X 240
#define Y 135
/*------------------字符设备内容----------------------*/
#define DEV_NAME "ecspi_tft"
#define DEV_CNT (1)
typedef struct tft_display_struct
{
	unsigned char x;
	unsigned char y;	
	unsigned char TextSize;
	unsigned char ch[128]; 
	u16 color;

}tft_display_struct;
typedef struct {
    struct spi_device *spi; //记录fb_info对象对应的spi设备对象
    struct task_struct *thread; //记录线程对象的地址，此线程专用于把显存数据发送到屏的驱动ic
}lcd_data_t;
struct fb_info *fbi;
void show_fb(struct fb_info *fbi, struct spi_device *spi);
static int tft_lcdfb_setcolreg(unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue,
			     unsigned int transp, struct fb_info *info);
struct fb_ops fops = {
    .owner		= THIS_MODULE,
    .fb_setcolreg	= tft_lcdfb_setcolreg,
    .fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static dev_t tft_devno;		 //定义字符设备的设备号
static struct cdev tft_chr_dev; //定义字符设备结构体chr_dev
struct class *class_tft;		 //保存创建的类
struct device *device_tft;		 // 保存创建的设备

/*------------------IIC设备内容----------------------*/
struct spi_device *tft_spi_device = NULL; //保存oled设备对应的spi_device结构体，匹配成功后由.prob函数带回。
struct device_node *tft_device_node;	   //ecspi_oled的设备树节点结构体
int tft_control_pin_number;			   // 保存oled D/C控制引脚的引脚号（从对应设备树节点中获取）
int tft_res_pin_number;			   // 保存oled D/C控制引脚的引脚号（从对应设备树节点中获取）
int thread_func(void *data)
{
   
    lcd_data_t *ldata = fbi->par;

    while (1)
    {   
        if (kthread_should_stop())
            break;
        show_fb(fbi, ldata->spi);
    }

    return 0;
}
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}
static u32 pseudo_palette[240*135];
static int tft_lcdfb_setcolreg(unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue,
			     unsigned int transp, struct fb_info *info)
{
	unsigned int val;
	
	if (regno > 16)
	{
		return 1;
	}

	/* 用red,green,blue三原色构造出val  */
	val  = chan_to_field(red,	&info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue,	&info->var.blue);
	
	pseudo_palette[regno] = val;
	return 0;
}
int myfb_new(struct spi_device *spi) //此函数在spi设备驱动的probe函数里被调用
{
    u8 *v_addr;
    size_t p_addr;
    lcd_data_t *data;

    /*
        coherent:连贯的
        分配连贯的物理内存
    */
    v_addr = dma_alloc_coherent(NULL, X*Y*4, &p_addr, GFP_KERNEL);
    
    //额外分配lcd_data_t类型空间
    fbi = framebuffer_alloc(sizeof(lcd_data_t), NULL);
    if(fbi == NULL){
		printk("fbi allow error!\n");
        return -1;
    }
    data = fbi->par; //data指针指向额外分配的空间
    data->spi = spi;

    fbi->pseudo_palette = pseudo_palette;
	fbi->var.activate       = FB_ACTIVATE_NOW;

    fbi->var.xres = X;
    fbi->var.yres = Y;
    fbi->var.xres_virtual = X;
    fbi->var.yres_virtual = Y;
    fbi->var.bits_per_pixel = 32; // 屏是rgb565, 但QT程序只能支持32位.还需要在刷图时把32位的像素数据转换成rgb565
    // fbi->var.red.offset = 11;
    // fbi->var.red.length = 5;
    // fbi->var.green.offset = 5;
    // fbi->var.green.length = 6;
    // fbi->var.blue.offset = 0;
    // fbi->var.blue.length = 5;
    fbi->var.red.offset = 16;
    fbi->var.red.length = 8;
    fbi->var.green.offset = 8;
    fbi->var.green.length = 8;
    fbi->var.blue.offset = 0;
    fbi->var.blue.length = 8;

    strcpy(fbi->fix.id, "myfb");
    fbi->fix.smem_start = p_addr; //显存的物理地址
    fbi->fix.smem_len = X*Y*4; 
    fbi->fix.type = FB_TYPE_PACKED_PIXELS;
    fbi->fix.visual = FB_VISUAL_TRUECOLOR;
    fbi->fix.line_length = X*4;

    fbi->fbops = &fops;
    fbi->screen_base = v_addr; //显存虚拟地址
    fbi->screen_size = X*Y*4; //显存大小

    register_framebuffer(fbi);
    data->thread = kthread_run(thread_func, fbi, spi->modalias);
    return 0;    
}

void myfb_del(void) //此函数在spi设备驱动remove时被调用
{
    lcd_data_t *data = fbi->par;
    kthread_stop(data->thread); //让刷图线程退出
    unregister_framebuffer(fbi);
    dma_free_coherent(NULL, fbi->screen_size, fbi->screen_base, fbi->fix.smem_start);
    framebuffer_release(fbi);
}
/*
*函数功能：向oled发送一个命令
*spi_device oled设备驱动对应的spi_device结构体。
*command  要发送的数据。
*返回值：成功，返回0 失败返回负数。
*/
static int tft_send_command(struct spi_device *spi_device, u8 command)
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
		return -1;
	}
	gpio_direction_output(tft_control_pin_number, 1);
	return 0;
}

/*
*函数功能：向oled发送一组命令
*spi_device oled设备驱动对应的spi_device结构体。
*commands  要发送的数据。
*返回值：成功，返回0 失败返回负数。
*/
static int tft_send_commands(struct spi_device *spi_device, u8 *commands, u16 lenght)
{
	int error = 0;
	struct spi_message *message;   //定义发送的消息
	struct spi_transfer *transfer; //定义传输结构体

	/*申请空间*/
	message = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

	/*设置 D/C引脚为低电平*/
	gpio_direction_output(tft_control_pin_number, 0);

	/*填充message和transfer结构体*/
	transfer->tx_buf = commands;
	transfer->len = lenght;
	spi_message_init(message);
	spi_message_add_tail(transfer, message);

	error = spi_sync(spi_device, message);
	kfree(message);
	kfree(transfer);
	if (error != 0)
	{
		printk("spi_sync error! \n");
		return -1;
	}

	return error;
}


/*
*向 oled 发送一个字节
*spi_device，指定oled 设备驱动的spi 结构体
*data, 要发送数据
*/
static int tft_send_one_u8(struct spi_device *spi_device, u8 data)
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
		return -1;
	}
	return 0;
}

/*
*向 oled 发送数据
*spi_device，指定oled 设备驱动的spi 结构体
*data, 要发送数据的地址
*lenght，发送的数据长度
*/
static int oled_send_data(struct spi_device *spi_device, u8 *data, u16 lenght)
{
	int error = 0;
	int index = 0;
	struct spi_message *message;   //定义发送的消息
	struct spi_transfer *transfer; //定义传输结构体

	/*设置 D/C引脚为高电平*/
	gpio_direction_output(tft_control_pin_number, 1);

	/*申请空间*/
	message = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

	/*每次发送 30字节，循环发送*/
	do
	{
		if (lenght > 30)
		{
			transfer->tx_buf = data + index;
			transfer->len = 30;
			spi_message_init(message);
			spi_message_add_tail(transfer, message);
			index += 30;
			lenght -= 30;
		}
		else
		{
			transfer->tx_buf = data + index;
			transfer->len = lenght;
			spi_message_init(message);
			spi_message_add_tail(transfer, message);
			index += lenght;
			lenght = 0;
		}
		error = spi_sync(spi_device, message);
		if (error != 0)
		{
			printk("spi_sync error! %d \n", error);
			return -1;
		}

	} while (lenght > 0);

	kfree(message);
	kfree(transfer);

	return 0;
}
static void LCD_WR_DATA(unsigned int dat)
{
 	u8 data[2];
	data[0]=dat>>8;
	data[1]=dat;
	oled_send_data(tft_spi_device,data,2);
}

/*
*回环测试函数
*spi_device，指定oled 设备驱动的spi 结构体
*/
void loop_back_test(struct spi_device *spi_device)
{
	u8 tx_buffer[2] = {0x66, 0x77};
	u8 rx_buffer[2];
	struct spi_message *message;   //定义发送的消息
	struct spi_transfer *transfer; //定义传输结构体

	message = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	printk("message size=%d,  transfer=%d \n", sizeof(struct spi_message), sizeof(struct spi_transfer));

	transfer->tx_buf = tx_buffer;
	transfer->rx_buf = rx_buffer;
	transfer->len = 2;

	spi_message_init(message);				 /* 初始化spi_message */
	spi_message_add_tail(transfer, message); /* 将spi_transfer添加到spi_message队列 */
	spi_sync(spi_device, message);			 /* 同步发送 */

	printk("tx_buffer=%02X, %02X \n", tx_buffer[0], tx_buffer[1]);
	printk("rx_buffer=%02X, %02X \n", rx_buffer[0], rx_buffer[1]);

	kfree(message);
	kfree(transfer);
}
/******************************************************************************
      函数说明：设置起始和结束地址
      入口数据：x1,x2 设置列的起始和结束地址
                y1,y2 设置行的起始和结束地址
      返回值：  无
******************************************************************************/
void LCD_Address_Set(struct spi_device *spi_device,u16 x1,u16 y1,u16 x2,u16 y2)
{
	if(USE_HORIZONTAL==0)
	{
		tft_send_command(spi_device,0x2a);//列地址设置
		LCD_WR_DATA(x1+52);
		LCD_WR_DATA(x2+52);
		tft_send_command(spi_device,0x2b);//行地址设置
		LCD_WR_DATA(y1+40);
		LCD_WR_DATA(y2+40);
		tft_send_command(spi_device,0x2c);//储存器写
	}
	else if(USE_HORIZONTAL==1)
	{
		tft_send_command(spi_device,0x2a);//列地址设置
		LCD_WR_DATA(x1+53);
		LCD_WR_DATA(x2+53);
		tft_send_command(spi_device,0x2b);//行地址设置
		LCD_WR_DATA(y1+40);
		LCD_WR_DATA(y2+40);
		tft_send_command(spi_device,0x2c);//储存器写
	}
	else if(USE_HORIZONTAL==2)
	{
		tft_send_command(spi_device,0x2a);//列地址设置
		LCD_WR_DATA(x1+40);
		LCD_WR_DATA(x2+40);
		tft_send_command(spi_device,0x2b);//行地址设置
		LCD_WR_DATA(y1+53);
		LCD_WR_DATA(y2+53);
		tft_send_command(spi_device,0x2c);//储存器写
	}
	else
	{
		tft_send_command(spi_device,0x2a);//列地址设置
		LCD_WR_DATA(x1+40);
		LCD_WR_DATA(x2+40);
		tft_send_command(spi_device,0x2b);//行地址设置
		LCD_WR_DATA(y1+52);
		LCD_WR_DATA(y2+52);
		tft_send_command(spi_device,0x2c);//储存器写
	}
}
/*
* 填充整个OLED 显示屏 
* bmp_dat， 填充的值 
*/
void tft_fill(unsigned int Color)
{
	unsigned int i,j;  	
	LCD_Address_Set(tft_spi_device,0,0,240-1,135-1);
    	for(i=0;i<LCD_W;i++)
	 {
	  for (j=0;j<LCD_H;j++)
	   	{
        	LCD_WR_DATA(Color);	 			 
	    }

	  }
}
/*oled 初始化函数*/
void spi_tft_init(void)
{
	/*初始化oled*/
	gpio_direction_output(tft_res_pin_number, 0);
	msleep(100);
	gpio_direction_output(tft_res_pin_number, 1);
	msleep(100);
	tft_send_command(tft_spi_device,0x36);
	tft_send_one_u8(tft_spi_device,0x70);

	tft_send_command(tft_spi_device,0x3A);
	tft_send_one_u8(tft_spi_device,0x05);

	tft_send_command(tft_spi_device,0xB2);
	tft_send_one_u8(tft_spi_device,0x0C);
	tft_send_one_u8(tft_spi_device,0x0C);
	tft_send_one_u8(tft_spi_device,0x00);
	tft_send_one_u8(tft_spi_device,0x33);
	tft_send_one_u8(tft_spi_device,0x33); 

	tft_send_command(tft_spi_device,0xB7); 
	tft_send_one_u8(tft_spi_device,0x35);  

	tft_send_command(tft_spi_device,0xBB);
	tft_send_one_u8(tft_spi_device,0x19);

	tft_send_command(tft_spi_device,0xC0);
	tft_send_one_u8(tft_spi_device,0x2C);

	tft_send_command(tft_spi_device,0xC2);
	tft_send_one_u8(tft_spi_device,0x01);

	tft_send_command(tft_spi_device,0xC3);
	tft_send_one_u8(tft_spi_device,0x12);   

	tft_send_command(tft_spi_device,0xC4);
	tft_send_one_u8(tft_spi_device,0x20);  

	tft_send_command(tft_spi_device,0xC6); 
	tft_send_one_u8(tft_spi_device,0x0F);    

	tft_send_command(tft_spi_device,0xD0); 
	tft_send_one_u8(tft_spi_device,0xA4);
	tft_send_one_u8(tft_spi_device,0xA1);

	tft_send_command(tft_spi_device,0xE0);
	tft_send_one_u8(tft_spi_device,0xD0);
	tft_send_one_u8(tft_spi_device,0x04);
	tft_send_one_u8(tft_spi_device,0x0D);
	tft_send_one_u8(tft_spi_device,0x11);
	tft_send_one_u8(tft_spi_device,0x13);
	tft_send_one_u8(tft_spi_device,0x2B);
	tft_send_one_u8(tft_spi_device,0x3F);
	tft_send_one_u8(tft_spi_device,0x54);
	tft_send_one_u8(tft_spi_device,0x4C);
	tft_send_one_u8(tft_spi_device,0x18);
	tft_send_one_u8(tft_spi_device,0x0D);
	tft_send_one_u8(tft_spi_device,0x0B);
	tft_send_one_u8(tft_spi_device,0x1F);
	tft_send_one_u8(tft_spi_device,0x23);

	tft_send_command(tft_spi_device,0xE1);
	tft_send_one_u8(tft_spi_device,0xD0);
	tft_send_one_u8(tft_spi_device,0x04);
	tft_send_one_u8(tft_spi_device,0x0C);
	tft_send_one_u8(tft_spi_device,0x11);
	tft_send_one_u8(tft_spi_device,0x13);
	tft_send_one_u8(tft_spi_device,0x2C);
	tft_send_one_u8(tft_spi_device,0x3F);
	tft_send_one_u8(tft_spi_device,0x44);
	tft_send_one_u8(tft_spi_device,0x51);
	tft_send_one_u8(tft_spi_device,0x2F);
	tft_send_one_u8(tft_spi_device,0x1F);
	tft_send_one_u8(tft_spi_device,0x1F);
	tft_send_one_u8(tft_spi_device,0x20);
	tft_send_one_u8(tft_spi_device,0x23);

	tft_send_command(tft_spi_device,0x21);

	tft_send_command(tft_spi_device,0x11); 
	msleep(120); 

	tft_send_command(tft_spi_device,0x29); 
	tft_fill(WHITE);
	printk("init OK!\n");
}

/*字符设备操作函数集，open函数实现*/
static int tft_open(struct inode *inode, struct file *filp)
{
	spi_tft_init(); //初始化显示屏
	return 0;
}


/*字符设备操作函数集，.write函数实现*/
static int tft_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *off)
{
	return 0;
}

//show frambuffer
//framebuffer线程刷屏函数
void show_fb(struct fb_info *fbi, struct spi_device *spi)
{
    int x, y;
    u32 k;
    size_t *p = (u16 *)(fbi->screen_base);
    u16 c;
    u8 *pp;
    u16 *memory;
    memory = kzalloc(240*2*135, GFP_KERNEL);	/* 申请内存 */
    LCD_Address_Set(spi,0,0,LCD_W-1,LCD_H-1);
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
            tft_send_one_u8(spi, c >> 8);
            tft_send_one_u8(spi, c & 0xff);
        }
    }
}
/*字符设备操作函数集，.release函数实现*/
static int tft_release(struct inode *inode, struct file *filp)
{
	tft_send_command(tft_spi_device, 0xae);//关闭显示
	return 0;
}



/*字符设备操作函数集*/
static struct file_operations oled_chr_dev_fops = {
	.owner = THIS_MODULE,
	.open = tft_open,
	.write = tft_write,
	.release = tft_release};

/*----------------平台驱动函数集-----------------*/
static int tft_probe(struct spi_device *spi)
{

	int ret = -1; //保存错误状态码
	printk(KERN_EMERG "\t  match successed  \n");

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
	/*初始化spi*/
	tft_spi_device = spi;
	tft_spi_device->mode = SPI_MODE_2;
	spi_setup(tft_spi_device);
spi_tft_init();
myfb_new(spi); //fb设备初始化
	/*---------------------注册 字符设备部分-----------------*/

	//采用动态分配的方式，获取设备编号，次设备号为0，
	//设备名称为rgb-leds，可通过命令cat  /proc/devices查看
	//DEV_CNT为1，当前只申请一个设备编号
	ret = alloc_chrdev_region(&tft_devno, 0, DEV_CNT, DEV_NAME);
	if (ret < 0)
	{
		printk("fail to alloc tft_devno\n");
		goto alloc_err;
	}

	//关联字符设备结构体cdev与文件操作结构体file_operations
	tft_chr_dev.owner = THIS_MODULE;
	cdev_init(&tft_chr_dev, &oled_chr_dev_fops);

	// 添加设备至cdev_map散列表中
	ret = cdev_add(&tft_chr_dev, tft_devno, DEV_CNT);
	if (ret < 0)
	{
		printk("fail to add cdev\n");
		goto add_err;
	}

	/*创建类 */
	class_tft = class_create(THIS_MODULE, DEV_NAME);

	/*创建设备 DEV_NAME 指定设备名，*/
	device_tft = device_create(class_tft, NULL, tft_devno, NULL, DEV_NAME);

	/*打印oled_spi_device 部分内容*/
	printk("max_speed_hz = %d\n", tft_spi_device->max_speed_hz);
	printk("chip_select = %d\n", (int)tft_spi_device->chip_select);
	printk("bits_per_word = %d\n", (int)tft_spi_device->bits_per_word);
	printk("mode = %02X\n", tft_spi_device->mode);
	printk("cs_gpio = %02X\n", tft_spi_device->cs_gpio);
    	printk("probe ...%s\n", tft_spi_device->modalias);
       
	return 0;

add_err:
	// 添加设备失败时，需要注销设备号
	unregister_chrdev_region(tft_devno, DEV_CNT);
	printk("\n error! \n");
alloc_err:

	return -1;
}

static int tft_remove(struct spi_device *spi)
{
	myfb_del();//注销fb
	/*删除设备*/
	device_destroy(class_tft, tft_devno);		   //清除设备
	class_destroy(class_tft);					   //清除类
	cdev_del(&tft_chr_dev);					   //清除设备号

	unregister_chrdev_region(tft_devno, DEV_CNT); //取消注册字符设备
	return 0;
}



/*指定 ID 匹配表*/
static const struct spi_device_id tft_device_id[] = {
	{"fire,ecspi_oled", 0},
	{}};

/*指定设备树匹配表*/
static const struct of_device_id tft_of_match_table[] = {
	{.compatible = "fire,ecspi_oled"},
	{}};
 

/*spi 总线设备结构体*/
struct spi_driver tft_driver = {
	.probe = tft_probe,
	.remove = tft_remove,
	.id_table = tft_device_id,
	.driver = {
		.name = "ecspi_oled",
		.owner = THIS_MODULE,
		.of_match_table = tft_of_match_table,
	},
};

/*
*驱动初始化函数
*/
static int __init tft_driver_init(void)
{
	int error;
	pr_info("tft_driver_init\n");
	error = spi_register_driver(&tft_driver);
	return error;
}

/*
*驱动注销函数
*/
static void __exit tft_driver_exit(void)
{
	pr_info("tft_driver_exit\n");
	spi_unregister_driver(&tft_driver);
}

module_init(tft_driver_init);
module_exit(tft_driver_exit);

MODULE_LICENSE("GPL");



//MISO----CSI_DATA07  MX6UL_PAD_CSI_DATA07__ECSPI1_MISO  0x10b0
//MOSI----CSI_DATA06  MX6UL_PAD_CSI_DATA06__ECSPI1_MOSI  0x10b0
//SS0-----CSI_DATA05  MX6UL_PAD_CSI_DATA05__ECSPI1_SS0   0x10b0
//SCLK----CSI_DATA04  MX6UL_PAD_CSI_DATA04__ECSPI1_SCLK  0x10b0
//D/C-----CSI_DATA03  MX6UL_PAD_CSI_DATA03__GPIO4_IO24   0x10b0
