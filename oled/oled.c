#include "oled.h"
#include "font.c"

#define WriteCmd(client, cmd) OLED_WriteByte(client, 0x00, cmd)
#define WriteDat(client, data) OLED_WriteByte(client, 0x40, data)

static void OLED_WriteByte(struct i2c_client *client, uint8_t addr, uint8_t data)
{
	uint8_t buf[] = {addr, data};
	i2c_master_send(client, buf, 2);
}

static void OLED_Cls(struct i2c_client *client);

int OLED_Probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	WriteCmd(client, 0xAE); //display off
	WriteCmd(client, 0x20);	//Set Memory Addressing Mode	
	WriteCmd(client, 0x10);	//00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	WriteCmd(client, 0xb0);	//Set Page Start Address for Page Addressing Mode,0-7
	WriteCmd(client, 0xc8);	//Set COM Output Scan Direction
	WriteCmd(client, 0x00); //---set low column address
	WriteCmd(client, 0x10); //---set high column address
	WriteCmd(client, 0x40); //--set start line address
	WriteCmd(client, 0x81); //--set contrast control register
	WriteCmd(client, 0xff); //亮度调节 0x00~0xff
	WriteCmd(client, 0xa1); //--set segment re-map 0 to 127
	WriteCmd(client, 0xa6); //--set normal display
	WriteCmd(client, 0xa8); //--set multiplex ratio(1 to 64)
	WriteCmd(client, 0x3F); //
	WriteCmd(client, 0xa4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	WriteCmd(client, 0xd3); //-set display offset
	WriteCmd(client, 0x00); //-not offset
	WriteCmd(client, 0xd5); //--set display clock divide ratio/oscillator frequency
	WriteCmd(client, 0xf0); //--set divide ratio
	WriteCmd(client, 0xd9); //--set pre-charge period
	WriteCmd(client, 0x22); //
	WriteCmd(client, 0xda); //--set com pins hardware configuration
	WriteCmd(client, 0x12);
	WriteCmd(client, 0xdb); //--set vcomh
	WriteCmd(client, 0x20); //0x20,0.77xVcc
	WriteCmd(client, 0x8d); //--set DC-DC enable
	WriteCmd(client, 0x14); //
	OLED_Cls(client);
	WriteCmd(client, 0xaf); //--turn on oled panel
	return 0;
}

int OLED_Remove(struct i2c_client *client)
{
	printk("remove\n");
	WriteCmd(client, 0xAE);
	return 0;
}

static void OLED_SetPos(struct i2c_client *client, uint8_t x, uint8_t y)
{
	WriteCmd(client, ((x & 0xF0) >> 4) | 0x10);
	WriteCmd(client, x & 0x0F);
	WriteCmd(client, 0xB0 + (y % 8));
}

static void OLED_Cls(struct i2c_client *client)
{
	uint8_t m, n;
	for (m = 0; m < 8; m++)
	{
		WriteCmd(client, 0xB0 + m);
		WriteCmd(client, 0x00);
		WriteCmd(client, 0x10);
		for (n = 0; n < 128; n++)
		{
			WriteDat(client, 0x00);
		}
	}
}

void OLED_Set(struct i2c_client *client, const char *str)
{
	uint8_t i, j, x, y;
	char ch;
	const uint8_t *buf;
	OLED_Cls(client);
	OLED_SetPos(client, 0, 0);
	x = 0;
	y = 0;
	for (i = 0, ch = *str; ch; ch = *(str + ++i))
	{
		if (ch < ' ' || ch > 'z')
		{
			ch = ' ';
		}
		buf = F6x8 + (6 * (ch - 0x20));
		OLED_SetPos(client, x, y);
		for (j = 0; j < 6; j++)
		{
			if (x < 128 && y < 64)
			{
				WriteDat(client, *(buf + j));
			}
			x++;
		}
		if (x + 6 >= 128)
		{
			x = 0;
			y++; // y一个就8行
			if (y >= 64)
			{
				return;
			}
		}
	}
}
