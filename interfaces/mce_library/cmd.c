/*******************************************************

 cmd.c - most command module routines live here

*******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <mce_library.h>

#include <mce/mce_ioctl.h>

#include "virtual.h"

#define LOG_LEVEL_CMD     LOGGER_DETAIL
#define LOG_LEVEL_REP_OK  LOGGER_DETAIL
#define LOG_LEVEL_REP_ER  LOGGER_INFO


inline int get_last_error(mce_context_t *context)
{
	return ioctl(C_cmd.fd, MCEDEV_IOCT_LAST_ERROR);
}


int log_data( logger_t *logger,
	      u32 *buffer, int count, int min_raw, char *msg,
	      int level)
{
	char out[2048];
	char *s = out + sprintf(out, msg);

	if (count > 2048/5) count = 2048/5;

	if (min_raw > count) min_raw = count;

	int idx = 0;
	while ( (idx < min_raw) || ((buffer[idx] != 0) && idx < count) )
		s += sprintf(s, " %08x", buffer[idx++]);

	while (idx<count) {
		int zero_idx = idx;
		while (idx<count && (buffer[idx] == 0))
			idx++;

		if (idx-zero_idx >= 2) {
			s += sprintf(s, " [%08x x %x]", 0, idx-zero_idx);
		} else {
			while (zero_idx<idx)
				s += sprintf(s, " %08x", buffer[zero_idx++]);
		}

		while ( (idx < count) && (buffer[idx] != 0) )
			s += sprintf(s, " %08x", buffer[idx++]);
	}

	return logger_print_level(logger, out, level);
}


int mcecmd_open (mce_context_t *context, char *dev_name)
{
	if (C_cmd.connected) mcecmd_close(context);

	if (strlen(dev_name)>=MCE_LONG-1)
		return -MCE_ERR_BOUNDS;

	C_cmd.fd = open(dev_name, O_RDWR);
	if (C_cmd.fd<0) return -MCE_ERR_DEVICE;

	// Set up connection to prevent outstanding replies after release
	ioctl(C_cmd.fd, MCEDEV_IOCT_SET,
	      ioctl(C_cmd.fd, MCEDEV_IOCT_GET) | MCEDEV_CLOSE_CLEANLY);


	C_cmd.connected = 1;
	strcpy(C_cmd.dev_name, dev_name);

	return 0;
}


int mcecmd_close(mce_context_t *context)
{
	C_cmd_check;

	if (close(C_cmd.fd) < 0)
		return -MCE_ERR_DEVICE;

	C_cmd.connected = 0;

	return 0;
}


/* Basic device write/read routines */

int mcecmd_send_command_now(mce_context_t* context, mce_command *cmd)
{	
	int error = write(C_cmd.fd, cmd, sizeof(*cmd));
	if (error < 0) {
		return -MCE_ERR_DEVICE;
	} else if (error != sizeof(*cmd)) {
		return get_last_error(context);
	}
	return 0;
}

int mcecmd_read_reply_now(mce_context_t* context, mce_reply *rep)
{
	int error = read(C_cmd.fd, rep, sizeof(*rep));
	if (error < 0) {
		return -MCE_ERR_DEVICE;
	} else if (error != sizeof(*rep)) {
		return get_last_error(context);
	}
	return 0;
}


int mcecmd_send_command(mce_context_t* context, mce_command *cmd, mce_reply *rep)
{
	int err = 0;
	char errstr[MCE_LONG];
	C_cmd_check;

	log_data(&C_logger, (u32*)cmd + 2, 62, 2, "command",
		 LOG_LEVEL_CMD);

	err = mcecmd_send_command_now(context, cmd);
	if (err<0) {
		sprintf(errstr, "command not sent, error %#x.", -err);
		logger_print_level(&C_logger, errstr, LOGGER_INFO);
		memset(rep, 0, sizeof(*rep));
		return err;
	}

	err = mcecmd_read_reply_now(context, rep);
	if (err != 0) {
		sprintf(errstr, "reply [communication error] %s", mcelib_error_string(err));
		logger_print_level(&C_logger, errstr, LOG_LEVEL_REP_ER);
		return err;
	}

	// Analysis of received packet
	err = mcecmd_checksum((u32*)rep, sizeof(*rep) / sizeof(u32)) ?
		-MCE_ERR_CHKSUM : 0;

	if (!err) err = mcecmd_cmd_match_rep(cmd, rep);


	switch (-err) {

	case MCE_ERR_CHKSUM:
		log_data(&C_logger, (u32*)rep, 60, 2,
			 "reply [checksum error] ",
			 LOG_LEVEL_REP_ER);
		break;

	case MCE_ERR_FAILURE:
		log_data(&C_logger, (u32*)rep, 60, 2,
			 "reply [command failed] ",
			 LOG_LEVEL_REP_ER);
		break;

	case MCE_ERR_REPLY:
		log_data(&C_logger, (u32*)rep, 60, 2,
			 "reply [consistency error] ",
			 LOG_LEVEL_REP_ER);
		break;

	case 0:
		log_data(&C_logger, (u32*)rep, 60, 2, "reply  ",
			 LOG_LEVEL_REP_OK);
		break;

	default:
		sprintf(errstr, "reply [strange error '%s'] ",
			mcelib_error_string(err));
		log_data(&C_logger, (u32*)rep, 60, 2,
			 errstr, LOG_LEVEL_REP_ER);
	}

	return err;
}

int mcecmd_load_param(mce_context_t* context, mce_param_t *param,
		      const char *card_str, const char *param_str)
{
	return mceconfig_lookup(context, card_str, param_str,
				&param->card, &param->param);
}


int mcecmd_write_block(mce_context_t* context, const mce_param_t *param,
		       int count, const u32 *data)
{
	// This used to run the show, but these days we pass it to write_range
	return mcecmd_write_range(context, param, 0, data, count);
}

int mcecmd_read_block(mce_context_t* context, const mce_param_t *param,
		      int count, u32 *data)
{
	// Boring... 
	return mcecmd_read_range(context, param, 0, data, count);
}

#define QUICK_FILL(cmd, card, para, n) {		\
		.preamble = {PREAMBLE_0, PREAMBLE_1},	\
			.command  = cmd,		\
				 .card_id  = card,	\
				 .para_id  = para,	\
				 .count    = n }

int mcecmd_send_command_simple(mce_context_t* context, int card_id, int para_id,
			       u32 cmd_code)
{
	mce_command cmd = QUICK_FILL(cmd_code, card_id, para_id, 1);
	cmd.data[0] = 1;
	cmd.checksum = mcecmd_cmd_checksum(&cmd);

	mce_reply rep;
	
	return mcecmd_send_command(context, &cmd, &rep);
}

int mcecmd_start_application(mce_context_t* context, const mce_param_t *param)
{
	int error = 0;
	int i;
	for (i=0; i < param->card.card_count && !error; i++) {
		error = mcecmd_send_command_simple(context,
						param->card.id[i],
						param->param.id,
						MCE_GO);
	}
	return error;
}

int mcecmd_stop_application(mce_context_t* context,  const mce_param_t *param)
{
	int error = 0;
	int i;
	for (i=0; i < param->card.card_count && !error; i++) {
		error = mcecmd_send_command_simple(context,
						param->card.id[i],
						param->param.id,
						MCE_ST);
	}
	return error;
}

int mcecmd_reset(mce_context_t* context,  const mce_param_t *param)
{
	int error = 0;
	int i;
	for (i=0; i < param->card.card_count && !error; i++) {
		error = mcecmd_send_command_simple(context,
						param->card.id[i],
						param->param.id,
						MCE_RS);
	}
	return error;
}


/* MCE special commands - these provide additional logical support */

int mcecmd_write_range(mce_context_t* context, const mce_param_t *param,
		       int data_index, const u32 *data, int count)
{
	int error = 0;
	u32 _block[MCE_CMD_DATA_MAX];
	u32* block = _block;
	int i;

	C_cmd_check;
	 
	if (count < 0)
		count = param->param.count - data_index;

	// Redirect Virtual cards, though virtual system will recurse here
	if (param->card.nature == MCE_NATURE_VIRTUAL)
		return mcecmd_write_virtual(context, param, data_index, data, count);
	
	// Separate writes for each target card.
	for (i=0; i<param->card.card_count; i++) {
		mce_reply rep;
		mce_command cmd;

		// Read any leading data in the block
		if (data_index != 0) {
			error = mcecmd_load_command(&cmd, MCE_RB, 
						    param->card.id[i], param->param.id,
						    data_index, 0, block);
			if (error) return error;

			error = mcecmd_send_command(context, &cmd, &rep);
			if (error) return error;

			memcpy(block, rep.data, data_index*sizeof(*block));
		}

		// Append our data
		memcpy(block+data_index, data, count*sizeof(*data));

		// Map the card, write the block
		error = mcecmd_load_command(
			&cmd, MCE_WB,
			param->card.id[i], param->param.id,
			count+data_index, count+data_index, block);
		if (error) return error;
		
		error = mcecmd_send_command(context, &cmd, &rep);
		if (error) return error;
	}
	return 0;
}

int mcecmd_read_range(mce_context_t* context, const mce_param_t *param,
		      int data_index, u32 *data, int count)
{
	int error = 0, i;

	C_cmd_check;

	if (count < 0)
		count = param->param.count;

	// Redirect Virtual cards, though virtual system will recurse here
	if (param->card.nature == MCE_NATURE_VIRTUAL)
		return mcecmd_read_virtual(context, param, data_index, data, count);
	
	for (i=0; i<param->card.card_count; i++) {
		mce_command cmd;
		mce_reply rep;
		error = mcecmd_load_command(&cmd, MCE_RB, 
					 param->card.id[i], param->param.id,
					 count+data_index, 0, NULL);
		if (error) return error;

		error = mcecmd_send_command(context, &cmd, &rep);
		if (error) return error;

		// I guess the data must be valid then.
		memcpy(data+i*count, rep.data+data_index, count*sizeof(u32));
	}
	
	return 0;
}

int mcecmd_write_element(mce_context_t* context, const mce_param_t *param,
			 int data_index, u32 datum)
{
	return mcecmd_write_range(context, param, data_index, &datum, 1);
}

int mcecmd_read_element(mce_context_t* context, const mce_param_t *param,
			int data_index, u32 *datum)
{
	return mcecmd_read_range(context, param, data_index, datum, 1);
}

int mcecmd_write_block_check(mce_context_t* context, const mce_param_t *param,
			     int count, const u32 *data, int retries)
{
	int i, error;
	u32 readback[MCE_CMD_DATA_MAX];
	int done = 0;

	if (param->card.card_count != 1)
		return -MCE_ERR_MULTICARD;

	do {
		error = mcecmd_write_block(context, param, count, data);
		
		if (error) return error;
		
		error = mcecmd_read_block(context, param, count, readback);
		if (error) return error;

		done = 1;
		for (i=0; i<count; i++) {
			if (data[i] != readback[i]) {
				done = 0;
				break;
			}
		}

	} while (!done && retries-- > 0);

	return (done ? 0 : -MCE_ERR_READBACK);
}


int mcecmd_interface_reset(mce_context_t* context)
{
	return ioctl(C_cmd.fd, MCEDEV_IOCT_INTERFACE_RESET);
}

int mcecmd_hardware_reset(mce_context_t* context)
{
	return ioctl(C_cmd.fd, MCEDEV_IOCT_HARDWARE_RESET);
}
