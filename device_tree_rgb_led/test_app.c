#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
int main(int argc, char *argv[])
{
	int fd = open("/dev/rgb_led", O_RDWR);
	unsigned char commend;
	printf("led_tiny test\n");  
	while(1)
	{
		commend=1;
		write(fd,&commend,sizeof(commend));
		usleep(100000);
		commend=2;
		write(fd,&commend,sizeof(commend));
		usleep(100000);
		commend=4;
		write(fd,&commend,sizeof(commend));
		usleep(100000);
		commend=8;
		write(fd,&commend,sizeof(commend));
		usleep(100000);
	}
    /*关闭文件*/
    int error = close(fd);
    if(error < 0)
    {
        printf("close file error! \n");
    }
    
    return 0;
}
