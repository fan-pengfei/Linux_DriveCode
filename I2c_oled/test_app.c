#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
//宏定义
#define X_WIDTH 		128  //oled显示屏列数
#define Y_WIDTH 		64   //oled显示屏行数

/*数据发送结构体*/
typedef struct oled_display_struct
{
	unsigned char x;
	unsigned char y;	
	unsigned char TextSize;
	unsigned char ch[128]; 

}oled_display_struct;
unsigned char temp_str[128]="Hello,World";
int main(int argc, char *argv[])
{
    /*打开文件*/
    int fd = open("/dev/I2C1_oled", O_RDWR);
    int i=0;
    if(fd < 0)
    {
			printf("open file : %s failed !\n", argv[0]);
			return -1;
    }
	
    struct oled_display_struct *display_struct = NULL;
    display_struct = malloc(sizeof(oled_display_struct) + 128 * 64);    
    if (display_struct == NULL)
    {
        printf("oled_fill malloc error \n");
        return -1;
    }
    display_struct->x=0;
    display_struct->y=0; 
    display_struct->TextSize=2;
    for(i=0;i<128;i++)
    {
			temp_str[i]=' ';
    }
    memcpy(display_struct->ch, temp_str,sizeof(temp_str));
    write(fd, display_struct, sizeof(oled_display_struct) + sizeof(temp_str));
    
    memcpy(display_struct->ch, "Hello,Xiaofei!",15);
    write(fd, display_struct, sizeof(oled_display_struct) + sizeof(temp_str));

    display_struct->x=0;
    display_struct->y=2; 
    display_struct->TextSize=2;

		while(1)
		{
    	i++;
    	sprintf((unsigned char *)temp_str,"num:%d",i);
    	memcpy(display_struct->ch, temp_str,sizeof(temp_str));
    	write(fd, display_struct, sizeof(oled_display_struct) + sizeof(temp_str));
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
