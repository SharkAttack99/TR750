/*
 * Copyright (C) 2015 Texas Instruments Inc
 *
 * Aneesh V <aneesh@ti.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "bqt.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define I2C_BUS			0
#define I2C_DEV_FILE_BASE	"/dev/i2c-"
#define I2C_SLAVE_ADDR_DEFUALT	0xAA
#define SYSFS_POLL_INTVL "/sys/module/bq27x00_battery/parameters/poll_interval"


struct i2c_info_t {
	int	i2c_file;
	int	drvr_poll_intvl;
};

static int linux_i2c_read(struct gauge_info_t *gauge, uint8_t slave_addr, uint8_t *buf, uint8_t len)
{
	int ret;
	struct i2c_rdwr_ioctl_data i2c_data;
	/* msg[0] for write command and msg[1] for read command */
	struct i2c_msg msgs[2];
	uint8_t reg = buf[0];
	int i2c_file = ((struct i2c_info_t *)(gauge->interface))->i2c_file;

	/*
	 * Write part
	 */
	/* Linux expects 7 bit slave address */
	msgs[0].addr = slave_addr >> 1;
	/* buf[0] contains reg address */
	msgs[0].buf = buf;
	msgs[0].flags = 0;
	msgs[0].len = 1;

	/*
	 * Read part
	 */
	msgs[1].addr = msgs[0].addr;
	/* data starts from buf[1] */
	msgs[1].buf = &buf[1];
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;

	i2c_data.nmsgs = 2;
	i2c_data.msgs = msgs;
	ret = ioctl(i2c_file, I2C_RDWR, &i2c_data);
	if (ret < 0) {
		fprintf(stderr, "I2C read failed reg %02x len %d err %d\n",
			reg, len, ret);
		return 0;
	}

	return 1;
}

static int linux_i2c_write(struct gauge_info_t *gauge, uint8_t slave_addr, uint8_t *buf, uint8_t len)
{
	int ret;
	struct i2c_rdwr_ioctl_data i2c_data;
	struct i2c_msg msgs[1];
	int i2c_file = ((struct i2c_info_t *)(gauge->interface))->i2c_file;

	msgs[0].addr = slave_addr >> 1;
	/* reg address is part of buf */
	msgs[0].buf = buf;
	msgs[0].flags = 0;
	msgs[0].len = len + 1;

	i2c_data.nmsgs = 1;
	i2c_data.msgs = msgs;
	ret = ioctl(i2c_file, I2C_RDWR, &i2c_data);
	if (ret < 0) {
		fprintf(stderr, "I2C write failed slave %02x reg %02x len %d err %d\n",
			msgs[0].addr, buf[0], len, ret);
		return 0;
	}

	return 1;
}

static int read_bq_poll_intvl(void)
{
	int poll_file = -1;
	int poll_intvl = -1;
	char buf[20];

	poll_file = open(SYSFS_POLL_INTVL, O_RDONLY);

	if ((poll_file >= 0) && read(poll_file, buf, 20)) {
		sscanf(buf, "%d", &poll_intvl);
		pr_dbg("gauge driver poll interval %ds\n", poll_intvl);
	} else {
		pr_err("Failed to read %s\n", SYSFS_POLL_INTVL);
	}

	if (poll_file >= 0)
		close(poll_file);

	return poll_intvl;
}

static int write_bq_poll_intvl(int poll_intvl)
{
	int poll_file = -1;
	char buf[20];
	int old_poll_intvl, new_poll_intvl;

	old_poll_intvl = read_bq_poll_intvl();

	if (old_poll_intvl == poll_intvl) {
		pr_info("polling interval already %ds\n", poll_intvl);
		return 1;
	}


	poll_file = open(SYSFS_POLL_INTVL, O_RDWR);

	if (poll_file >= 0) {
		sprintf(buf, "%d", poll_intvl);
		write(poll_file, buf, 20);
		close(poll_file);
	}

	new_poll_intvl = read_bq_poll_intvl();

	if ((poll_file < 0) || (new_poll_intvl != poll_intvl)) {
		pr_err("failed to set gauge driver polling intvl to %d\n", poll_intvl);
		return 0;
	} else {
		pr_info("changed polling interval from %ds to %ds\n",
			old_poll_intvl, poll_intvl);
		return 1;
	}
}

static int lock_gauge_interface_linux(struct gauge_info_t *gauge)
{
	struct i2c_info_t *i2c = gauge->interface;

	pr_dbg(">>\n");
	i2c->drvr_poll_intvl = read_bq_poll_intvl();

	/*
	 * Turn off polling. This may not be enough to silence the
	 * driver. We may have to expose a new sysfs file to completely
	 * lock the driver out from accessing the gauge.
	 */
	if ((i2c->drvr_poll_intvl > 0) && !write_bq_poll_intvl(0)) {
		pr_err("Failed to stop driver polling, may cause"
			" communication errors during command execution..\n");
		return 0;
	}

	return 1;
}

static int unlock_gauge_interface_linux(struct gauge_info_t *gauge)
{
	struct i2c_info_t *i2c = gauge->interface;

	/* Turn ON polling */
	if ((i2c->drvr_poll_intvl > 0) &&
		!write_bq_poll_intvl(i2c->drvr_poll_intvl)) {
		pr_err("Failed to restore driver polling\n");
		return 0;
	}

	return 1;
}

static int init_linux_i2c_interface(struct gauge_info_t *gauge, int argc, char **argv)
{
	const char *i2c_dev_file, *i2c_bus, *slave_addr;
	char *tmp;
	int ret = 0;
	char buf[100];
	struct i2c_info_t *i2c;

	pr_dbg(">>\n");

	i2c = (struct i2c_info_t *) malloc(sizeof(struct i2c_info_t));
	if (!i2c)
		goto end;

	i2c_dev_file = get_cmdline_argument(argc, argv, "--i2c-dev-file=", 1);
	if (!i2c_dev_file) {
		i2c_bus = get_cmdline_argument(argc, argv, "--i2c-bus=", 1);
		if (i2c_bus) {
			snprintf(buf, 100, "%s%s", I2C_DEV_FILE_BASE, i2c_bus);
		} else {
			snprintf(buf, 100, "%s%d", I2C_DEV_FILE_BASE, I2C_BUS);
			pr_err("I2C dev file not specified - assuming %s\n", buf);
		}
		i2c_dev_file = buf;
	}


	i2c->i2c_file = open(i2c_dev_file, O_RDWR);
	if (i2c->i2c_file < 0) {
		pr_err("Failed to open I2C device %s\n", i2c_dev_file);
		goto end;
	}

	slave_addr = get_cmdline_argument(argc, argv, "--slave-addr=", 1);
	if (slave_addr) {
		gauge->slave_addr = (uint8_t) strtoul(slave_addr, &tmp, 16);
	} else {
		pr_err("slave address not provided, assuming 0x%02x\n",
			I2C_SLAVE_ADDR_DEFUALT);
		gauge->slave_addr = I2C_SLAVE_ADDR_DEFUALT;
	}

	gauge->interface = i2c;
	ret = 1;
end:
	if (!ret)
		pr_err("failed to initialize I2C interface\n");

	pr_dbg("<<\n");
	return ret;
}

static void close_linux_i2c_interface(struct gauge_info_t *gauge)
{
	struct i2c_info_t *i2c = gauge->interface;

	close(i2c->i2c_file);

	free(i2c);

	gauge->interface = NULL;
}

static void sleep_ms(uint16_t ms)
{
	usleep(ms * 1000);
}

int setup_comm_callbacks(struct gauge_info_t *gauge)
{
	gauge->init_comm_interface = init_linux_i2c_interface;
	gauge->close_comm_interface = close_linux_i2c_interface;
	gauge->lock_gauge_interface = lock_gauge_interface_linux;
	gauge->unlock_gauge_interface = unlock_gauge_interface_linux;
	gauge->read = linux_i2c_read;
	gauge->write = linux_i2c_write;
	gauge->sleep_ms = sleep_ms;

	return 1;
}

