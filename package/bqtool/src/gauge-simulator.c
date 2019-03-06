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

#ifdef GAUGE_SIMULATOR

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define GAUGE_SIM_FILE_MAX_LINE_LEN ((32 + 4) * 3)
const char *gauge_sim_file = "gauge_sim_dm.txt";
int read_params_block(struct csv_info_t *csv, struct gauge_info_t *gauge);
int write_params_block(struct csv_info_t *csv, struct gauge_info_t *gauge);

static uint16_t control_read(struct gauge_info_t *gauge, uint16_t ctrl_cmd)
{
	return 0;
}


static int open_dm_virtual(struct gauge_info_t *gauge, int argc, char **argv)
{
	return 1;
}

static void close_dm_virtual(struct gauge_info_t *gauge, int argc, char **argv)
{
	return;
}

static void reset_virtual(struct gauge_info_t *gauge)
{
	return;
}

long int get_block_line(struct file_info_t *file, char **line,
	uint8_t subclass, uint8_t blk_ind)
{
	long int pos = ftell(file->fd);

	while ((*line = read_line(file))) {
		char *str = *line;
		if (strtoul(str, &str, 16) == subclass &&
			strtoul(str, &str, 16) == blk_ind)
			return pos;
		pos = ftell(file->fd);
	}

	return -1;
}

static int read_dm_block(struct gauge_info_t *gauge, uint8_t subclass,
	uint8_t blk_ind, uint8_t *buf)
{
	char *line, *tmp;
	long int pos;
	int i;

	struct file_info_t *file = create_file(gauge_sim_file, "r",
		GAUGE_SIM_FILE_MAX_LINE_LEN);

	pos = get_block_line(file, &line, subclass, blk_ind);

	if (pos == -1) {
		for (i = 1; i < 33; i++)
			buf[i] = 0;
	} else {
		for (i = 1; i < 33; i++)
			buf[i] = (uint8_t) strtoul(&line[(i - 1) * 3 + 5], &tmp, 16);
	}

	//fclose(file->fd);

	free_file(file);

	return 1;
}

static int write_dm_block(struct gauge_info_t *gauge, uint8_t subclass,
	uint8_t blk_ind, uint8_t *buf)
{
	char *line;
	long int pos, i;

	struct file_info_t *file = create_file(gauge_sim_file, "r+",
		GAUGE_SIM_FILE_MAX_LINE_LEN);

	pos = get_block_line(file, &line, subclass, blk_ind);

	if (pos == -1)
		fseek(file->fd, 0, SEEK_END);
	else
		fseek(file->fd, pos, SEEK_SET);

	fprintf(file->fd, "%02x %02x", subclass, blk_ind);
	for (i = 1; i < 33; i++)
		fprintf(file->fd, " %02x", buf[i]);

	fprintf(file->fd, "\n");
	fflush(file->fd);

	free_file(file);

	return 1;
}

static void sleep_ms_simulator(uint16_t ms)
{
	usleep(ms * 1000);
}

int init_gauge_interface(struct gauge_info_t *gauge, int argc, char **argv)
{
	//struct gauge_info_t *gauge = &csv->gauge;

	gauge->device_num = 0x0545;
	gauge->fw_version = 0x0502;
	gauge->endianness = BYTEORDER_BE;
	gauge->ic = GAUGE_IC_X;
	gauge->write_params = write_params_block;
	gauge->read_params = read_params_block;
	gauge->open_dm = open_dm_virtual;
	gauge->close_dm = close_dm_virtual;
	gauge->reset = reset_virtual;
	gauge->sleep_ms = sleep_ms_simulator;

	return 1;
}
#endif
