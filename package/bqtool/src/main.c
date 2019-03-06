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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <ctype.h>

#include "bqt.h"

#define ARRAY_SIZE(arr) (sizeof((arr))/sizeof(arr[0]))
/*
 * Sample CSV Header:
* Texas Instruments Data Flash File
* File created Fri Oct 02 17:52:22 2015
*
* Device Number 421
* Firmware Version 1.08.10
* Build Number not available
* Order Number not available
*
* bqz Device Number 0x0421
* bqz Firmware Version 0x0108
* bqz Build Number 1.08
*
* Field Order: Class name, SubClass name, Parameter name, Parameter Value, Data Type, Data Length, Address Offset, Default Value, Minimum Value, Maximum Value, Display Format, Read Formula, Write Formula, Flags, Display Units, Help, Native Units, Data Flash Raw Value
 */

const char *fld_ordr_marker = "* Field Order:";
const char *dev_num_marker = "* Device Number ";
const char *fw_ver_marker = "* Firmware Version ";
const char *disp_unit_fld_name = "Display Units";

struct command_t {
	const char *cmd_name;
	int (*cmd_func)(int argc, char **argv);
	const char *help;
};

struct export_format_t {
	char **fields;
	int	num_fields;
	uint8_t	value_fld_ind;
	uint8_t *fmt_csv_to_params_csv_fld_map;
};


struct export_info_t {
	FILE		*fd;
	uint32_t	period;
	uint8_t		no_hdr;
	uint8_t		timestamp;
};

enum cmd_type_t {
	CMD_INVALID = 0,
	CMD_R,	/* Read */
	CMD_W,	/* Write */
	CMD_C,	/* Compare */
	CMD_X,	/* Delay */
};

enum bqfs_mode_t {
	BQFS_HDQ = 1,
	BQFS_I2C
};

struct bqfs_cmd_t {
	char cmd;
	uint8_t mode;
	uint8_t addr;
	uint8_t len;
	uint16_t delay;
	/* Maximum length including slave address, register, and data */
	uint8_t max_len;
	uint32_t line_num;
	uint8_t *buf;
	uint8_t *tmp_buf;
};

void print(FILE *out, const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vfprintf(out, fmt, ap);
        va_end(ap);
}

/*
 * Field names for fields used in this program.
 * They must be strictly int order of the fields in
 * enum field_t
 */
static const char *used_fields[NUM_USED_FIELDS] = {
	"Parameter name",	/* USED_FLD_NAME */
	"Parameter Value",	/* USED_FLD_VALUE */
	"Minimum Value",	/* USED_FLD_MIN_VAL */
	"Maximum Value",	/* USED_FLD_MAX_VAL */
	"Data Type",		/* USED_FLD_DATATYPE */
	"Data Length",		/* USED_FLD_DATA_LENGTH */
	"Address Offset",	/* USED_FLD_ADDRESS_OFFST */
	"Display Format",	/* USED_FLD_DISPLAY_FORMAT */
	"Read Formula",		/* USED_FLD_READ_FORMULA */
	"Write Formula",	/* USED_FLD_WRITE_FORMULA */
};


/*
 * If str2 is part of str1 return the remaining
 * part of str1 after str2.
 * E.g.:
 * str1 = "* Device Number 0x0545"
 * str2 = "Device Number "
 * strstr_end output: "0x0545"
 */
static char *strstr_end(char *str1, const char *str2)
{
	char *res = strstr(str1, str2);

	if (res)
		res += strlen(str2);

	return res;
}

static char *skip_leading_whitespaces(char *str)
{
	char *c = str;

	while((*c == ' ' || *c == '\t') && (*c != '\0'))
		c++;

	return c;
}

char *read_line(struct file_info_t *file)
{
	int c;
	int i = 0;
	FILE *fd = file->fd;
	char *line_buf = file->line_buf;

	file->line_num++;

	while (1) {
		c = fgetc(fd);

		if (feof(fd)) {
			pr_dbg("EOF\n");
			break;
		} else if (ferror(fd)) {
			i = 0;
			break;
		}

		if (((c == '\r') || (c == '\n') || (c == '\t')
			|| (c == ' ')) && (i == 0)) {
			/*
			 * Skip leading white space, if any, at the beginning
			 * of the line because this interferes with strtok
			 */
			pr_dbg("Leading whitespace at line %d\n",
				file->line_num);
			if (c == '\n')
				file->line_num++;
			continue;	/* blank line, let's continue */
		} else if (c == '\r' || c== '\n') {
			/* We've reached end of line */
			break;
		}

		line_buf[i++] = c;

		if (i == file->max_line_len) {
			/*
			 * Re-allocate in case the line is longer than
			 * expected
			 */
			line_buf = (char *)realloc(line_buf, file->max_line_len * 2);
			pr_err("Line %d longer than expected,"
				" reallocating..\n", file->line_num);
		} else if (i == file->max_line_len * 2) {
			/*
			 * The line is already twice the expected maximum length
			 * - maybe the bqfs/dffs needs to be fixed
			 */
			pr_err("Line %d too long, abort parsing..\n",
				file->line_num);
			i = 0;
			break;
		}
	}

	line_buf[i++] = '\0';

	if (i < 2)
		return NULL;

	return line_buf;
}

/*
 * Get tokens from a csv. Comma at the end results in an
 * empty string as the last token. Successive commas also
 * result in empty string as token.
 */
static char *get_delim_separated_item(char **str, char delim)
{
	char *start = *str;
	char *c = *str;

	if (!start)
		return NULL;

	c = start = skip_leading_whitespaces(start);

	while (*c != delim && *c != '\0')
		c++;

	if (*c == delim) {
		*str = c + 1;
		/* ',' found now, strip trailing whitespace if any */
		while(*(--c) == ' ' || *c == '\t' || *c == '\n')
			pr_err("whitespace found in csv field: %s\n", start);
		/* Null terminate */
		*(++c) = '\0';
	} else {
		*str = NULL;
	}

	return start;
}

static uint8_t extract_csv_fields(char *line, char ***fieldsp,
	uint8_t max_fields)
{
	const char *field;
	int tkn_num = 0, tkn_len;
	char **fields = *fieldsp;

	fields = (char **) malloc(sizeof(char*) * max_fields);
	memset(fields, 0, sizeof(char*) * max_fields);

	while ((field = get_delim_separated_item(&line, ',')) && (tkn_num < max_fields)) {

		tkn_len = strlen(field);
		fields[tkn_num] = (char *) malloc(tkn_len + 1);
		/* copy the string from line to newly allocated string */
		memcpy(fields[tkn_num], field, tkn_len + 1);
		tkn_num++;
	}

	/*
	 * Now that we know the number of fields, allocate a new array for
	 * fields with just the right number of entries
	 */
	if (tkn_num && tkn_num < max_fields) {
		fields = realloc(fields, sizeof(char*) * tkn_num);
	} else if (!tkn_num) {
		free(fields);
		fields = NULL;
	}

	*fieldsp = fields;

	return tkn_num;
}

static int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a || !*b)
            return d;
    }
}

static int get_field_map(uint8_t *A_to_B_map, char **fld_arr_A, int len_A, char **fld_arr_B, int len_B)
{
	int i, j;

	for (i = 0; i < len_A; i++) {
		/*
		 * Some of the shorter versions of the gg.csv files
		 * out there have "Units" instead of "Display Units"
		 * Be forgiving with such files
		 */
		if (strcicmp(fld_arr_A[i], "Units") == 0) {
			fld_arr_A[i] = (char *) realloc(fld_arr_A[i],
				strlen(disp_unit_fld_name) + 1);
			if (!fld_arr_A[i])
				return 0;
			strcpy(fld_arr_A[i], disp_unit_fld_name);
		}

		for (j = 0; j < len_B; j++) {
			if (strcicmp(fld_arr_A[i], fld_arr_B[j]) == 0) {
				A_to_B_map[i] = j;
				pr_dbg("field \"%s\" at index %d\n", fld_arr_A[i], j);
				break;
			}
		}

		if (j == len_B) {
			pr_err("couldn't find field :%s:\n", fld_arr_A[i]);
			return 0;
		}
	}

	return 1;
}

static int parse_header(struct csv_header_t *header)
{
	bool found_fld_ordr = 0;
	bool found_device = 0;
	bool found_fw_ver = 0;
	char *line = NULL;
	char *token = NULL, *tmp;
	uint32_t fw_version = 0;
	int i = 0;

	header->num_fields = 0;

	/*
	 * On finding the field order marker, don't go any further looking
	 * for header. Other than that no assumptions about the location
	 * of specific header items.
	 */
	while (!found_fld_ordr && (line = read_line(header->file))) {
		if ((token = strstr_end(line, fld_ordr_marker)))
		{
			found_fld_ordr = 1;

			header->num_fields = extract_csv_fields(token, &header->field_names,
				MAX_CSV_FIELDS);
		} else if ((token = strstr_end(line, dev_num_marker))) {
			found_device = 1;
			header->device_num = (uint16_t)strtoul(token, NULL, 16);
			pr_dbg("device_num = %d\n", header->device_num);
		} else if ((token = strstr_end(line, fw_ver_marker))) {
			i = 0;
			while ((tmp = get_delim_separated_item(&token, '.'))) {
				fw_version <<= 8;
				fw_version |= (uint8_t) strtoul(tmp, NULL, 16);
				i++;
			}

			found_fw_ver = 1;
			if (i == 2) {
				header->fw_version = (uint16_t) fw_version;
			} else if (i == 3) {
				header->fw_version = (uint16_t) (fw_version >> 8);
				header->fw_ver_bld = (uint16_t) (fw_version & 0xFF);
			} else {
				found_fw_ver = 0;
				continue;
			}
		}
	}

	pr_dbg("device = 0x%04x fw = 0x%04x num_fields = %d\n",
		header->device_num,
		header->fw_version, header->num_fields);

	if (!found_device || !found_fw_ver) {
		pr_err("could not find dev info from hdr dev=0x%04x fw=0x%04x\n",
			header->device_num, header->fw_version);
	}

	if (!header->num_fields){
		pr_err("could not find field info from header\n");
		return 0;
	}

	return 1;
}

static int discard_unused_flds(char ***fields, int num_used_flds, uint8_t *used_flds_map)
{
	char **tmp = (char **) malloc(sizeof(char *) * num_used_flds);
	int i;

	pr_dbg(">> %x %d %x\n", fields, num_used_flds, used_flds_map);

	if (!tmp) {
		pr_err("failed\n");
		return 0;
	}

	for (i = 0; i < num_used_flds; i++)
		tmp[i] = (*fields)[used_flds_map[i]];

	free(*fields);
	*fields = tmp;

	pr_dbg("<<\n");

	return 1;
}

/*
 * Use hard-coded format strings
 * We don't want to allocate one format string per parameter,
 * so just point to string literals
 */
const char *get_float_fmt_str(int fract_len)
{
	const char *fmt;

	switch (fract_len) {
	case 1:
		fmt = "%.1f";
		break;
	case 2:
		fmt = "%.2f";
		break;
	case 3:
		fmt = "%.3f";
		break;
	case 4:
	default:
		fmt = "%.4f";
		break;
	}

	return fmt;
}

/*
 * Use hard-coded format strings
 * We don't want to allocate one format string per parameter,
 * so just point to string literals
 */
const char *get_hex_fmt_str(int len)
{
	const char *fmt;

	switch (len) {
	case 2:
		fmt = "%02x";
		break;
	case 4:
		fmt = "%04x";
		break;
	case 8:
	default:
		fmt = "%08x";
		break;
	}

	return fmt;
}

/*
 * Display format affects how the parameter is read from
 * and written to the csv. For instance an integer data
 * type with display format dd.dd will be read from the csv
 * as a float and multiplied by 100 before applying the
 * write formula and writing to DM (divide by 100 in the
 * opposite direction).
 */
static int get_display_format(const char *str, struct param_t *param)
{
	int fract_len = 0, len = 0, num_dec_pts = 0;
	const char *dec_point = NULL;
	const char *c = str;
	char type_char, ch;

	/* Move until we find the type character */
	while (*c++ == '.');

	type_char = *(--c);
	type_char = tolower(type_char);

	/* String is a special case, no need to look for decimal points etc */
	if (type_char == 's') {
		param->disp_type = DISP_FMT_S;
		param->csv_fraction_len = 0;
		param->fmt_str = "%s";
		return 1;
	}


	c = str;

	while ((ch = *c)) {
		if (ch == '.') {
			num_dec_pts++;
			dec_point = c;
		} else if (ch != type_char) {
			pr_err("multiple type characters in display"
				" format: %s", str);
			return 0;
		}
		c++;
		len++;
	}

	if (!len || num_dec_pts > 1) {
		pr_err("wrong display format \"%s\": len=%d num_dec_pts=%d\n",
			str, len, num_dec_pts);
		return 0;
	}

	if (num_dec_pts == 1)
		fract_len = len - (dec_point - str) - 1;
	else
		fract_len = 0;

	switch (type_char) {
	case 'd':
		param->csv_fraction_len = fract_len;
		if (!fract_len) {
			param->disp_type = DISP_FMT_I;
			param->fmt_str = "%lld";
		} else {
			/*
			 * Although the DM value is integer it's
			 * converted to float for displaying in csv
			 */
			param->disp_type = DISP_FMT_F;
			param->fmt_str = get_float_fmt_str(fract_len);
		}
		break;
	case 'h':
		param->disp_type = DISP_FMT_H;
		if (fract_len) {
			pr_err("fractional part not supported for hex display format\n");
			return 0;
		}
		param->csv_fraction_len = 0;
		param->fmt_str = get_hex_fmt_str(len);
		break;
	case 'f':
		param->disp_type = DISP_FMT_F;
		param->csv_fraction_len = 0;
		param->fmt_str = get_float_fmt_str(fract_len);
		break;
	case 's':
		/* Already taken care of, we shouldn't be here, so fall through */
	default:
		pr_err("Unsupported display format \"%s\"\n", str);
		return 0;

	}


	return 1;
}

int extract_int(const char *str, int base, long long int *res)
{
	char *temp;

	*res = strtoll(str, &temp, base);

	if (str == temp) {
		pr_err("extracting integer failed: %s\n", str);
		return 0;
	}

	return 1;
}

static int extract_integer(const char *str, long long int *res)
{
	if ((strstr(str, "0x") == str) || (strstr(str, "0X") == str))
		return extract_int(str, 16, res);
	else
		return extract_int(str, 10, res);
}

static int extract_double(const char *str, double *res)
{
	char *temp;

	*res = strtod(str, &temp);

	if (str == temp) {
		pr_err("Extracting double failed: %s\n", str);
		return 0;
	}

	return 1;
}

/*
 * Get the scale factor:
 * Scale factor is the inverse of division factor
 * and division factor is saved as log10 value.
 */
static int get_scale_factor(int div_factor_log)
{
	int scale_factor = 1, i;
	for (i = 0; i < div_factor_log; i++)
		scale_factor *= 10;

	return scale_factor;
}

static long long int sign_extend(int32_t val, int len)
{
	long long int ll;

	switch (len) {
	case 1:
		ll = (int8_t) (val & 0xFF);
		break;
	case 2:
		ll = (int16_t) (val & 0xFFFF);
		break;
	case 4:
	default:
		ll = (int32_t) (val & 0xFFFFFFFF);
		break;
	}

	return ll;
}

int check_limits(struct param_t *param)
{
	int8_t ovflow = 0, uflow = 0;
	long long int val, min, max;

	switch (param->raw_type) {
	case DATATYPE_I:
		val = sign_extend(param->val.i, param->data_len);
		min = sign_extend(param->min.i, param->data_len);
		max = sign_extend(param->max.i, param->data_len);
		ovflow = val > max;
		uflow = val < min;
		break;
	case DATATYPE_U:
		ovflow = param->val.u > param->max.u;
		uflow = param->val.u < param->min.u;
		break;
	case DATATYPE_F:
		ovflow = param->val.f > param->max.f;
		uflow = param->val.f < param->min.f;
		break;
	case DATATYPE_S:
		/* ignore limits */
		break;
	default:
		return 0;
	}

	if (ovflow) {
		pr_err("%s: parameter value exceeds max value"
			" 0x%04x 0x%04x\n", param->name, param->val.u,
			param->max.u);
	}

	if (uflow) {
		pr_err("%s: parameter value less than min value"
			" 0x%04x 0x%0x4x\n", param->name, param->val.u,
			param->min.u);
	}

	return !(ovflow || uflow);
}

int update_value_string(struct param_t *param, int value_fld_ind)
{
	unsigned int scale_factor, len;
	long long int ll;
	char *str;
	char new_str[MAX_USED_FLD_LEN];
	double d;


	if (param->raw_type == DATATYPE_S) {
		snprintf(new_str, param->data_len - 1, "%s", &param->val_s[1]);
		goto update_str;
	}

	scale_factor = get_scale_factor(param->csv_fraction_len);
	switch (param->raw_type) {
	case DATATYPE_I:
		ll = sign_extend(param->val.i, param->data_len);
		d = ll;
		break;
	case DATATYPE_U:
		/*
		 * Assumption is that long long is at least 64
		 * bits long. This seems to be guaranteed by C99
		 * https://en.wikipedia.org/wiki/C_data_types#Size
		 */
		ll = (long long int) param->val.u;
		d = ll;
		pr_dbg("u %u ll %ld d %lg", param->val.u, ll, d);
		break;
	case DATATYPE_F:
	default:
		d = param->val.f;
		break;
	}

	if (param->read_formula_expr) {
		d = evaluate_expr(param->read_formula_expr, d);
		ll = (long long int) d;
	}

	if (scale_factor != 1)
		d /= scale_factor;

	if (param->disp_type == DISP_FMT_F)
		sprintf(new_str, param->fmt_str, (float) d);
	else
		sprintf(new_str, param->fmt_str, ll);

update_str:
	str = param->fields[value_fld_ind];
	len = strlen(new_str);
	if (len > strlen(str)) {
		str = (char *) realloc(str, len + 1);
		if (!str)
			return 0;
	}
	memcpy(str, new_str, len + 1);
	param->fields[value_fld_ind] = str;

	pr_dbg("%s %s\n", str, new_str);

	return 1;
}
/*
 * The "x" in read/write formula is the raw DF value.
 * So, the display format(like dd.dd) related scaling
 * should be applied before applying the formula in the
 * write direction and the scaling should be applied after
 * the formula in read direction.
 *
 * The min, max limit check is applicable only in write
 * direction and applies after all transformations. So, they
 * are in the same unit as the raw DF value.
 */

static int extract_param_data(struct param_t *param, uint8_t *used_fld_ind)
{
	int err, scale_factor, base;
	unsigned long int ul;
	long long int llmax, llmin, llval;
	double d;
	char *str, *tmp, *val, *min, *max, *name;
	unsigned int len;

	/*
	 * Parameter name
	 */
	name = param->fields[used_fld_ind[USED_FLD_NAME]];

	/*
	 * Address Offset
	 */
	str = param->fields[used_fld_ind[USED_FLD_ADDRESS_OFFST]];
	ul = strtoul(str, &tmp, 16);
	if (str == tmp) {
		pr_err("%s: couldn't extract param offset: %s\n", name, str)
		return 0;
	}
	/* The MSB byte is subclass and the LSB byte is offset */
	param->offset = (uint32_t) ul & 0xFFFFFFFF;

	/*
	 * Data Length
	 */
	str = param->fields[used_fld_ind[USED_FLD_DATA_LENGTH]];
	err = extract_int(str, 10, &llval);
	if (!err) {
		pr_err("%s: error parsing datalength \"%s\"\n", name, str);
		return 0;
	}
	param->data_len = (uint8_t) llval;

	/*
	 * Read Formula, Write Formula
	 */
	str = param->fields[used_fld_ind[USED_FLD_READ_FORMULA]];
	/* No need to create Reverse Polish Notation expression if formula is "x" */
	if (strcmp(str, "x") != 0 && strcmp(str, "X") != 0) {
		//printf("%s\n", str);
		param->read_formula_expr = parse_expression(str);
		if (!param->read_formula_expr) {
			pr_err("%s: Error parsing read formula \"%s\"\n", name, str);
			return 0;
		}

	}
	str = param->fields[used_fld_ind[USED_FLD_WRITE_FORMULA]];
	/* No need to create Reverse Polish Notation expression if formula is "x" */
	if (strcmp(str, "x") != 0 && strcmp(str, "X") != 0) {
		param->write_formula_expr = parse_expression(str);
		if (!param->write_formula_expr) {
			pr_err("%s: Error parsing write formula \"%s\"\n", name, str);
			return 0;
		}
	}

	/*
	 * Display Format
	 */
	str = param->fields[used_fld_ind[USED_FLD_DISPLAY_FORMAT]];
	if (!get_display_format(str, param))
		return 0;

	/*
	 * Data Type
	 */
	str = param->fields[used_fld_ind[USED_FLD_DATATYPE]];
	switch (toupper(*str)) { /* Assume datatype to be single char */
	case 'I':
		param->raw_type = DATATYPE_I;
		break;
	case 'U':
	case 'B':
		param->raw_type = DATATYPE_U;
		break;
	case 'F':
		param->raw_type = DATATYPE_F;
		break;
	case 'S':
		param->raw_type = DATATYPE_S;
		if (param->disp_type != DISP_FMT_S)
			return 0;
		break;
	default:
		pr_err("%s: unknown data type %s", name, str);
		return 0;
	}

	/*
	 * Min and Max Values
	 */
	min = param->fields[used_fld_ind[USED_FLD_MIN_VAL]];
	max = param->fields[used_fld_ind[USED_FLD_MAX_VAL]];
	if (param->disp_type == DISP_FMT_H)
		base = 16;
	else
		base = 10;

	err = 0;
	switch(param->raw_type) {
	case DATATYPE_I:
	case DATATYPE_U:
		err |= !extract_int(min, base, &llmin);
		err |= !extract_int(max, base, &llmax);
		if (param->raw_type == DATATYPE_I) {
			param->min.i = (int32_t) (llmin & 0xFFFFFFFF);
			param->max.i = (int32_t) (llmax & 0xFFFFFFFF);

		} else {
			param->min.u = (uint32_t) (llmin & 0xFFFFFFFF);
			param->max.u = (uint32_t) (llmax & 0xFFFFFFFF);
		}
		break;
	case DATATYPE_F:
		err |= !extract_double(min, &d);
		param->max.f = (float) d;
		err |= !extract_double(max, &d);
		param->max.f = (float) d;
		break;
	case DATATYPE_S:
		/* In case of strings just ignore min and max */
		break;
	default:
		return 0;
	}

	val = param->fields[used_fld_ind[USED_FLD_VALUE]];
	switch(param->disp_type) {
	case DISP_FMT_I:
	case DISP_FMT_H:
		err |= !extract_int(val, base, &llval);
		if (param->raw_type == DATATYPE_I) {
			param->val.i = (int32_t) (llval & 0xFFFFFFFF);
			d = param->val.i;
		} else {
			param->val.u = (uint32_t) (llval & 0xFFFFFFFF);
			d = param->val.u;
		}
		break;
	case DISP_FMT_F:
		/*
		 * Parameters with display formats like dd.d also
		 * will end up here
		 */
		err |= !extract_double(val, &d);
		break;
	case DISP_FMT_S:
		if (param->data_len > 32) {
			pr_err("%s: bqtool doesn't support a string longer than 31"
				" bytes long. Consider fixing bqtool!!\n", name);
			return 0;
		}
		/*
		 * Please note that per the spec, the first byte in the buffer is the length,
		 * rest the string itself. The string need not be null terminated, but let's
		 * null terminate it for use within the tool
		 */
		param->val_s = (char *) malloc(param->data_len + 1);
		if (!param->val_s)
			return 0;
		memset(param->val_s, 0, param->data_len + 1);
		len = strlen(val);
		if (len > (unsigned) (param->data_len - 1))
			len = param->data_len - 1;
		strncpy(&param->val_s[1], val, len);
		param->val_s[0] = (uint8_t) len;
		param->val_s[len + 1] = '\0';
		break;
	default:
		return 0;
	}

	if (err) {
		pr_err("%s: error extracting Parameter Value\n", name);
		return 0;
	}

	scale_factor = get_scale_factor(param->csv_fraction_len);
	if (scale_factor != 1)
		d *= scale_factor;
	if (param->write_formula_expr)
		d = evaluate_expr(param->write_formula_expr, d);

	switch(param->raw_type) {
	case DATATYPE_I:
		param->val.i = (int32_t) d;
		break;
	case DATATYPE_U:
		param->val.u = (uint32_t) d;
		break;
	case DATATYPE_F:
		param->val.f = (float) d;
		break;
	}

	return 1;
}

static int copy_string(char **dest, const char *src)
{
	unsigned int len = strlen(src);

	*dest = (char *) malloc(len + 1);
	if (!(*dest))
		return 0;

	strcpy(*dest, src);

	return 1;
}

static int parse_params(struct csv_info_t *csv, struct export_format_t *exp)
{
	int num_fields, i = 0, err = 0;
	char *line;
	struct file_info_t *file = csv->header->file;
	const char *name;

	pr_dbg(">>\n");

	struct param_t *params = (struct param_t *) malloc(sizeof(struct param_t) * MAX_NUM_PARAMS);
	if (!params)
		return 0;

	memset(params, 0, sizeof(struct param_t) * MAX_NUM_PARAMS);
	csv->params = params;

	while ((line = read_line(file)) && (i < MAX_NUM_PARAMS) && !err) {
		num_fields = extract_csv_fields(line, &params[i].fields, csv->header->num_fields);
		if (num_fields != csv->header->num_fields) {
			pr_err("parameter parsing failed 1 at \"%s\":%d num_fields = %d\n",
				file->fname, file->line_num, num_fields);
			return 0;
		}

		if (!extract_param_data(&params[i], csv->used_field_index))
			return 0;

		name = params[i].fields[csv->used_field_index[USED_FLD_NAME]];
		if (!copy_string(&params[i].name, name))
			return 0;
		/*
		 * Now that parsing the parameter is over, remove fields
		 * that are not needed for export if user has provided
		 * export format
		 */
		if (exp) {
			if (!discard_unused_flds(&params[i].fields, exp->num_fields,
					exp->fmt_csv_to_params_csv_fld_map))
				return 0;
		}

		i++;
	}

	if (i == 0) {
		pr_err("No parameters in csv\n");
		return 0;
	}
	csv->num_params = i;

	if (line) {
		pr_err("parameter parsing failed - more parameters than expected\"%s: %d\n",
				file->line_num);
		return 0;
	}

	/* reallocate the array to save space */
	if (csv->num_params < MAX_NUM_PARAMS)
		csv->params = realloc(params, sizeof(struct param_t) * csv->num_params);

	if (!csv->params)
		return 0;

	pr_dbg("<<\n");

	return 1;
}

struct gauge_info_t *create_gauge(void)
{
	struct gauge_info_t *gauge =
		(struct gauge_info_t *) malloc(sizeof(struct gauge_info_t));

	if (!gauge)
		return NULL;

	memset(gauge, 0, sizeof(struct gauge_info_t));


	return gauge;
}

static void free_gauge(struct gauge_info_t *gauge)
{
	if (!gauge)
		return;

#if 0
	if (gauge->unlock_gauge_interface)
		gauge->unlock_gauge_interface(gauge);
#endif

	if (gauge->close_comm_interface)
		gauge->close_comm_interface(gauge);

	if (gauge->interface)
		free(gauge->interface);

	free(gauge);
}

static void free_fields(char **fields, int num_fields)
{
	int i;

	if (!fields)
		return;

	for (i = 0; i < num_fields; i++) {
		if (fields[i])
			free(fields[i]);
	}

	free(fields);
}

static void free_param(struct param_t *param, int num_fields)
{
	if (!param)
		return;

	if (param->fields)
		free_fields(param->fields, num_fields);

	if (param->read_formula_expr)
		delete_queue(param->read_formula_expr);

	if (param->write_formula_expr)
		delete_queue(param->write_formula_expr);

	if (param->raw_type == DATATYPE_S && param->val_s)
		free(param->val_s);

	if (param->name)
		free(param->name);
}

static void free_params(struct param_t *params, int num_params, int num_fields)
{
	int i;

	if (!params)
		return;

	for (i = 0; i < num_params; i++)
		free_param(&params[i], num_fields);

	free(params);
}

static int file_exists(const char *fname)
{

	FILE *fd = fopen(fname, "r");

	if (!fd)
		return 0;
	else {
		fclose(fd);
		return 1;
	}

}

void free_file(struct file_info_t *file)
{
	if (!file)
		return;

	if (file->fd) {
		fclose(file->fd);
		file->fd = NULL;
	}

	if (file->line_buf) {
		free(file->line_buf);
		file->line_buf = NULL;
	}

	free(file);
}

struct file_info_t *create_file(const char *fname, const char *mode,
	int max_line_len)
{
	struct file_info_t *file =
		(struct file_info_t *) malloc(sizeof(struct file_info_t));

	if (!file)
		return NULL;

	file->fd = fopen(fname, mode);
	if (!file->fd) {
		pr_err("Error opening file %s\n", fname);
		free(file);
		return NULL;
	}

	file->line_buf = (char *) malloc(max_line_len);
	if (!file->line_buf) {
		free(file);
		return NULL;
	}
	file->max_line_len = max_line_len;

	return file;
}

static void free_csv_hdr(struct csv_header_t *header)
{
	if (!header)
		return;

	if (header->file)
		free_file(header->file);

	if (header->field_names)
		free_fields(header->field_names, header->num_fields);
}




struct csv_header_t *create_csv_hdr(const char *fname)
{
	struct csv_header_t *header =
		(struct csv_header_t *) malloc(sizeof(struct csv_header_t));

	if (!header)
		return NULL;

	memset(header, 0, sizeof(struct csv_header_t));

	header->file = create_file(fname, "r", MAX_LINE_LEN_GG_CSV);
	if (!header->file)
		return NULL;

	return header;
}

static void free_csv(struct csv_info_t *csv)
{
	if (!csv)
		return;

	if (csv->params)
		free_params(csv->params, csv->num_params,
			csv->header->num_fields);

	if (csv->header)
		free_csv_hdr(csv->header);

	free(csv);
}

struct csv_info_t *create_csv(const char *fname)
{
	struct csv_info_t *csv = (struct csv_info_t *)
		malloc(sizeof(struct csv_info_t));

	if (!csv)
		return NULL;

	memset(csv, 0, sizeof(struct csv_info_t));

	csv->header = create_csv_hdr(fname);
	if (!csv->header)
		return NULL;

	return csv;
}

static void free_exp_fmt(struct export_format_t *exp)
{
	if (!exp)
		return;

	if (exp->fields)
		free_fields(exp->fields, exp->num_fields);

	if (exp->fmt_csv_to_params_csv_fld_map)
		free(exp->fmt_csv_to_params_csv_fld_map);

	free(exp);
}

struct export_format_t *parse_export_format(const char *fname)
{
	int i, ret = 0;
	char *line;
	struct file_info_t *file = NULL;
	struct export_format_t *exp = NULL;

	file = create_file(fname, "r", MAX_LINE_LEN_GG_CSV);
	exp = (struct export_format_t*) malloc(sizeof(struct export_format_t));

	if (!exp || !file)
		goto end;

	while ((line = read_line(file))) {
		if ((line = strstr_end(line, fld_ordr_marker)))
		{
			exp->num_fields = extract_csv_fields(line, &exp->fields, MAX_CSV_FIELDS);
			break;
		}
	}


	if (!line || !exp->num_fields)
		goto end;

	/* Find the index of "Parameter Value" field */
	for (i = 0; i < exp->num_fields; i++) {
		if (strcicmp(exp->fields[i], used_fields[USED_FLD_VALUE]) == 0) {
			exp->value_fld_ind = i;
			break;
		}
	}

	if (i == exp->num_fields) {
		pr_err("couldn't find field \"Paramter Value\" in --format-csv\n");
		goto end;
	}


	exp->fmt_csv_to_params_csv_fld_map = (uint8_t *) malloc(exp->num_fields);
	if (!exp->fmt_csv_to_params_csv_fld_map)
		goto end;

	ret = 1;
	return exp;

end:
	free_file(file);

	if (!ret) {
		free_exp_fmt(exp);
		return NULL;
	}

	return exp;
}

static uint8_t find_key_from_csv(struct csv_info_t *csv, const char *key_name, uint32_t *key)
{
	int i;
	const char *str;
	char *tmp;

	/*
	 * Keys are typically at the end. So, start from the end
	 * We will do this unconditionally because we want to see
	 * if we will need full access keys.
	 */
	for (i = csv->num_params - 1; i >= 0; i--) {
		str = csv->params[i].fields[csv->used_field_index[USED_FLD_NAME]];
		if (strcicmp(str, key_name) == 0) {
			str = csv->params[i].fields[csv->used_field_index[USED_FLD_VALUE]];
			*key = strtoul(str, &tmp, 16);
			return 1;
		}
	}

	return 0;
}


struct csv_info_t *parse_csv_file(const char *fname, struct export_format_t *exp)
{
	struct csv_info_t *csv = create_csv(fname);
	int ret = 1;

	if (!csv)
		goto error;

	if (!parse_header(csv->header)) {
		pr_err("error parsing header of csv: %s\n", fname);
		goto error;
	}


	if (!get_field_map(csv->used_field_index, (char **) used_fields,
		sizeof(used_fields) / sizeof(used_fields[0]),
		csv->header->field_names, csv->header->num_fields)) {
		pr_err("error getting used field indices\n");
		goto error;
	}
	csv->value_fld_ind = csv->used_field_index[USED_FLD_VALUE];

	if (exp) {
		ret = get_field_map(exp->fmt_csv_to_params_csv_fld_map, exp->fields,
				exp->num_fields, csv->header->field_names,
				csv->header->num_fields);
		if (!ret) {
			pr_err("Error mapping --format-csv fields to"
				" --params-csv fields. Falling back to"
				" --params-csv for export format\n")
			exp = NULL;
		}
		/* Discard unused fields in the header */

	}

	if (exp) {
		ret = discard_unused_flds(&csv->header->field_names, exp->num_fields,
				exp->fmt_csv_to_params_csv_fld_map);
	}


	if (ret && !parse_params(csv, exp)) {
		pr_err("Error parsing params\n");
		goto error;
	}

	if (exp) {
		csv->header->num_fields = exp->num_fields;
		csv->value_fld_ind = exp->value_fld_ind;
	}

	/* See if we can find the unseal keys from the csv */
	csv->unseal_key_found =
		find_key_from_csv(csv, "Sealed to Unsealed", &csv->unseal_key);
	csv->fullaccess_key_found =
		find_key_from_csv(csv, "Unsealed to Full", &csv->fullaccess_key);

	return csv;

error:
	free_csv(csv);

	return NULL;

}

static int check_device_match(struct gauge_info_t *gauge, struct csv_info_t *csv)
{
	if ( csv->header->device_num != gauge->device_num ||
		csv->header->fw_version != gauge->fw_version ||
		(csv->header->fw_ver_bld &&  (csv->header->fw_ver_bld != gauge->fw_ver_bld)))
	{
		pr_err("The Device Number or FW version in the gg.csv"
			" don't match with respective value read from the"
			" device.. CSV: 0x%04x 0x%04x 0x%04x Gauge: 0x%04x"
			" 0x%04x 0x%04x\n", csv->header->device_num,
			csv->header->fw_version, csv->header->fw_ver_bld,
			gauge->device_num, gauge->fw_version, gauge->fw_ver_bld);
		return 0;
	}

	return 1;
}

/*
 * These values will be overridden later by the gauge related code
 * if we can independently find them from commandline inputs
 */
static void init_gauge_from_csv(struct gauge_info_t *gauge, struct csv_info_t *csv)
{
	gauge->unseal_key_found = csv->unseal_key_found;
	gauge->unseal_key = csv->unseal_key;
	gauge->fullaccess_key_found = csv->fullaccess_key_found;
	gauge->fullacccess_key = csv->fullaccess_key;
}

static void print_csv_row(FILE *fd, const char *first_word, char **fields,int num_fields)
{
	int i;

	fprintf(fd, "%s", first_word);

	for (i = 0; i < num_fields; i++)
		fprintf(fd, ",%s", fields[i]);

	fprintf(fd, "\n");
}

static void print_csv_fld_ordr(FILE *fd, const char *first_word, char **fields,int num_fields)
{
	int i;

	fprintf(fd, "%s", first_word);

	for (i = 0; i < num_fields; i++)
		fprintf(fd, ", %s", fields[i]);

	fprintf(fd, "\n");
}

static int write_csv_file(struct csv_info_t *csv, struct export_info_t *exp,
	const char *timestamp)
{
	char buf[MAX_USED_FLD_LEN];
	time_t now;
	struct tm *t = NULL;
	uint32_t i;
	struct param_t *params = csv->params;
	uint8_t tmp1, tmp2;
	FILE *fd = exp->fd;

	if (!exp->no_hdr) {
		now = time(NULL);
		t = localtime(&now);

		fprintf(fd, "* Texas Instruments Data Flash File\n");
		fprintf(fd, "* File created %s", asctime(t));
		fprintf(fd, "* Created by bqtool\n");
		fprintf(fd, "* Device Number %x\n", csv->header->device_num);
		tmp1 = (uint8_t) ((csv->header->fw_version >> 8) & 0xFF);
		tmp2 = (uint8_t) (csv->header->fw_version & 0xFF);
		fprintf(fd, "* Firmware Version %x.%02x", tmp1, tmp2);
		if (csv->header->fw_ver_bld)
			fprintf(fd, ".%02x\n", csv->header->fw_ver_bld);
		else
			fprintf(fd, "\n");

		fprintf(fd, "* Build Number not available\n");
		fprintf(fd, "* Order Number not available\n*\n");
		fprintf(fd, "* bqz Device Number not available\n");
		fprintf(fd, "* bqz Firmware Version not available\n");
		fprintf(fd, "* bqz Build Number not available\n*\n");
		if (timestamp) {
			/* First column shall be the timestamp */
			snprintf(buf, MAX_USED_FLD_LEN, "%s %s",
				fld_ordr_marker, "Timestamp");
			print_csv_fld_ordr(fd, buf, csv->header->field_names,
				csv->header->num_fields);
		} else {
			snprintf(buf, MAX_USED_FLD_LEN, "%s %s",
				fld_ordr_marker, csv->header->field_names[0]);
			print_csv_fld_ordr(fd, buf, &csv->header->field_names[1],
				csv->header->num_fields - 1);
		}
	}

	/* Export all fields */
	for (i = 0; i < csv->num_params; i++) {
		if (timestamp) {
			print_csv_row(fd, timestamp, params[i].fields,
				csv->header->num_fields);
		} else {
			snprintf(buf, MAX_USED_FLD_LEN, "%s",
				params[i].fields[0]);
			print_csv_row(fd, buf, &params[i].fields[1],
				csv->header->num_fields - 1);
		}

	}

	fflush(fd);

	return !ferror(fd);
}

const char *get_cmdline_argument(int argc, char **argv, const char *arg_identifier, int arg_with_val)
{
	int i;
	uint32_t len;

	for (i = 2; i < argc; i++) {
		if (strstr(argv[i], arg_identifier) == argv[i]) {
			/*
			 * For switches such as --export-header
			 * return the whole string
			 */
			if (!arg_with_val)
				return argv[i];
			/*
			 * For arguments such as --params-csv=<csv-file-name>
			 * return only the <csv-file-name> part
			 */
			len = strlen(arg_identifier);
			if (strlen(argv[i]) > len)
				return argv[i] + len;
			else
				return NULL;
		}
	}

	pr_dbg("could not find argument \"%s\"\n", arg_identifier);

	return NULL;
}

struct gauge_info_t *init_gauge(struct csv_info_t *csv, int argc,
	char **argv, uint8_t op)
{
	struct gauge_info_t *gauge = create_gauge();
	if (!gauge)
		return NULL;
	gauge->op = op;

	if ((op == OP_DM_READ || op == OP_DM_WRITE) && csv)
		init_gauge_from_csv(gauge, csv);

	if (!init_gauge_interface(gauge, argc, argv))
		goto error;

	if (op == OP_DM_WRITE && !check_device_match(gauge, csv))
		goto error;

	return gauge;
error:
	free_gauge(gauge);
	return NULL;
}

static int import_gg_csv(int argc, char **argv)
{
	const char *fname;
	int err = 1;
	struct gauge_info_t *gauge = NULL;
	struct csv_info_t *csv = NULL;

	fname = get_cmdline_argument(argc, argv, "--params-csv=", 1);
	if (!fname) {
		pr_err("--param-csv not found\n");
		return 0;
	}

	csv = parse_csv_file(fname, NULL);
	if (!csv)
		goto end;

	gauge = init_gauge(csv, argc, argv, OP_DM_WRITE);
	if (!gauge)
		goto end;

	gauge->lock_gauge_interface(gauge);

	if (!gauge->open_dm(gauge, argc, argv))
		goto end;

	err = !gauge->write_params(csv, gauge);


	if (get_cmdline_argument(argc, argv, "--reset", 0)) {
		gauge->reset(gauge);
		gauge->sleep_ms(10000);
	}

	gauge->close_dm(gauge, argc, argv);

end:
	if (gauge) {
		gauge->unlock_gauge_interface(gauge);
		free_gauge(gauge);
	}

	free_csv(csv);

	return !err;
}

static void free_exp_info(struct export_info_t *exp)
{
	if (!exp)
		return;

	if (exp->fd && exp->fd != stdout)
		fclose(exp->fd);

	free_exp_info(exp);
}

struct export_info_t *get_export_info(int argc, char **argv)
{
	const char *no_hdr, *prd, *timestamp, *output_csv;
	struct export_info_t *exp_info;
	FILE *fd = stdout;

	exp_info = (struct export_info_t *) malloc(sizeof(struct export_info_t));
	if (!exp_info)
		return NULL;

	memset(exp_info, 0, sizeof(struct export_info_t));

	prd = get_cmdline_argument(argc, argv, "--period=", 1);
	if (prd)
		exp_info->period = strtoul(prd, NULL, 10);

	timestamp = get_cmdline_argument(argc, argv, "--timestamp", 0);
	exp_info->timestamp = timestamp ? 1 : 0;

	output_csv = get_cmdline_argument(argc, argv, "--output-csv=", 1);
	if (output_csv)
		fd = fopen(output_csv, "w");

	if (!fd) {
		fd = stdout;
		pr_err("error opening output csv %s redirecting"
			" to stdout instead", output_csv);
	}
	exp_info->fd = fd;

	no_hdr = get_cmdline_argument(argc, argv, "--no-header", 0);
	exp_info->no_hdr = no_hdr ? 1 : 0;

	return exp_info;
}

struct csv_info_t *create_export_csv(int argc, char **argv,
	const char *ip_csv_marker, const char *fmt_csv_marker)
{
	const char *input_csv, *format_csv;
	struct csv_info_t *csv;
	struct export_format_t *exp_fmt = NULL;

	input_csv = get_cmdline_argument(argc, argv, ip_csv_marker, 1);
	if (!input_csv)
		return NULL;

	format_csv = get_cmdline_argument(argc, argv, fmt_csv_marker, 1);
	if (format_csv)
		exp_fmt = parse_export_format(format_csv);

	csv = parse_csv_file(input_csv, exp_fmt);

	free_exp_fmt(exp_fmt);

	return csv;
}

static void copy_dev_info_from_gauge(struct csv_info_t *csv,
	struct gauge_info_t *gauge)
{
	if (!csv)
		return;

	csv->header->device_num = gauge->device_num;
	csv->header->fw_version = gauge->fw_version;
	csv->header->fw_ver_bld = gauge->fw_ver_bld;
}

static int export_loop(struct csv_info_t *params_csv, struct csv_info_t *regs_csv,
	struct gauge_info_t *gauge, int argc, char **argv)
{
	int err = 0;
	time_t now;
	struct tm *t = NULL;
	char buf[MAX_USED_FLD_LEN];
	const char *timestamp = NULL;
	struct export_info_t *exp_info = NULL;

	if (!regs_csv && !params_csv) {
		pr_err("no valid csv for exporting\n");
		return 0;
	}

	exp_info = get_export_info(argc, argv);
	if (!exp_info)
		goto end;

	copy_dev_info_from_gauge(params_csv, gauge);
	copy_dev_info_from_gauge(regs_csv, gauge);

	while (1) {
		if (exp_info->timestamp) {
			now = time(NULL);
			t = localtime(&now);
			strftime(buf, MAX_USED_FLD_LEN,
				"%m/%d/%y %H:%M:%S", t);
			timestamp = buf;
		}

		gauge->lock_gauge_interface(gauge);

		if (!err && regs_csv) {
			err |= !read_regs(regs_csv, gauge);
			if (!err)
				err |= !write_csv_file(regs_csv, exp_info, timestamp);
		}

		if (!err && params_csv) {
			if ((err = !gauge->open_dm(gauge, argc, argv)))
				break;
			err |= !gauge->read_params(params_csv, gauge);
			gauge->close_dm(gauge, argc, argv);
			if (err)
				break;
			err |= !write_csv_file(params_csv, exp_info, timestamp);
		}

		gauge->unlock_gauge_interface(gauge);

		if (err || !exp_info->period)
			break;

		gauge->sleep_ms(exp_info->period);
	};

end:
	gauge->unlock_gauge_interface(gauge);
	free_exp_info(exp_info);

	return !err;
}

static int export_gg_csv(int argc, char **argv)
{
	struct csv_info_t *csv = NULL;
	struct gauge_info_t *gauge = NULL;
	int ret = 0;

	csv = create_export_csv(argc, argv, "--params-csv=",
		"--format-csv=");
	if (!csv)
		goto end;

	gauge = init_gauge(csv, argc, argv, OP_DM_READ);
	if (!gauge)
		goto end;

	ret = export_loop(csv, NULL, gauge, argc, argv);

end:
	free_csv(csv);
	free_gauge(gauge);

	return ret;
}

static int export_regs(int argc, char **argv)
{
	struct csv_info_t *csv = NULL;
	struct gauge_info_t *gauge = NULL;
	int ret = 0;

	csv = create_export_csv(argc, argv, "--regs-csv=",
		"--regs-format-csv=");
	if (!csv)
		goto end;

	gauge = init_gauge(csv, argc, argv, OP_REG_DUMP);
	if (!gauge)
		goto end;

	ret = export_loop(NULL, csv, gauge, argc, argv);

end:
	free_csv(csv);
	free_gauge(gauge);

	return ret;
}

static int combined_export(int argc, char **argv)
{
	struct csv_info_t *regs_csv = NULL, *dm_csv = NULL;
	struct gauge_info_t *gauge = NULL;
	int ret = 0;

	regs_csv = create_export_csv(argc, argv, "--regs-csv=",
		"--regs-format-csv=");
	dm_csv = create_export_csv(argc, argv, "--dm-csv=",
		"--dm-format-csv=");
	if (!regs_csv || !dm_csv)
		goto end;

	gauge = init_gauge(dm_csv, argc, argv, OP_DM_READ);
	if (!gauge)
		goto end;

	ret = export_loop(dm_csv, regs_csv, gauge, argc, argv);

end:
	free_csv(regs_csv);
	free_csv(dm_csv);
	free_gauge(gauge);

	return ret;
}

#define MAX_ROM_MONITOR_ERR_CNT 5

static int rom_gauge_monitor(int argc, char **argv)
{
	const char *init, *save_rest;
	struct csv_info_t *csv = NULL;
	int ret, err, err_cnt;
	struct export_info_t *exp_info = NULL;
	time_t now;
	struct tm *t = NULL;
	char buf[MAX_USED_FLD_LEN];
	const char *timestamp = NULL;
	struct gauge_info_t *gauge = NULL;

begin:
	ret = err = err_cnt = 0;
	/*
	 * Let's not rely on ITPOR and initialize the device
	 * unconditionally here. Later it will be re-initialized
	 * if we detect ITPOR in the loop
	 */
	init = get_cmdline_argument(argc, argv, "--init-csv=", 1);
	if (!init) {
		pr_err("--init-csv not provided\n");
		return 0;
	}

	csv = parse_csv_file(init, NULL);
	if (!csv)
		goto end;

	gauge = init_gauge(csv, argc, argv, OP_DM_WRITE);
	if (!gauge)
		goto end;

	gauge->lock_gauge_interface(gauge);

	if (!gauge->open_dm(gauge, argc, argv))
		goto end;

	pr_info("Programming --init-csv %s\n", init);
	err |= !gauge->write_params(csv, gauge);
	if (err) {
		pr_err("Error programming %s\n", init);
	}

	save_rest= get_cmdline_argument(argc, argv, "--save-restore-csv=", 1);
	if (!save_rest || !file_exists(save_rest)) {
		pr_err("--save-restore-csv= not provided or the file not"
			" found. periodic save will not be performed..\n");
		goto end;
	}

	/* --save-restore-csv exists, program it */
	free_csv(csv);
	csv = parse_csv_file(save_rest, NULL);
	if (!csv)
		goto end;

	/* Restore the params */
	pr_info("restoring params from %s\n", save_rest);
	err |= !gauge->write_params(csv, gauge);
	gauge->close_dm(gauge, argc, argv);
	if (err) {
		pr_err("Error initializing ROM gauge. Aborting monitoring!");
		goto end;
	}


	pr_info("Initialization successful! Start monitoring..\n");
	/*
	 * Close the --save-restore-csv file, we need to reopen it with
	 * "w" mode for saving params
	 */
	free_file(csv->header->file);
	csv->header->file = NULL;

	exp_info = get_export_info(argc, argv);
	exp_info->no_hdr = 0;
	if (!exp_info->period) {
		pr_err("--period= not provided, assuming 1 min\n");
		exp_info->period = 60000;
	}

	err_cnt = 0;
	while (err_cnt < MAX_ROM_MONITOR_ERR_CNT) {

		gauge->lock_gauge_interface(gauge);

		if (gauge->itpor(gauge)) {
			free_gauge(gauge);
			gauge = NULL;
			free_csv(csv);
			csv = NULL;
			pr_err("Reset detected.. Re-initializing..\n")
			goto begin;
		}

		err = !gauge->read_params(csv, gauge);
		if (exp_info->timestamp) {
			now = time(NULL);
			t = localtime(&now);
			strftime(buf, MAX_USED_FLD_LEN, "%m/%d/%y %H:%M:%S", t);
			timestamp = buf;
		}

		gauge->unlock_gauge_interface(gauge);

		if (err) {
			err_cnt++;
			continue;
		} else {
			err_cnt = 0;
			exp_info->fd = fopen(save_rest, "w");
			if (exp_info->fd) {
				pr_info("saving params to %s\n", save_rest);
				err |= !write_csv_file(csv, exp_info, timestamp);
				fclose(exp_info->fd);
			}
		}


		gauge->sleep_ms(exp_info->period);
	}

	if (err_cnt >= MAX_ROM_MONITOR_ERR_CNT)
		pr_err("Error count exceeded limit.. Exiting..\n");

end:
	free_csv(csv);

	if (gauge) {
		gauge->unlock_gauge_interface(gauge);
		free_gauge(gauge);
	}

	free_exp_info(exp_info);

	return ret && !err;
}

static void free_bqfs_cmd(struct bqfs_cmd_t *cmd)
{
	if (!cmd)
		return;

	if (cmd->buf)
		free(cmd->buf);

	if (cmd->tmp_buf)
		free(cmd->tmp_buf);

	free(cmd);
}


/*
 * Returns:
 * number of bytes extracted
 */
static int extract_hex_byte_stream(char **str, uint8_t *buf, int max_len)
{
	char *tok, *tmp;
	unsigned long val;
	int i = 0;

	/*
	 * We can do this using strtoul without tokenizing.
	 * But let's tokenize in the interest of better
	 * error handling
	 */
	while ((tok = get_delim_separated_item(str, ' ')) && i < max_len) {
		val = strtoul(tok, &tmp, 16);
		//pr_dbg("%s\n", tok);
		if (tmp == tok) {
			pr_err("token appears to be not a hex number: %s\n",
				tok);
			return 0;
		}

		buf[i++] = (uint8_t) val;
	}

	return i;
}

#ifdef BQTOOL_DEBUG
static void pr_dbg_bytes(uint8_t *buf, uint32_t size)
{
	uint32_t i;

	for (i = 0; i < size; i++)
		pr_dbg_raw(" %02x", buf[i]);

}
#else
static void pr_dbg_bytes(uint8_t *buf, uint32_t size)
{
}
#endif

/*
 * Returns:
 * 1  - Command execution successful
 * -1 - Command execution failed
 */
static int bqfs_exec_cmd(struct gauge_info_t *gauge, struct bqfs_cmd_t *cmd)
{
	int cmd_base_len, ret = 1;
	uint8_t data_len, *cmd_buf;

	cmd_base_len = (cmd->mode == BQFS_HDQ) ? 1 : 2;

	/*
	 * I2C:
	 * cmd->buf[0] => slave address
	 * cmd->buf[1] => reg address
	 * cmd->buf[2] onwards data
	 *
	 * HDQ
	 * cmd->buf[0] => reg address
	 * cmd->buf[1] onwards data
	 *
	 * For HDQ, slave address assigned below is not correct,
	 * but it's never used.
	 */
	gauge->slave_addr = cmd->buf[0];

	/* Register address is part of the command data in the read/write API */
	cmd_buf = &cmd->buf[cmd_base_len - 1];

	/* data_len is the actual data length, doesn't count reg address */
	data_len = cmd->len - cmd_base_len;

	switch (cmd->cmd) {
	case 'R':
		/*
		 * This command is not really useful now
		 * It may be useful later when implementing dfpo
		 * support
		 */
		ret = do_read(gauge, cmd_buf, data_len);
		break;
	case 'W':
		ret = do_write(gauge, cmd_buf, data_len);
		break;
	case 'C':
		/*
		 * Copy data to compare later. Command buffer
		 * includes register + data
		 */
		/*
		memcpy(cmd->tmp_buf, &cmd_buf[1], data_len);
		ret = do_read(gauge, cmd_buf, data_len);
		*/
		memset(cmd->tmp_buf, 0x00, cmd->max_len);
		/* Copy reg address */
		memcpy(cmd->tmp_buf, cmd_buf, 1);
		ret = do_read(gauge, cmd->tmp_buf, data_len);
		if (!ret || memcmp(&(cmd->tmp_buf[1]), &cmd_buf[1], data_len)) {
			/*
			pr_err("\nFailed to execute command C at line %d:\n", cmd->line_num);
			pr_err("Expected data: ");
			pr_dbg_bytes(&cmd_buf[1], data_len);
			pr_err("\nReceived data: ");
			pr_dbg_bytes(cmd->tmp_buf, data_len + 1);
			pr_err("\n");
			*/
			ret = -1;
		}
		break;
	case 'X':
		gauge->sleep_ms(cmd->delay);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret <= 0) {
		pr_err("command execution failed at line %d\n", cmd->line_num);
		return -1;
	}

	return 1;
}

/*
 * Returns:
 * 1  - successfully found a new command
 * 0  - End of stream
 * -1 - Error parsing command
 */
static int bqfs_get_cmd(struct file_info_t *file, struct bqfs_cmd_t *cmd)
{
	char *tok, *buf, *tmp;
	int ret, cmd_base_len;
	unsigned long int val;

	pr_dbg(">>\n");
	/*
	 * Get the first non-comment line:
	 * Comment liens start with ';'
	 */
	while ((buf = read_line(file)) && buf[0] == ';');

	if (!buf)
		return 0;

	tok = get_delim_separated_item(&buf, ':');
	if (!tok || (strlen(tok) != 1)) {
		pr_err("Error parsing command at line %d tok=%s buf=%s\n",
			file->line_num, tok, buf);
		return -1;
	}

	cmd->cmd = toupper(tok[0]);
	cmd->line_num = file->line_num;

	cmd_base_len = (cmd->mode == BQFS_HDQ) ? 1 : 2;

	pr_dbg_raw("%c:", cmd->cmd);

	switch (cmd->cmd) {
	case 'R':
		ret = extract_hex_byte_stream(&buf, cmd->buf, cmd_base_len);
		if (ret != cmd_base_len)  {
			pr_err("error parsing read cmd at line %d buf=%s\n",
				file->line_num, buf);
			return -1;
		}
		val = strtoul(buf, &tmp, 10);
		cmd->len = (uint8_t) val;
		if (buf == tmp || val != cmd->len) {
			pr_err("error parsing rd cmd len at line %d buf=%s\n",
				file->line_num, buf);
			return -1;
		}
		pr_dbg_bytes(cmd->buf, cmd_base_len);
		pr_dbg_raw("%d\n", cmd->len);
		break;
	case 'W':
	case 'C':
		//pr_dbg("%x\n", &buf);
		cmd->len = extract_hex_byte_stream(&buf, cmd->buf, cmd->max_len);
		if (cmd->len < cmd_base_len + 1)  {
			pr_err("error parsing cmd bytes at line %d buf=%s\n",
				file->line_num, buf);
			return -1;
		}

		pr_dbg_bytes(cmd->buf, cmd->len);
		pr_dbg_raw("\n");
		break;
	case 'X':
		val = strtoul(buf, &tmp, 10);
		cmd->delay = (uint16_t) val;
		if (buf == tmp || val != cmd->delay) {
			pr_err("Error parsing delay at line %d buf=%s\n",
				file->line_num, buf);
			return -1;
		}
		pr_dbg_bytes(cmd->buf, cmd_base_len);
		pr_dbg_raw(" %d\n", cmd->delay);
		break;
	default:
		pr_err("No command or unexpected command at"
			" line %d tok=\"%s\" buf=\"%s\"",
			file->line_num, tok, buf);
		return -1;
	}

	return 1;
}

static int bqfs_flash(int argc, char **argv)
{
	struct file_info_t *file = NULL;
	const char *fname, *max_block_len, *mode;
	struct bqfs_cmd_t *cmd = NULL;
	struct gauge_info_t *gauge = NULL;
	long long int max_len = 0;
	int ret = 0, i;
	uint8_t slave_addr;

	fname = get_cmdline_argument(argc, argv, "--bqfs-file=", 1);
	if (!fname) {
		pr_err("--bqfs-file not provided\n");
		goto end;
	}

	cmd = (struct bqfs_cmd_t *) malloc(sizeof(struct bqfs_cmd_t));
	if (!cmd)
		goto end;
	mode = get_cmdline_argument(argc, argv, "--mode=", 1);

	if (mode && strcicmp(mode, "hdq") == 0)
		cmd->mode = BQFS_HDQ;
	else
		cmd->mode = BQFS_I2C;

	max_block_len = get_cmdline_argument(argc, argv,
		"--max-block-len=", 1);
	if (max_block_len)
		extract_integer(max_block_len, &max_len);
	if (!max_len)
		max_len = BQFS_MAX_DATA_BLOCK_LEN;
	cmd->max_len = (cmd->mode == BQFS_HDQ) ? max_len + 1: max_len + 2;

	file = create_file(fname, "r", (cmd->max_len + 2) * 3);
	if (!file)
		goto end;

	cmd->buf = (uint8_t*) malloc(cmd->max_len);
	cmd->tmp_buf = (uint8_t*) malloc(cmd->max_len);
	if (!cmd->buf || !cmd->tmp_buf)
		goto end;

        gauge = init_gauge(NULL, argc, argv, OP_BQFS_FLASH);
	if (!gauge)
		goto end;

	/* gauge->open_dm(gauge, argc, argv); */
	/* Save the FW mode slave addr before executing the bqfs */
	slave_addr = gauge->slave_addr;

	/* gauge->lock_gauge_interface(gauge); */
	ret = bqfs_get_cmd(file, cmd);
	while(ret > 0) {
		i = 0;
		while (1) {
			gauge->sleep_ms(20);
			ret = bqfs_exec_cmd(gauge, cmd);
			if (ret > 0) {
				break;
			}

			if (++i == 3) {
				goto end;
			}

			pr_err("Retrying bqfs cmd line %d..\n", cmd->line_num);
		}

		if (ret == 1) {
			fputc('.', stdout);
			fflush(stdout);
		} else {
			break;
		}

		ret = bqfs_get_cmd(file, cmd);
	}
	fputc('\n', stdout);

	if (get_cmdline_argument(argc, argv, "--reset", 0))
		gauge->reset(gauge);

	/* Restore the slave adress */
	gauge->slave_addr = slave_addr;
	/* gauge->close_dm(gauge, argc, argv); */

	/* gauge->unlock_gauge_interface(gauge); */
	/*
	 * If ret == 0, reached end of stream without any errors
	 */
	if (ret == 0)
		ret = 1;
	else
		ret = 0;

end:
	pr_dbg("<<\n");
	free_file(file);
	free_bqfs_cmd(cmd);
	free_gauge(gauge);

	return ret;
}

static const struct command_t commands[] = {
#ifdef CMD_IMPORT_GG_CSV
	{"--import-gg-csv", import_gg_csv, "--params-csv=<gg-csv> [--unseal-key=<key> --fullaccess-key=<key> --i2c-dev-file=<dev-file> --i2c-bus=<bus-num> --reset --exit-seal=<seal-state>]"},
#endif
#ifdef CMD_ROM_GAUGE_MONITOR
	{"--rom-gauge-monitor", rom_gauge_monitor, "--init-csv=<gg-csv> --save-restore-csv=<gg-csv> --period=<prd-ms> [--timestamp --unseal-key=<key> --fullaccess-key=<key> --i2c-dev-file=<dev-file> --i2c-bus=<bus-num> --exit-seal=<seal-state>]"},
#endif
#ifdef CMD_BQFS_FLASH
	{"--bqfs-flash", bqfs_flash, "--bqfs-file=<bqfs/dffs file> [--mode=<interface> --max-block-len=<max-blk-len> --unseal-key=<key> --fullaccess-key=<key> --i2c-dev-file=<dev-file> --i2c-bus=<bus-num> --reset --exit-seal=<seal-state>]"},
#endif
#ifdef CMD_EXPORT_GG_CSV
	{"--export-gg-csv", export_gg_csv, "--params-csv=<gg-csv> [--output-csv=<csv-file> --format-csv=<csv-file> --period=<prd-ms> --timestamp --no-header --unseal-key=<key> --fullaccess-key=<key> --i2c-dev-file=<dev-file> --i2c-bus=<bus-num> --exit-seal=<seal-state>]"},
#endif
#ifdef CMD_EXPORT_REGS
	{"--export-regs", export_regs, "--regs-csv=<regs-csv> [--output-csv=<csv-file> --regs-format-csv=<regs-csv> --i2c-dev-file=<dev-file> --i2c-bus=<bus-num>]"},
#endif
#ifdef CMD_COMBINED_EXPORT
	{"--combined-export", combined_export, "--dm-csv=<gg-csv> --regs-csv=<regs-csv> [--output-csv=<csv-file> --dm-format-csv==<csv-rile> --regs-format-csv=<regs-csv> --period=<prd-ms> --timestamp --no-header --unseal-key=<key> --fullaccess-key=<key> --i2c-dev-file=<dev-file> --i2c-bus=<bus-num> --exit-seal=<seal-state>]"},
#endif
};

const char *bqt_readme = "https://git.ti.com/bms-linux/bqtool/blobs/master/README";

static void usage(int cmd_ind)
{
	unsigned int i;

	pr_err_raw("Usage:\n");
	if (cmd_ind == -1) {
		for (i = 0; i < ARRAY_SIZE(commands); i++) {
			pr_err_raw("%s %s\n\n", commands[i].cmd_name,
				commands[i].help);
		}
	} else {
		pr_err_raw("%s %s\n\n", commands[cmd_ind].cmd_name,
			commands[cmd_ind].help);
	}

	pr_err_raw("Please see %s for details of command options\n",
		bqt_readme);
}

int main(int argc, char **argv)
{
	unsigned int i;
	int ret = -1;

	if (argc < 2) {
		usage(-1);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[1], commands[i].cmd_name) == 0) {
			ret = commands[i].cmd_func(argc, argv);
			break;
		}
	}

	/*
	if (ret == -1) {
		pr_err("Unrecognized command: %s\n", argv[1]);
		usage(-1);
		return -1;
	} else if (ret == 0) {
		pr_err("comamnd \"%s\" failed\n: ", argv[1]);
		usage(i);
		return -1;
	} else {
		pr_info("comamnd \"%s\" executed successfully!\n", argv[1]);
	}
	*/
	if (ret == 1) {
		pr_info("comamnd \"%s\" executed successfully!\n", argv[1]);
	} else {
		pr_info("comamnd \"%s\" executed failed!\n", argv[1]);
	}

	return 0;
}
