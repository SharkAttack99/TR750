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
#ifndef __BQT_COMMON_H_
#define __BQT_COMMON_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

/********************************************************
 * Compile-time feature selection
 *******************************************************/
/* Enable this flag for debug prints */
//#define BQTOOL_DEBUG

/*
 * Commands supported. Remove unused commands
 * if necessary to save memory
 */
#define CMD_IMPORT_GG_CSV
#define CMD_EXPORT_GG_CSV
#define CMD_EXPORT_REGS
#define CMD_COMBINED_EXPORT
#define CMD_BQFS_FLASH
#define CMD_ROM_GAUGE_MONITOR

/*
 * Gauge simulator for testing the front-end on a PC
 * using a file to store DM content.Only limited set
 * of features supported
 */
//#define GAUGE_SIMULATOR

#ifndef GAUGE_SIMULATOR

/*
 * COMM Interface:
 * Select only one of the below
 */
#define GAUGE_COMM_IF_LINUX_I2C
/* #define GAUGE_COMM_IF_LINUX_HDQ - not supported, just for example */

/*
 * Gauge families supported:
 * Remove those you do not want if you want to save memory.
 * But make sure to leave at least one enabled.
 */
#define BQ8032
#define BQ8034
#define BQ8035
#define BQ8036
#define BQ8037
#define BQ8101

#endif /* #ifndef GAUGE_SIMULATOR */

/********************************************************
 * utily macros for message logging
 *******************************************************/
void print(FILE *out, const char *fmt, ...);
#ifdef BQTOOL_DEBUG
#define pr_dbg(...)	{ print(stderr, "bqtool: %s(): ", __func__); print(stderr, __VA_ARGS__); }
#define pr_dbg_raw(...) {print(stderr, __VA_ARGS__);}
#else
#define pr_dbg(...)
#define pr_dbg_raw(...)
#endif
#define pr_info(...)	{ print(stdout, "bqtool: %s(): ", __func__); print(stdout, __VA_ARGS__); }
#define pr_err(...)	{ print(stderr, "bqtool: %s(): ", __func__); print(stderr, __VA_ARGS__); }
#define pr_err_raw(...)	{ print(stderr, __VA_ARGS__); }


/********************************************************
 * Exported data structures and functions from main.c
 *******************************************************/
/*
 * Buffer allocated while reading in a line. To be on safer side,
 * we will re-allocate with an additional MAX_LINE_LEN_GG_CSV
 * bytes when we run out of buffer, so the real maximum line
 * length is 2 * MAX_LINE_LEN_GG_CSV
 */
#define MAX_LINE_LEN_GG_CSV	300
#define MAX_CSV_FIELDS		30
#define MAX_USED_FLD_LEN	100
#define MAX_NUM_PARAMS		500
#define MAX_NUM_FIELDS		30
#define BQFS_MAX_DATA_BLOCK_LEN	(96 + 4)

struct file_info_t {
	const char *fname;
	FILE	*fd;
	int	line_num;
	char	*line_buf;
	int	max_line_len;
};

enum field_t {
	USED_FLD_NAME = 0,
	USED_FLD_VALUE,
	USED_FLD_MIN_VAL,
	USED_FLD_MAX_VAL,
	USED_FLD_DATATYPE,
	USED_FLD_DATA_LENGTH,
	USED_FLD_ADDRESS_OFFST,
	USED_FLD_DISPLAY_FORMAT,
	USED_FLD_READ_FORMULA,
	USED_FLD_WRITE_FORMULA,
	NUM_USED_FIELDS, /* Always should be the last entry */
};

struct csv_header_t {
	uint16_t		device_num;
	uint16_t		fw_version;
	uint8_t			fw_ver_bld;
	int			num_fields;
	struct file_info_t	*file;
	char			**field_names;
};

enum val_type_t {
	DATATYPE_I = 0,
	DATATYPE_U,
	DATATYPE_F,
	DATATYPE_S,
};

enum param_csv_format_t {
	DISP_FMT_I,
	DISP_FMT_H,
	DISP_FMT_F,
	DISP_FMT_S,
};

union val_t {
	int32_t		i;
	uint32_t	u;
	float		f;
};

enum endianness_t {
	BYTEORDER_LE = 1,
	BYTEORDER_BE
};

enum gauge_ic_type {
	GAUGE_IC_X = 1,
	GAUGE_IC_B
};

enum gauge_op_t {
	OP_DM_READ = 1,
	OP_DM_WRITE,
	OP_BQFS_FLASH,
	OP_REG_DUMP
};

struct csv_info_t;
struct gauge_info_t {
	uint16_t	device_num;
	uint16_t	fw_version;
	uint8_t		fw_ver_bld;
	uint32_t	family;
	uint8_t		slave_addr;
	void		*interface;
	uint8_t		orig_seal_status;
	uint8_t		endianness;
	uint32_t	unseal_key;
	uint32_t	fullacccess_key;
	uint8_t		unseal_key_found;
	uint8_t		fullaccess_key_found;
	uint8_t		ic;
	uint8_t		op;
	int (*read_params)(struct csv_info_t *csv, struct gauge_info_t *gauge);
	int (*write_params)(struct csv_info_t *csv, struct gauge_info_t *gauge);
	int (*open_dm)(struct gauge_info_t *gauge, int argc, char **argv);
	void (*close_dm)(struct gauge_info_t *gauge, int argc, char **argv);
	void (*reset)(struct gauge_info_t *gauge);
	int (*itpor)(struct gauge_info_t *gauge);
	/* Communication interface - OS depedent */
	int (*read)(struct gauge_info_t *gauge, uint8_t slave_addr, uint8_t *buf, uint8_t len);
	int (*write)(struct gauge_info_t *gauge, uint8_t slave_addr, uint8_t *buf, uint8_t len);
	/* OS communication support */
	int (*lock_gauge_interface)(struct gauge_info_t *gauge);
	int (*unlock_gauge_interface)(struct gauge_info_t *gauge);
	int (*init_comm_interface)(struct gauge_info_t *gauge, int argc, char **argv);
	void (*close_comm_interface)(struct gauge_info_t *gauge);
	void (*sleep_ms)(uint16_t ms);
};

struct param_t {
	uint8_t raw_type;
	uint8_t disp_type;
	uint8_t csv_fraction_len;
	char	*val_s; /* for parameters of datatype 's' */
	const char *fmt_str;
	char *name;
	uint32_t offset;
	union val_t val;
	union val_t min;
	union val_t max;
	struct queue_t *read_formula_expr;
	struct queue_t *write_formula_expr;
	uint8_t data_len;
	char	**fields;
};

struct csv_info_t {
	struct csv_header_t	*header;
	uint8_t			used_field_index[NUM_USED_FIELDS];
	uint32_t		unseal_key;
	uint32_t		fullaccess_key;
	uint8_t			unseal_key_found;
	uint8_t			fullaccess_key_found;
	uint8_t			value_fld_ind;
	struct param_t		*params;
	uint32_t		num_params;
};

char *read_line(struct file_info_t *file);
void free_file(struct file_info_t *file);
struct file_info_t *create_file(const char *fname, const char *mode,
	int max_line_len);
int extract_int(const char *str, int base, long long int *res);
const char *get_cmdline_argument(int argc, char **argv,
	const char *arg_identifier, int arg_with_val);
int update_value_string(struct param_t *param, int value_fld_ind);
int check_limits(struct param_t *param);


/********************************************************
 * Exported data structures and functions from expression-parser.c
 *******************************************************/
struct node_t {
	struct node_t *next;
	struct node_t *prev;
	void *val_p;
};

struct queue_t {
	struct node_t *head;
	struct node_t *tail;
	struct node_t *curr;
};

void delete_queue(struct queue_t *q);
struct queue_t *parse_expression(const char *expr);
double evaluate_expr(struct queue_t *rpn_q, double val);

/********************************************************
 * Exported data structures and functions from gauge.c
 *******************************************************/
/* Registers common */
#define REG_CONTROL		0x00
#define REG_BLOCK_DATA_CLASS	0x3E
#define REG_DATA_BLOCK		0x3F
#define REG_BLOCK_DATA		0x40
#define REG_BLOCK_DATA_CHECKSUM	0x60
#define REG_BLOCK_DATA_CONTROL	0x61

/* CONTROL commands */
#define CTRL_CMD_STATUS		0x0000
#define CTRL_CMD_DEVICE_TYPE	0x0001
#define CTRL_CMD_FW_VERSION	0x0002
#define CTRL_CMD_SEAL		0x0020
#define CTRL_CMD_RESET		0x0041

/* Register bit fields */
#define CTRL_STATUS_SS	(1 << 13)
#define CTRL_STATUS_FAS	(1 << 14)

/* ROM gauge registers */
#define ROM_GAUGE_FLAGS				0x06
#define CTRL_CMD_ROM_GAUGE_SOFT_RESET		0x0042
#define CTRL_CMD_ROM_GAUGE_EXIT_CFGUPDATE	0x0043
#define CTRL_CMD_ROM_GAUGE_SET_CFGUPDATE	0x0013
#define CTRL_CMD_ROM_GAUGE_DM_CODE		0x0004


/* ROM gauge bit fields */
#define ROM_GAUGE_FLAG_ITPOR		0x20
#define ROM_GAUGE_FLAG_CFGUPDATE_MODE	0x10
#define ROM_GAUGE_CTRL_STATUS_INITCOMP	0x80

/* ROM gauge unseal key */
#define ROM_GAUGE_UNSEAL_KEY		0x80008000


int init_gauge_interface(struct gauge_info_t *gauge, int argc, char **argv);
int do_read(struct gauge_info_t *gauge, uint8_t *buf, uint8_t len);
int do_write(struct gauge_info_t *gauge, uint8_t *buf, uint8_t len);
int read_regs(struct csv_info_t *csv, struct gauge_info_t *gauge);

/********************************************************
 * OS/Communication Interface Abstraction
 *******************************************************/
int setup_comm_callbacks(struct gauge_info_t *gauge);
#endif
