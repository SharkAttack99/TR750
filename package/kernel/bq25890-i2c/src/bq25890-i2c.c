#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#define I2C_RETRY_CNT    3
#define bq25890_I2C_ADDR   (0xD4 >> 1)

static DEFINE_MUTEX(battery_mutex);

struct charger_device_info {
    struct  device        *dev;
    int     id;
};

static const struct of_device_id bq25890_of_match[] = {
	{.compatible = "bq25890-i2c"},
};
MODULE_DEVICE_TABLE(of, bq25890_of_match);

static const struct i2c_device_id bq25890_i2c_id[] = {
	{"bq25890", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, bq25890_i2c_id);

static unsigned char bq_read_i2c_byte(struct charger_device_info *di, u8 reg)
{
    struct i2c_client *client = to_i2c_client(di->dev);
    struct i2c_msg msg[2];
    unsigned char data;
    unsigned char ret;
    int i = 0;

    if (!client->adapter)
        return -ENODEV;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].buf = &reg;
    msg[0].len = sizeof(reg);
    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = &data;
    msg[1].len = 1;

    mutex_lock(&battery_mutex);
    for(i = 0; i < I2C_RETRY_CNT; i++){
        ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
        if(ret >= 0) break;
        msleep(5);
    }
    mutex_unlock(&battery_mutex);
    if (ret < 0)
        return ret;

    ret = data;

    return ret;
}

static int bq_write_i2c_byte(struct charger_device_info *di, u8 reg, unsigned char value)
{
    struct i2c_client *client = to_i2c_client(di->dev);
    struct i2c_msg msg;
    unsigned char data[4];
    int ret;
    int i = 0;

    if (!client->adapter)
        return -ENODEV;

    data[0] = reg;
    data[1] = value;
    
    msg.len = 2;
    msg.buf = data;
    msg.addr = client->addr;
    msg.flags = 0;

    mutex_lock(&battery_mutex);
    for(i = 0; i < I2C_RETRY_CNT; i++){
        ret = i2c_transfer(client->adapter, &msg, 1);
        if(ret >= 0) break;
        msleep(5);
    }
    mutex_unlock(&battery_mutex);    
    if (ret < 0)
        return ret;

    return 0;
}

static int bq25890_config_interface(struct charger_device_info *di, u8 reg, unsigned char value, unsigned char mask, unsigned char shift)
{
	unsigned char bq25890_reg = 0;
	unsigned char bq25890_reg_ori = 0;
	unsigned int ret = 0;
	
	/*
	struct i2c_client *client = to_i2c_client(di->dev);

	client->addr = bq25890_I2C_ADDR; 
	*/
	bq25890_reg = bq_read_i2c_byte(di, reg);

	bq25890_reg_ori = bq25890_reg;

	bq25890_reg &= ~(mask << shift);
	bq25890_reg |= (value << shift);

	ret = bq_write_i2c_byte(di, reg, bq25890_reg);
	if (ret < 0) {
		dev_err(di->dev, "Failed to write data to reg[%x]\n", reg);
	} else {
		dev_info(di->dev, "write Reg[%x]=0x%x from 0x%x\n", reg, bq25890_reg, bq25890_reg_ori);
	}

	return ret;
}

static void bq25890_dump_register(struct charger_device_info *di)
{
	int i = 0;
	/*
	struct i2c_client *client = to_i2c_client(di->dev);
	client->addr = bq25890_I2C_ADDR; 
	*/

	for (i = 0; i < 3; i++) {
		int j;
		unsigned char bq25890_reg_buf[7];
		memset(bq25890_reg_buf, 0x0, sizeof(bq25890_reg_buf));
		for(j = 0; j < 7; j++) {
			bq25890_reg_buf[j] = bq_read_i2c_byte(di, (i*7)+j);
		}
		dev_info(di->dev, "[bq25890 reg@][0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x\n", \
									(i*7)+0, bq25890_reg_buf[0], (i*7)+1, bq25890_reg_buf[1], (i*7)+2, bq25890_reg_buf[2], \
														(i*7)+3, bq25890_reg_buf[3], (i*7)+4, bq25890_reg_buf[4], (i*7)+5, bq25890_reg_buf[5], \
																			(i*7)+6, bq25890_reg_buf[6]);
	}
}

static void bq25890_charger_hw_init(struct charger_device_info *di)
{
    int ret;
	/*
	struct i2c_client *client = to_i2c_client(di->dev);

	client->addr = bq25890_I2C_ADDR; 
	*/
	ret = bq_write_i2c_byte(di, 0x00, 0x66);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x01, 0x0a);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x02, 0x10);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x03, 0x1a);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x04, 0x19);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x05, 0x11);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x06, 0x5c);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x07, 0x9d);
    mdelay(2);
#if 0
	ret = bq_write_i2c_byte(di, 0x09, 0xc4);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x0a, 0x74);
    mdelay(2);
	ret = bq_write_i2c_byte(di, 0x0d, 0x92);
    mdelay(2);
#endif
	/* ret = bq_write_i2c_byte(di, 0x09, 0x64); */
    if(ret < 0) {
        dev_err(di->dev, "Failed to bq25890_charger_hw_init \n");
	}

	bq25890_dump_register(di);
}

static int bq25890_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct charger_device_info *di;

	printk(KERN_INFO "[bq25890_charger_probe]\n");

	di = kzalloc(sizeof(*di), GFP_KERNEL);
    if (!di) {
        dev_err(&client->dev, "failed to allocate device info data\n");
        ret = -ENOMEM;
		return ret;
    }

    di->dev = &client->dev;

    i2c_set_clientdata(client, di);

	bq25890_charger_hw_init(di);

	return ret;
}

static struct i2c_driver bq25890_driver = {
	.driver = {
		   .name = "bq25890-i2c",
#ifdef CONFIG_OF
		   .of_match_table = bq25890_of_match,
#endif
		   },
	.probe = bq25890_charger_probe,
	.id_table = bq25890_i2c_id,
};

static int bq25890_charger_init(void)
{
	int ret = 0;

	printk(KERN_INFO "bq25890 module init\n");

	ret = i2c_add_driver(&bq25890_driver);
	if (ret) {
		printk(KERN_ERR "Unable to register bq25890_charger i2c driver\n");
	}

	return ret;
}

static int __init bq25890_init(void)
{
	int ret = 0;

	ret = bq25890_charger_init();

	return ret;
}

static void bq25890_charger_exit(void)
{
	i2c_del_driver(&bq25890_driver);
}

static void __exit bq25890_exit(void)
{
	bq25890_charger_exit();
}

module_init(bq25890_init);
module_exit(bq25890_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25890 Driver");
MODULE_AUTHOR("Kyson Lok <kysonlok@gmail.com>");
