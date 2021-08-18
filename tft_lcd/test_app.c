#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#define X_WIDTH 		240  //oled显示屏列数
#define Y_WIDTH 		135   //oled显示屏行数
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define BRED             0XF81F
#define GRED 			       0XFFE0
#define GBLUE			       0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			     0XBC40 //棕色
#define BRRED 			     0XFC07 //棕红色
#define GRAY  			     0X8430 //灰色
//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上三色为PANEL的颜色 
 
#define LIGHTGREEN     	 0X841F //浅绿色
#define LGRAY 			     0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)
/*数据发送结构体*/
typedef struct tft_display_struct
{
	unsigned char x;
	unsigned char y;	
	unsigned char TextSize;
	unsigned char ch[128]; 
	unsigned int color;

}tft_display_struct;
unsigned char temp_str[128]="Hello,World";
int main(int argc, char *argv[])
{
unsigned int i=0;
    /*打开文件*/
    int fd = open("/dev/ecspi_tft", O_RDWR);
    if(fd < 0)
    {
			printf("open file : %s failed !\n", argv[0]);
			return -1;
    }

   struct tft_display_struct *display_struct = NULL;
    display_struct = malloc(sizeof(tft_display_struct) + 240 * 135);    
    if (display_struct == NULL)
    {
        printf("tft_fill malloc error \n");
        return -1;
    }
    display_struct->x=0;
    display_struct->y=0; 
    display_struct->TextSize=10;
    display_struct->color=RED;
    for(i=0;i<128;i++)
    {
			temp_str[i]=' ';
    }
    memcpy(display_struct->ch, temp_str,sizeof(temp_str));
    write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));
    
    memcpy(display_struct->ch, "Hello,Xiaofei!",15);
    write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));

    display_struct->x=0;
    display_struct->y=20; 
    display_struct->TextSize=2;
while(1)
{
    	i++;
    	sprintf((unsigned char *)temp_str,"num:%d",i);
    	memcpy(display_struct->ch, temp_str,sizeof(temp_str));
    display_struct->y=20; 
	display_struct->color=RED;
    	write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));
    display_struct->y=40; 
	display_struct->color=BLACK;
    	write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));
    display_struct->y=60; 
	display_struct->color=BLUE;
    	write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));
    display_struct->y=80; 
	display_struct->color=GRAY;
    	write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));
    display_struct->y=100; 
	display_struct->color=GREEN;
    	write(fd, display_struct, sizeof(tft_display_struct) + sizeof(temp_str));
    	usleep(1000);
	}

    /*关闭文件*/
    int error = close(fd);
    if(error < 0)
    {
        printf("close file error! \n");
    }
    
    return 0;
}
