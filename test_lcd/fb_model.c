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
#define X 240
#define Y 135
#define LCD_W 240
#define LCD_H 135
typedef struct {
    struct spi_device *spi; //记录fb_info对象对应的spi设备对象
    struct task_struct *thread; //记录线程对象的地址，此线程专用于把显存数据发送到屏的驱动ic
}lcd_data_t;
struct fb_info *fbi;
void show_fb(struct fb_info *fbi, struct spi_device *spi);
static int tft_lcdfb_setcolreg(unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue,
			     unsigned int transp, struct fb_info *info);
void LCD_Clear(struct ipsTft_dev *dev);
struct fb_ops fops = {
    .owner		= THIS_MODULE,
    .fb_setcolreg	= tft_lcdfb_setcolreg,
    .fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

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

static u32 pseudo_palette[240*240];
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

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
    u32 p_addr;
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
 * @description	: 向ipsTft多个寄存器写入数据
 * @param - dev:  ipsTft设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 ipsTft_write_regs(struct spi_device *spi,u8 *buf, u32 len)
{
    int ret;
	struct spi_message m;
	struct spi_transfer *t;
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	t->tx_buf = buf;			/* 要写入的数据 */
	t->len = len;				/* 写入的字节数 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	kfree(t);					/* 释放内存 */
	return ret;
}

/*
 * @description	: 向ipsTft指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  ipsTft设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */	

static void ipsTft_write_onereg(struct spi_device *spi, u8 buf)
{
	ipsTft_write_regs(spi,&buf, 1);
}

/*
    funciton: 写一个命令
*/
void write_command(struct spi_device *spi, u8 cmd)
{
    // dc , command:0
    gpio_set_value(ipsTftdev.dc_gpio, 0); 
    ipsTft_write_onereg(spi,cmd);
}
/*
    funciton: 写一个数据
*/
void write_data(struct spi_device *spi, u8 data)
{
    gpio_set_value(ipsTftdev.dc_gpio, 1);
    ipsTft_write_onereg(spi,data);
}
/*
    funciton: 写多个数据
*/
static void write_datas(struct spi_device *spi, u8 *data,u32 len)
{
    gpio_set_value(ipsTftdev.dc_gpio, 1);
    ipsTft_write_regs(spi,data,len);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 * 					  一般在open的时候将private_data似有向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ipsTft_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ipsTftdev; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int ipsTft_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* ipsTft操作函数 */
static const struct file_operations ipsTft_ops = {
	.owner = THIS_MODULE,
	.open = ipsTft_open,
	.release = ipsTft_release,
};

void Address_set(struct spi_device *spi,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2)
{ 
    write_command(spi,0x2a);
    write_data(spi,x1>>8);
    write_data(spi,x1);
    write_data(spi,x2>>8);
    write_data(spi,x2);
  
    write_command(spi,0x2b);
    write_data(spi,y1>>8);
    write_data(spi,y1);
    write_data(spi,y2>>8);
    write_data(spi,y2);

    write_command(spi,0x2C);					 						 
}

/*
    刷屏函数
*/
void LCD_Clear(struct ipsTft_dev *dev)
{
	u16 i,j;
     u16 *memory;
     memory = (u16 *)kzalloc(240*240*2, GFP_KERNEL);	/* 申请内存 */
	Address_set(dev->private_data,0,0,LCD_W-1,LCD_H-1);
    write_command(dev->private_data,0x2C);
    msleep(3000);//延迟显示，显示完上面钢铁侠图片
    memcpy(memory,gImage_q,240*240*2);
     int count = 0,split=10;
    while (count < split)//split 用于设置分批发送数据，有时候st7789spi速率跟不上容易丢数据，偶尔会提示spi2 send ....类似的错误
    {
        write_datas(dev->private_data,(u8 *)memory+count*240*240*2/split,240*240*2/split);
        count++;
    }
      kfree(memory);
}
/*
 * ipsTft内部寄存器初始化函数 
 * @param  	: 无
 * @return 	: 无
 */
void ipsTft_reginit(struct ipsTft_dev *dev)
{
     int i, j, n;
    gpio_set_value(ipsTftdev.res_gpio, 0);
    mdelay(20);
    gpio_set_value(ipsTftdev.res_gpio, 1);
    mdelay(20);
    n = 0; // n用于记录数据数组spi_lcd_datas的位置
    //发命令，并发出命令所需的数据
    for (i = 0; i < ARRAY_SIZE(cmds); i++) //命令
    {
        write_command(dev->private_data, cmds[i].reg_addr);
        for (j = 0; j < cmds[i].len; j++) //发出命令后，需要发出的数据
            if(cmds[i].len!=0)
                write_data(dev->private_data, spi_lcd_datas[n++]);
        if (cmds[i].delay_ms) //如有延时则延时
            mdelay(cmds[i].delay_ms);
    }
    n=0;
    LCD_Clear(dev);
    msleep(5000);
    printk("ips init finish!\n");
}

//show frambuffer
//framebuffer线程刷屏函数
void show_fb(struct fb_info *fbi, struct spi_device *spi)
{
    int x, y;
    u32 k;
    u32 *p = (u16 *)(fbi->screen_base);
    u16 c;
    u8 *pp;
    u16 *memory;
    memory = kzalloc(240*2*240, GFP_KERNEL);	/* 申请内存 */
    Address_set(spi,0,0,LCD_W-1,LCD_H-1);
    write_command(spi,0x2C);
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
            *((u16 *)memory+y*fbi->var.yres+x) = ((c&0xff)<<8)|((c&0xff00)>>8);
        }
    }
    //split 用于设置分批发送数据，有时候st7789spi速率跟不上容易丢数据，偶尔会提示spi2 send ....类似的错误
    int count = 0,split=1;  
    while (count < split)
    {
        write_datas(spi,(u8 *)memory+count*fbi->var.yres*fbi->var.xres*2/split,fbi->var.yres*fbi->var.xres*2/split);
        count++;
    }
    kfree(memory);
}

 /*
  * @description     : spi驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : spi设备
  * @param - id      : spi设备ID
  * 
  */	
static int ipsTft_probe(struct spi_device *spi)
{
	int ret = 0;

	/* 1、构建设备号 */
	if (ipsTftdev.major) {
		ipsTftdev.devid = MKDEV(ipsTftdev.major, 0);
		register_chrdev_region(ipsTftdev.devid, ipsTft_CNT, ipsTft_NAME);
	} else {
		alloc_chrdev_region(&ipsTftdev.devid, 0, ipsTft_CNT, ipsTft_NAME);
		ipsTftdev.major = MAJOR(ipsTftdev.devid);
	}

	/* 2、注册设备 */
	cdev_init(&ipsTftdev.cdev, &ipsTft_ops);
	cdev_add(&ipsTftdev.cdev, ipsTftdev.devid, ipsTft_CNT);

	/* 3、创建类 */
	ipsTftdev.class = class_create(THIS_MODULE, ipsTft_NAME);
	if (IS_ERR(ipsTftdev.class)) {
		return PTR_ERR(ipsTftdev.class);
	}

	/* 4、创建设备 */
	ipsTftdev.device = device_create(ipsTftdev.class, NULL, ipsTftdev.devid, NULL, ipsTft_NAME);
	if (IS_ERR(ipsTftdev.device)) {
		return PTR_ERR(ipsTftdev.device);
	}

	/* 获取设备树中cs片选信号 */
	ipsTftdev.nd = of_find_node_by_path("/soc/aips-bus@02000000/spba-bus@02000000/ecspi@02010000");
	if(ipsTftdev.nd == NULL) {
		printk("ecspi3 node not find!\r\n");
		return -EINVAL;
	}
	/* 2、 获取设备树中的gpio属性，得到BEEP所使用的BEEP编号 */
	ipsTftdev.cs_gpio = of_get_named_gpio(ipsTftdev.nd, "cs-gpio", 0);
	if(ipsTftdev.cs_gpio < 0) {
		printk("can't get cs-gpio");
		return -EINVAL;
	}
    ipsTftdev.nd = of_find_node_by_path("/ipsRes");
	if(ipsTftdev.nd == NULL) {
		printk("res-gpio node not find!\r\n");
		return -EINVAL;
        }
    ipsTftdev.res_gpio = of_get_named_gpio(ipsTftdev.nd, "res-gpio", 0);
    if(ipsTftdev.res_gpio < 0) {
		printk("can't get res-gpio");
		return -EINVAL;
	}

    ipsTftdev.nd = of_find_node_by_path("/ipsDc");
	if(ipsTftdev.nd == NULL) {
		printk("ipsDcgpio node not find!\r\n");
		return -EINVAL;
        }
    ipsTftdev.dc_gpio = of_get_named_gpio(ipsTftdev.nd, "dc-gpio", 0);
    if(ipsTftdev.dc_gpio < 0) {
		printk("can't get ipsDc-gpio");
		return -EINVAL;
	}

	/* 3、设置GPIO1_IO20为输出，并且输出高电平 */
	ret = gpio_direction_output(ipsTftdev.cs_gpio, 1);//失能板子上的磁力计
	if(ret < 0) {
		printk("can't set cs gpio!\r\n");
	}
    gpio_set_value(ipsTftdev.cs_gpio,1);
    ret = gpio_direction_output(ipsTftdev.res_gpio, 1);
	if(ret < 0) {
		printk("can't set res gpio!\r\n");
	}
    ret = gpio_direction_output(ipsTftdev.dc_gpio, 1);
	if(ret < 0) {
		printk("can't set dc gpio!\r\n");
	}

	/*初始化spi_device */
	spi->mode = SPI_MODE_2;	/*MODE0，CPOL=0，CPHA=0 //出问题的地方!!!*/
	spi_setup(spi);
	ipsTftdev.private_data = spi; /* 设置私有数据 */

	/* 初始化ipsTft内部寄存器 */
	ipsTft_reginit(&ipsTftdev);
    myfb_new(spi); //fb设备初始化		
	return 0;
}

/*
 * @description     : spi驱动的remove函数，移除spi驱动的时候此函数会执行
 * @param - client 	: spi设备
 * @return          : 0，成功;其他负值,失败
 */
static int ipsTft_remove(struct spi_device *spi)
{
    myfb_del();//注销fb

	/* 删除设备 */
	cdev_del(&ipsTftdev.cdev);
	unregister_chrdev_region(ipsTftdev.devid, ipsTft_CNT);

	/* 注销掉类和设备 */
	device_destroy(ipsTftdev.class, ipsTftdev.devid);
	class_destroy(ipsTftdev.class);
   
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id ipsTft_id[] = {
	{"alientek,ipsTft", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id ipsTft_of_match[] = {
	{ .compatible = "alientek,ipsTft" },
	{ /* Sentinel */ }
};

/* SPI驱动结构体 */	
static struct spi_driver ipsTft_driver = {
	.probe = ipsTft_probe,
	.remove = ipsTft_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "ipsTft",
		   	.of_match_table = ipsTft_of_match, 
		   },
	.id_table = ipsTft_id,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ipsTft_init(void)
{
	return spi_register_driver(&ipsTft_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ipsTft_exit(void)
{
	spi_unregister_driver(&ipsTft_driver);
}

module_init(ipsTft_init);
module_exit(ipsTft_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("raymond");

