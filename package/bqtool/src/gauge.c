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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define SEAL_UNSEAL_POLLING_LIMIT_MS	1000

int do_read(struct gauge_info_t *gauge, uint8_t *buf, uint8_t len)
{
	int i = 0;
	uint8_t addr = gauge->slave_addr;

	while (i++ < 4) {
		if (gauge->read(gauge, addr, buf, len))
			return 1;
		gauge->sleep_ms(10);
		pr_err("Retrying read..\n")
	}


	return 0;
}

int do_write(struct gauge_info_t *gauge, uint8_t *buf, uint8_t len)
{
	int i = 0;
	uint8_t addr = gauge->slave_addr;

	while (i++ < 4) {
		if (gauge->write(gauge, addr, buf, len))
			return 1;
		gauge->sleep_ms(10);
		pr_err("Retrying write..\n")
	}

	return 0;
}

static uint16_t read_word(struct gauge_info_t *gauge, uint8_t reg)
{
	/* Let's not assume Endianness */
	uint8_t buf[3];
	uint16_t reg_val = 0;

	buf[0] = reg;

	if (do_read(gauge, buf, 2)) {
		reg_val = buf[2] << 8;
		reg_val |= buf[1];
	}

	return reg_val;
}

static int write_word(struct gauge_info_t *gauge, uint8_t reg, uint16_t reg_val)
{
	/* Let's not assume Endianness */
	uint8_t buf[3];

	buf[0] = reg;
	buf[1] = (uint8_t) (reg_val & 0xFF);
	buf[2] = (uint8_t) ((reg_val & 0xFF00) >> 8);


	return do_write(gauge, buf, 2);
}

static uint8_t read_byte(struct gauge_info_t *gauge, uint8_t reg)
{
	/* Let's not assume Endianness */
	uint8_t buf[2];

	buf[0] = reg;

	if (do_read(gauge, buf, 1))
		return buf[1];

	return 0;
}

static int write_byte(struct gauge_info_t *gauge, uint8_t reg, uint8_t val)
{
	/* Let's not assume Endianness */
	uint8_t buf[2];

	buf[0] = reg;
	buf[1] = val;

	return do_write(gauge, buf, 1);
}

static void control_write(struct gauge_info_t *gauge, uint16_t val)
{
	write_word(gauge, REG_CONTROL, val);
}

static uint16_t control_read(struct gauge_info_t *gauge, uint16_t ctrl_cmd)
{
	control_write(gauge, ctrl_cmd);

	gauge->sleep_ms(5);

	return read_word(gauge, REG_CONTROL);
}

static uint16_t control_status(struct gauge_info_t *gauge)
{
	return control_read(gauge, CTRL_CMD_STATUS);
}

static uint16_t device_type(struct gauge_info_t *gauge)
{
	return control_read(gauge, CTRL_CMD_DEVICE_TYPE);
}

static uint16_t fw_version(struct gauge_info_t *gauge)
{
	return control_read(gauge, CTRL_CMD_FW_VERSION);
}

static int sealed(struct gauge_info_t *gauge)
{
	return control_status(gauge) & CTRL_STATUS_SS;
}

static int fullaccess_sealed(struct gauge_info_t *gauge)
{
	if (gauge->family == 0x8101)
		return 0;

	return control_status(gauge) & CTRL_STATUS_FAS;
}

static int seal(struct gauge_info_t *gauge)
{
	int i = 0;
	if (sealed(gauge))
		return 1;

	control_write(gauge, CTRL_CMD_SEAL);

	while (i < SEAL_UNSEAL_POLLING_LIMIT_MS/10) {
		i++;
		if (sealed(gauge)) {
			pr_info("sealed the device\n");
			break;
		}
		gauge->sleep_ms(10);
	}

	if (i == SEAL_UNSEAL_POLLING_LIMIT_MS / 10) {
		pr_err("sealing failed\n");
		return 0;
	}

	return 1;
}


static int do_unseal(struct gauge_info_t *gauge, uint32_t key, int fullaccess)
{
	int i = 0;

	if ((fullaccess && !fullaccess_sealed(gauge)) ||
		(!fullaccess && !(sealed(gauge))))
		return 1;


	while (i < SEAL_UNSEAL_POLLING_LIMIT_MS/20) {
		control_write(gauge, (uint16_t) (key & 0xFFFF));
		gauge->sleep_ms(5);
		control_write(gauge, (uint16_t) ((key & 0xFFFF0000) >> 16));
		gauge->sleep_ms(5);
		if ((fullaccess && !fullaccess_sealed(gauge)) ||
			(!fullaccess && !(sealed(gauge)))) {
			pr_info("unsealed %s the device..\n",
				fullaccess ? "fullaccess" : "");
			break;
		}
		gauge->sleep_ms(10);
		i++;
	}

	if (i == SEAL_UNSEAL_POLLING_LIMIT_MS / 20) {
		pr_err("unsealing %s failed!\n",
			fullaccess ? "fullaccess" : "");
		return 0;
	}

	return 1;
}

static int unseal(struct gauge_info_t *gauge)
{
	return do_unseal(gauge, gauge->unseal_key, 0);
}

static int unseal_fullaccess(struct gauge_info_t *gauge)
{
	return do_unseal(gauge, gauge->fullacccess_key, 1);
}

static uint8_t native_endianness() {
    int i = 1;
    char *p = (char *)&i;

    if (p[0] == 1)
        return BYTEORDER_LE;
    else
        return BYTEORDER_BE;
}

static void reverse_byteorder(uint8_t *val, int len)
{
	int i;
	uint8_t tmp;

	for (i = 0; i < len / 2; i++) {
		tmp = val[i];
		val[i] = val[len - i - 1];
		val[len - i - 1] = tmp;
	}
}

static void copy_reverse_byteorder(uint8_t *dest, uint8_t *src, int len)
{
	int i;

	for (i = 0; i < len; i++)
		dest[i] = src[len - i - 1];
}

/*
 * Convert from a X single to an IEEE754 floating point
 * number in Little Endian format
 */
float x_float_to_ieee(uint8_t *data)
{
	float res;
	uint8_t *pK;

	pK=(unsigned char*)&res;

	*pK=*(data+3);
	*(pK+1)=*(data+2);
	*(pK+2)=*(data+1) & 0x7f;
	*(pK+3)=((*data) >> 1 )- 1;

	if(*data & 0x01) *(pK+2) |= 0x80;

	if(*(data+1) & 0x80) *(pK+3) |= 0x80;

	return res;
}


/* Convert from IEEE754 single to a x float */
static void ieee_float_to_x(uint8_t *dest, float ff)
{
	unsigned char *f;
	f=(unsigned char*)&ff;

	*dest=((*(f+3))<<1)+2;
	*(dest+1)=(*(f+2)) & 0x7f;
	*(dest+2)=*(f+1);
	*(dest+3)=*f;

	if(*(f+3) & 0x80)
	*(dest+1) |= 0x80;

	if(*(f+2) & 0x80)
	*dest |=1;
}

static uint8_t checksum(uint8_t *data)
{
	uint16_t sum = 0;
	int i;

	for (i = 0; i < 32; i++)
		sum += data[i];

	sum &= 0xFF;

	return 0xFF - sum;
}

/*
 * !!!!! buf should be 33 bytes long !!!!!
 * 1 byte for the register address and 32 bytes for data,
 * write made zero-copy this way
 */
static int do_rw_dm_block(struct gauge_info_t *gauge, uint8_t subclass,
	uint8_t blk_ind, uint8_t *buf, int write, uint32_t delay)
{
	int err = 0;
	uint16_t cksum, cksum_calc;

	err |= !write_byte(gauge, REG_BLOCK_DATA_CONTROL, 0);
	err |= !write_byte(gauge, REG_BLOCK_DATA_CLASS, subclass);
	err |= !write_byte(gauge, REG_DATA_BLOCK, blk_ind);
	gauge->sleep_ms(5);
	if (write) {
		buf[0] = REG_BLOCK_DATA;
		err |= !do_write(gauge, buf, 32);

		/* Write checksum - this is where we need a big delay */
		cksum_calc = checksum(&buf[1]);
		err |= !write_byte(gauge, REG_BLOCK_DATA_CHECKSUM, cksum_calc);
		gauge->sleep_ms(delay);

		/* Readback checksum and compare */
		err |= !write_byte(gauge, REG_DATA_BLOCK, blk_ind);
		gauge->sleep_ms(5);
		cksum = read_byte(gauge, REG_BLOCK_DATA_CHECKSUM);
		if (cksum != cksum_calc) {
			pr_err("checksum failure on write cksum 0x%02x cksum_calc 0x%02x\n",
				cksum, cksum_calc);
			err = 1;
		}
	} else {
		buf[0] = REG_BLOCK_DATA;
		err |= !do_read(gauge, buf, 32);

		/* Read checksum and compare */
		cksum = read_byte(gauge, REG_BLOCK_DATA_CHECKSUM);
		cksum_calc = checksum(&buf[1]);
		if (cksum != cksum_calc) {
			pr_err("checksum failure on read cksum 0x%02x cksum_calc 0x%02x\n",
				cksum, cksum_calc);
			err = 1;
		}
	}

	if (err) {
		pr_err("error accessing subclass 0x%02x blk_ind 0x%02x write %d\n",
			subclass, blk_ind, write);
	}

	return !err;
}

static int rw_dm_block(struct gauge_info_t *gauge, uint8_t subclass,
	uint8_t blk_ind, uint8_t *buf, int write)
{
	int i = 0;
	uint32_t delay = 300;

	while ((i++ < 5)) {
		if (do_rw_dm_block(gauge, subclass, blk_ind, buf, write, delay))
			return 1;

		/* increase the delay by 100ms at every step */
		delay += 100;
		pr_err("retrying %s block..\n", write == 1 ? "write" : "read");
	}

	return 0;
}

static int read_dm_block(struct gauge_info_t *gauge, uint8_t subclass,
	uint8_t blk_ind, uint8_t *buf)
{
	int ret = rw_dm_block(gauge, subclass, blk_ind, buf, 0);

	return ret;
}

static int write_dm_block(struct gauge_info_t *gauge, uint8_t subclass,
	uint8_t blk_ind, uint8_t *buf)
{
	return rw_dm_block(gauge, subclass, blk_ind, buf, 1);
}

int read_params_block(struct csv_info_t *csv, struct gauge_info_t *gauge)
{
	unsigned int i;
	int err = 0;
	struct param_t *params = csv->params;
	uint8_t subclass, blk_ind, offset;
	uint16_t blk_id, blk_id_prev = 0;
	uint8_t buf[65], tmp;
	union val_t tmp_val;
	uint8_t host_endianness = native_endianness();
	int read_twoblocks = 0;

	pr_info("reading params..\n");

	for (i = 0; !err && i < csv->num_params; i++) {
		subclass = (uint8_t) ((params[i].offset & 0xFF000000) >> 24);
		offset = (uint8_t) (params[i].offset & 0xFF);

		/* block number within subclass */
		blk_ind = offset >> 5;

		/* offset within the block */
		offset = offset & 0x1F;
		if (offset + params[i].data_len > 32) {
			/*
			 * Parameter overflowing to the next block.
			 * Read that block too
			 */
			read_twoblocks = 1;
			tmp = buf[32];
			err |= !read_dm_block(gauge, subclass, blk_ind + 1, &buf[32]);
			buf[32] = tmp;
		}

		/*
		 * unique id for the block:
		 * combination of subclass and blk_ind. Useful to uniquely
		 * identify the blocks and figure out when it's time to read
		 * the next block
		 */
		blk_id = (subclass << 8) | blk_ind;

		if (i == 0 || blk_id != blk_id_prev) {
			err |= !read_dm_block(gauge, subclass, blk_ind, buf);
			if (err)
				goto end;
		}

		/*
		 * note "offset + 1" below. buf[0] contains the
		 * reg address: BLOCK_DATA
		 * x_float_to_ieee returns the output in IEEE little Endian,
		 * so convert to Big Endian if the host is Big Endian.
		 */
		tmp_val = params[i].val;

		/*
		 * zero it out before reading new value
		 * Due to the way we are printing them it's important
		 * not to have any stale bits at the higher nibbles for
		 * 16-bit and 8-bit values. Stale bits are possible
		 * because we do not limit check in the input file
		 * in case of export
		 */
		params[i].val.u = 0;
		if (params[i].raw_type == DATATYPE_F) {
			params[i].val.f = x_float_to_ieee(&buf[offset + 1]);
			if (host_endianness == BYTEORDER_BE)
				reverse_byteorder((uint8_t*) &params[i].val, params[i].data_len);
		} else if (params[i].raw_type == DATATYPE_S) {
			memcpy(params[i].val_s, &buf[offset + 1], params[i].data_len);
		} else if (gauge->endianness != host_endianness) {
			/* Fix byte ordering (Endianness) for integral numerical types */
			copy_reverse_byteorder((uint8_t*) &params[i].val, &buf[offset + 1], params[i].data_len);
		} else {
			memcpy(&params[i].val, &buf[offset + 1], params[i].data_len);
		}

		if (params[i].raw_type == DATATYPE_S) {
			/* Null terminate at the end of buffer for safety */
			params[i].val_s[params[i].data_len] = '\0';
			/* Put length at first byte of the buffer */
			params[i].val_s[0] = (uint8_t) strlen(&params[i].val_s[1]);
		}

		/*
		 * When we are exporting, we check the limits,
		 * but do not abort the operation if there is
		 * an error
		 */
		check_limits(&params[i]);

		/* Update the value string if a binary comparison with old value fails */
		if (tmp_val.u != params[i].val.u)
			err |= !update_value_string(&params[i], csv->value_fld_ind);

		blk_id_prev = blk_id;
		if (read_twoblocks) {
			/* now use the overflow buffer as the regular buffer */
			memcpy(&buf[1], &buf[33], 32);
			blk_id_prev++;
			read_twoblocks = 0;
		}
	}

end:
	if (err)
		pr_err("FAILED!!\n");

	pr_info("reading params successful!\n");

	return !err;
}



int write_params_block(struct csv_info_t *csv, struct gauge_info_t *gauge)
{
	unsigned int i;
	struct param_t *params = csv->params;
	uint8_t subclass = 0, blk_ind = 0, offset;
	uint16_t blk_id, blk_id_prev = 0;
	uint8_t buf[65], tmp;
	union val_t tmp_val;
	uint8_t host_endianness = native_endianness();
	int read_twoblocks = 0, err = 0;

	pr_info("writing params..\n");

	for (i = 0; !err && i < csv->num_params; i++) {
		if (!check_limits(&params[i]))
			goto end;

		subclass = (uint8_t) ((params[i].offset & 0xFF000000) >> 24);
		offset = (uint8_t) (params[i].offset & 0xFF);

		/* block number within subclass */
		blk_ind = offset >> 5;

		/* offset within the block */
		offset = offset & 0x1F;
		if (offset + params[i].data_len > 32) {
			/*
			 * Parameter overflowing to the next block.
			 * Read that block too
			 */
			read_twoblocks = 1;
			tmp = buf[32];
			err |= !read_dm_block(gauge, subclass, blk_ind + 1, &buf[32]);
			buf[32] = tmp;
		}

		/*
		 * unique id for the block:
		 * combination of subclass and blk_ind. Useful to uniquely
		 * identify the blocks and figure out when it's time to read
		 * the next block
		 */
		blk_id = (subclass << 8) | blk_ind;

		if (i == 0 || blk_id != blk_id_prev) {

			/*
			 * if this is the first parameter the buf is not valid yet
			 * in all other cases it's valid
			 */
			if (i != 0) {
				err |= !write_dm_block(gauge, (uint8_t) ((blk_id_prev & 0xFF00) >> 8),
					(uint8_t) (blk_id_prev & 0xFF), buf);
			}

			if (!err)
				err |= !read_dm_block(gauge, subclass, blk_ind, buf);
			if (err)
				goto end;
		 }

		/*
		 * note "offset + 1" below. buf[0] contains the
		 * reg address: BLOCK_DATA
		 * ieee_float_to_x returns the output in x format,
		 * so no Endian conversion required after that.
		 */
		tmp_val = params[i].val;

		if (gauge->ic == GAUGE_IC_X && params[i].raw_type == DATATYPE_F) {
			/* ieee_float_to_x assumes the input float to be in Little Endian */
			if (host_endianness == BYTEORDER_BE)
				reverse_byteorder((uint8_t*) &tmp_val, params[i].data_len);
			ieee_float_to_x(&buf[offset + 1], tmp_val.f);

		} else if (params[i].raw_type == DATATYPE_S) {
			memcpy(&buf[offset + 1], params[i].val_s, params[i].data_len);
		} else if (gauge->endianness != host_endianness) {
			copy_reverse_byteorder(&buf[offset + 1], (uint8_t*) &params[i].val, params[i].data_len);
		} else {
			memcpy(&buf[offset + 1], &params[i].val, params[i].data_len);
		}

		blk_id_prev = blk_id;
		if (read_twoblocks) {
			/*
			 * now write the first block and use the overflow buffer
			 * as the regular buffer
			 */
			err |= !write_dm_block(gauge, (uint8_t) ((blk_id_prev & 0xFF00) >> 8),
				(uint8_t) (blk_id_prev & 0xFF), buf);
			memcpy(&buf[1], &buf[33], 32);
			blk_id_prev++;
			read_twoblocks = 0;
		}
	}

	err |= !write_dm_block(gauge, subclass, blk_ind, buf);

end:
	if (err)
		pr_err("FAILED!!\n");

	pr_info("writing params successful!\n");

	return !err;
}

int read_regs(struct csv_info_t *csv, struct gauge_info_t *gauge)
{
	unsigned int i;
	int err = 0;
	struct param_t *params = csv->params;
	uint8_t buf[5];
	uint8_t host_endianness = native_endianness();
	union val_t tmp_val;
	uint8_t ctrl, reg;
	uint16_t ctrl_reg;

	pr_info("reading regs..\n");

	memset(buf, 0, 5);

	for (i = 0; !err && i < csv->num_params; i++) {
		if (params[i].data_len > 4) {
			pr_err(" %s: data length %d greater than max allowed 4\n")
			return 0;
		}

		tmp_val = params[i].val;

		ctrl = (uint8_t) ((params[i].offset & 0x01000000) >> 24);
		if (ctrl) {
			if (params->data_len > 2) {
				pr_err("control command read can not be more"
				" than 2 bytes long. Truncating to 2 bytes..\n");
				params->data_len = 2;
			}
			ctrl_reg = (uint16_t) (params[i].offset & 0xFFFF);
			params[i].val.u = control_read(gauge, ctrl_reg);
		} else {
			reg = (uint8_t) (params[i].offset & 0xFF);
			buf[0] = reg;
			err |= !do_read(gauge, buf, params[i].data_len);

			/* zero it out before reading new value */
			params[i].val.u = 0;
			if (params[i].raw_type == DATATYPE_F) {
				reverse_byteorder((uint8_t*) &buf[1], params[i].data_len);
				params[i].val.f = x_float_to_ieee(&buf[1]);
				if (host_endianness == BYTEORDER_BE)
					reverse_byteorder((uint8_t*) &params[i].val, params[i].data_len);
			} else if (params[i].raw_type == DATATYPE_S) {
				memcpy(params[i].val_s, &buf[1], params[i].data_len);
			} else if (host_endianness == BYTEORDER_BE) {
				/* Fix byte ordering (Endianness) for integral numerical types */
				copy_reverse_byteorder((uint8_t*) &params[i].val, &buf[1], params[i].data_len);
			} else {
				memcpy(&params[i].val, &buf[1], params[i].data_len);
			}
		}

		/* Update the value string if a binary comparison with old value fails */
		if (tmp_val.u != params[i].val.u)
			err |= !update_value_string(&params[i], csv->value_fld_ind);
	}


	if (err)
		pr_err("FAILED!!\n");

	pr_info("reading regs successful!\n");

	return !err;
}

static int seal_after_dm_access(struct gauge_info_t *gauge, int argc, char **argv)
{
	/* By default return to the previous seal state */
	int seal_req = gauge->orig_seal_status;
	int err = 0;
	const char *exit_seal = get_cmdline_argument(argc, argv, "--exit-seal=", 1);

	if (exit_seal) {
		if (strcmp(exit_seal, "seal") == 0)
			seal_req = 1;
		else if (strcmp(exit_seal, "unseal") == 0)
			seal_req = 0;
		else if (strcmp(exit_seal, "original") == 0)
			seal_req = gauge->orig_seal_status;
		else
			pr_err("undetected --exit-seal= request\n");
	}

	if (seal_req) {
		err |= seal(gauge);
	} else {
		err |= unseal(gauge);
		err |= unseal_fullaccess(gauge);
	}

	return err;
}

static uint32_t gauge_fmly_from_user(int argc, char **argv)
{
	uint32_t ret = 0;
	char *tmp;
	const char *family = get_cmdline_argument(argc, argv, "--gauge-family=", 1);

	if (!family)
		return 0;

	if (strstr(family, "bq") == family )
		ret = strtoul(&family[2], &tmp, 16);

	pr_dbg("family from user %s\n", family);

	if (!ret)
		pr_err("bad input for --gauge-family\n");

	return ret;
}

static uint32_t autodetect_gauge_family(uint16_t dev)
{
	uint32_t family = 0;

	switch(dev) {
	case 0x0500:
	case 0x0501:
	case 0x0505:
                family = 0x8032;
                break;
	case 0x0510:
	case 0x0541:
	case 0x0410:
                family = 0x8034;
                break;
	case 0x0520:
	case 0x0545:
	case 0x0530:
	case 0x0531:
	case 0x0532:
	case 0x0620:
                family = 0x8035;
                break;
	case 0x0425:
                family = 0x8036;
                break;
	case 0x0546:
	case 0x0741:
	case 0x0742:
                family = 0x8037;
                break;
	case 0x0421:
	/*case 0x0411:
	case 0x0441:*/
	case 0x0621:
                family = 0x8101;
                break;
	default:
		pr_err("uable to auto-detect gauge family\n");
		break;
	}

	pr_dbg("autodetected family bq%04x\n", family);

	return family;
}

static int get_gauge_family(struct gauge_info_t *gauge, int argc, char **argv)
{
	uint32_t usr_fmly, det_fmly;

	usr_fmly = gauge_fmly_from_user(argc, argv);
	det_fmly = autodetect_gauge_family(gauge->device_num);
	if (det_fmly) {
		gauge->family = det_fmly;
		if (usr_fmly && (usr_fmly != det_fmly)) {
			pr_err("User provided gauge-family does not"
			"match with autodetected family. Using autodetected"
			"family usr 0x%0x auto 0x%0x\n",
			usr_fmly, det_fmly);
		}
	} else if (usr_fmly) {
		gauge->family = usr_fmly;
	} else if (gauge->op == OP_BQFS_FLASH){
		gauge->family = 0x8035;
		pr_err("Unable to get gauge family for bqfs flashing. Assuming 0x8035..\n");
	} else {
		pr_err("unable to detect gauge family. Aborting..\n");
		return 0;
	}

	return 1;
}

static void reset_gauge(struct gauge_info_t *gauge)
{
	pr_info("resetting gauge..\n");
	control_write(gauge, CTRL_CMD_RESET);
}


static int open_dm_flash(struct gauge_info_t *gauge, int argc, char **argv)
{
	long long int res;
	int err_ss = 0, err_fas = 0, keys_in_gg = 0;
	int ss, fas;

	pr_dbg(">>\n");

	ss = sealed(gauge);
	fas = fullaccess_sealed(gauge);
	gauge->orig_seal_status = (uint8_t) (ss || fas);
	if (!ss && !fas)
		return 1;

	const char *unseal_key_cmdline =
		get_cmdline_argument(argc, argv, "--unseal-key=", 1);
	const char *fullaccess_key_cmdline =
		get_cmdline_argument(argc, argv, "--fullaccess-key=", 1);

	keys_in_gg = gauge->unseal_key_found || gauge->fullaccess_key_found;

	if (unseal_key_cmdline && extract_int(unseal_key_cmdline, 16, &res)) {
		gauge->unseal_key = (uint32_t) res;
		gauge->unseal_key_found = 1;
		pr_dbg("Unseal key from command line 0x%08x\n",
			gauge->unseal_key);
	}

	if (fullaccess_key_cmdline &&  extract_int(fullaccess_key_cmdline, 16, &res)) {
		gauge->fullacccess_key = (uint32_t) res;
		gauge->fullaccess_key_found = 1;
		pr_dbg("Full access key from command line 0x%08x\n",
			gauge->fullacccess_key);
	}

	if (ss && gauge->unseal_key_found) {
		err_ss = !unseal(gauge);
	} else if (ss && !gauge->unseal_key_found && gauge->op != OP_BQFS_FLASH) {
		pr_err("unseal key not provided\n");
		err_ss = 1;
	}

	if (fas && gauge->fullaccess_key_found) {
		/* Ignore the error if the keys are not in gg */
		err_fas = keys_in_gg && !unseal_fullaccess(gauge);
	} else if (fas && keys_in_gg && !gauge->fullaccess_key_found) {
		pr_err("Full access key not provided even though read/write of keys is requested\n");
		err_fas = 1;
	}

	pr_dbg("<<\n");

	return !(err_ss || err_fas);
}

static void close_dm_flash(struct gauge_info_t *gauge, int argc, char **argv)
{
	seal_after_dm_access(gauge, argc, argv);
}

#define CFG_UPDATE_POLLING_RETRY_LIMIT_MS	5000

static void reset_gauge_rom(struct gauge_info_t *gauge)
{
	pr_err("Doing reset of ROM gauge is not a good idea!! Ignoring..\n");
}

static int enter_cfgupdate_mode(struct gauge_info_t *gauge)
{
	int i = 0;
	uint16_t flags;

	control_write(gauge, CTRL_CMD_ROM_GAUGE_SET_CFGUPDATE);

	while (i < CFG_UPDATE_POLLING_RETRY_LIMIT_MS / 100) {
		i++;
		flags = read_word(gauge, ROM_GAUGE_FLAGS);
		if (flags & ROM_GAUGE_FLAG_CFGUPDATE_MODE)
			break;
		gauge->sleep_ms(100);
	}

	if (i == CFG_UPDATE_POLLING_RETRY_LIMIT_MS / 100) {
		pr_err("failed flags %04x\n", flags);
		return 0;
	}

	return 1;
}

static int exit_cfgupdate_mode(struct gauge_info_t *gauge)
{
	int i = 0;
	uint16_t flags;

	control_write(gauge, CTRL_CMD_ROM_GAUGE_EXIT_CFGUPDATE);

	while (i < CFG_UPDATE_POLLING_RETRY_LIMIT_MS / 100) {
		i++;
		flags = read_word(gauge, ROM_GAUGE_FLAGS);
		if (!(flags & ROM_GAUGE_FLAG_CFGUPDATE_MODE))
			break;
		gauge->sleep_ms(100);
	}

	if (i == CFG_UPDATE_POLLING_RETRY_LIMIT_MS / 100) {
		pr_err("failed %04x\n", flags);
		return 0;
	}

	return 1;
}

static int rom_itpor(struct gauge_info_t *gauge)
{
	return read_word(gauge, ROM_GAUGE_FLAGS) & ROM_GAUGE_FLAG_ITPOR;
}

static int open_dm_rom(struct gauge_info_t *gauge, int argc, char **argv)
{
	int ret = 1;

	gauge->unseal_key = ROM_GAUGE_UNSEAL_KEY;
	gauge->unseal_key_found = 1;
	gauge->fullaccess_key_found = 1;
	gauge->orig_seal_status = (uint8_t) sealed(gauge);

	if (gauge->orig_seal_status)
		ret = unseal(gauge);

	if (ret && gauge->op == OP_DM_WRITE) {
		return enter_cfgupdate_mode(gauge);
	}

	return ret;
}

static void close_dm_rom(struct gauge_info_t *gauge, int argc, char **argv)
{
	if (gauge->op == OP_DM_WRITE)
		exit_cfgupdate_mode(gauge);

	seal_after_dm_access(gauge, argc, argv);
}

static int gauge_family_specific_init(struct gauge_info_t *gauge)
{
	switch(gauge->family) {
#if (defined(BQ8032) || defined(BQ8034) || defined(BQ8035) || defined(BQ8036) || defined(BQ8037))
	case 0x8032:
	case 0x8034:
	case 0x8035:
	case 0x8036:
	case 0x8037:
		gauge->ic = GAUGE_IC_X;
		gauge->endianness = BYTEORDER_BE;
		gauge->open_dm = open_dm_flash;
		gauge->close_dm = close_dm_flash;
		gauge->write_params = write_params_block;
		gauge->read_params = read_params_block;
		gauge->reset = reset_gauge;
		break;
#endif
#if defined(BQ8101)
	case 0x8101:
		gauge->ic = GAUGE_IC_X;
		gauge->endianness = BYTEORDER_BE;
		gauge->open_dm = open_dm_rom;
		gauge->close_dm = close_dm_rom;
		gauge->write_params = write_params_block;
		gauge->read_params = read_params_block;
		gauge->reset = reset_gauge_rom;
		gauge->itpor = rom_itpor;
		gauge->fw_ver_bld = (uint8_t)
			control_read(gauge, CTRL_CMD_ROM_GAUGE_DM_CODE);
		break;
#endif
	default:
		pr_err("unsupported gauge family 0x%0x\n", gauge->family);
		return 0;
	}

	return 1;
}

#ifndef GAUGE_SIMULATOR
int init_gauge_interface(struct gauge_info_t *gauge, int argc, char **argv)
{
	if (!setup_comm_callbacks(gauge))
		return 0;

	if (!gauge->init_comm_interface(gauge, argc, argv))
		return 0;

	/* Get device type and FW version */
	gauge->device_num = device_type(gauge);
	gauge->fw_version = fw_version(gauge);

	if (gauge->op == OP_REG_DUMP)
		return 1;

	if (!get_gauge_family(gauge, argc, argv))
		return 0;

	return gauge_family_specific_init(gauge);
}
#endif

