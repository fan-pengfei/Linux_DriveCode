#include <linux/i2c.h>

int OLED_Probe(struct i2c_client *client, const struct i2c_device_id *id);
int OLED_Remove(struct i2c_client *client);
void OLED_Set(struct i2c_client *client, const char *str);

