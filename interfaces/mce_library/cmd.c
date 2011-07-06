/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
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
#include <mce/defaults.h>

#include "context.h"
#include "eth.h"
#include "net.h"
#include "sdsu.h"
#include "virtual.h"

#define LOG_LEVEL_CMD     MASLOG_DETAIL
#define LOG_LEVEL_REP_OK  MASLOG_DETAIL
#define LOG_LEVEL_REP_ER  MASLOG_INFO

static int log_data(maslog_t maslog, u32 *buffer, int count, int min_raw,
        char *msg, int level)
{
	char out[2048];
	char *s = out + sprintf(out, "%s", msg);

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

    return maslog_print_level(maslog, out, level);
}

int mcecmd_open(mce_context_t context)
{
    int err;
    if (C_cmd.connected)
        mcecmd_close(context);

    if (context->maslog == NULL) {
        char *ptr = mcelib_shell_expand("mcelib[${MAS_MCE_DEV}]",
                context->dev_index);
        context->maslog = maslog_connect(context, ptr);
        free(ptr);
    }

    /* set I/O handlers */
    switch(context->dev_route) {
        case sdsu:
            SET_IO_METHODS(context, cmd, sdsu);
            break;
        case eth:
            SET_IO_METHODS(context, cmd, eth);
            break;
        case net:
            SET_IO_METHODS(context, cmd, net);
            break;
        default:
            fprintf(stderr, "mcecmd: Cannot attach CMD: Unhandled route.\n");
            return -MCE_ERR_ATTACH;
    }
    if ((err = C_cmd.connect(context)))
        return err;

	// Set up connection to prevent outstanding replies after release
    C_cmd.ioctl(context, MCEDEV_IOCT_SET,
            C_cmd.ioctl(context, MCEDEV_IOCT_GET, 0) | MCEDEV_CLOSE_CLEANLY);

    // Most applications using this library will want to read their own replies.
	mcecmd_lock_replies(context, 1);

	C_cmd.connected = 1;

	return 0;
}

int mcecmd_close(mce_context_t context)
{
    int err = 0;
	C_cmd_check;

    if ((err = C_cmd.disconnect(context)))
        return err;

	C_cmd.connected = 0;

	return 0;
}


int mcecmd_lock_replies(mce_context_t context, int lock)
{
    int flags = C_cmd.ioctl(context, MCEDEV_IOCT_GET, 0);
	int err;
	if (lock) {
		// Set up connection to prevent outstanding replies after release
        err = C_cmd.ioctl(context, MCEDEV_IOCT_SET,
                flags | MCEDEV_CLOSED_CHANNEL);
	} else {
		// Don't do that.
        err = C_cmd.ioctl(context, MCEDEV_IOCT_SET,
                flags & ~MCEDEV_CLOSED_CHANNEL);
    }
	return err;
}

/* Basic device write/read routines */
int mcecmd_send_command_now(mce_context_t context, mce_command *cmd)
{
    return C_cmd.write(context, cmd, sizeof(*cmd));
}

int mcecmd_read_reply_now(mce_context_t context, mce_reply *rep)
{
    return C_cmd.read(context, rep, sizeof(*rep));
}

#define MAX_SEND_ATTEMPTS 5
int mcecmd_send_command(mce_context_t context, mce_command *cmd, mce_reply *rep)
{
	int err = 0;
	int attempts = 0;
	char errstr[MCE_LONG];
	C_cmd_check;

    log_data(C_maslog, (u32*)cmd + 2, 62, 2, "command", LOG_LEVEL_CMD);

	/* Loop the attempts to protect against very rare partial
	   command packet transfers.  Reply packet will have zeros for
	   card address and thus fail the consistency check. */

	do {
		err = mcecmd_send_command_now(context, cmd);
		if (err<0) {
			sprintf(errstr, "command not sent, error %#x.", -err);
            maslog_print_level(C_maslog, errstr, MASLOG_INFO);
			memset(rep, 0, sizeof(*rep));
			return err;
		}

		err = mcecmd_read_reply_now(context, rep);
		if (err != 0) {
			sprintf(errstr, "reply [communication error] %s",
				mcelib_error_string(err));
            maslog_print_level(C_maslog, errstr, LOG_LEVEL_REP_ER);
			return err;
		}

		// Analysis of received packet
		err = mcecmd_checksum((u32*)rep, sizeof(*rep) / sizeof(u32)) ?
			-MCE_ERR_CHKSUM : 0;

		if (!err) err = mcecmd_cmd_match_rep(cmd, rep);

	} while (attempts++ < MAX_SEND_ATTEMPTS && err == -MCE_ERR_REPLY);

	switch (-err) {

	case MCE_ERR_CHKSUM:
        log_data(C_maslog, (u32*)rep, 60, 2,
			 "reply [checksum error] ",
			 LOG_LEVEL_REP_ER);
		break;

	case MCE_ERR_FAILURE:
        log_data(C_maslog, (u32*)rep, 60, 2,
			 "reply [command failed] ",
			 LOG_LEVEL_REP_ER);
		break;

	case MCE_ERR_REPLY:
        log_data(C_maslog, (u32*)rep, 60, 2,
			 "reply [consistency error] ",
			 LOG_LEVEL_REP_ER);
		break;

	case 0:
        log_data(C_maslog, (u32*)rep, 60, 2, "reply  ",
			 LOG_LEVEL_REP_OK);
		break;

	default:
		sprintf(errstr, "reply [strange error '%s'] ",
			mcelib_error_string(err));
        log_data(C_maslog, (u32*)rep, 60, 2,
			 errstr, LOG_LEVEL_REP_ER);
	}

	return err;
}

int mcecmd_load_param(mce_context_t context, mce_param_t *param,
		      const char *card_str, const char *param_str)
{
	return mceconfig_lookup(context, card_str, param_str,
				&param->card, &param->param);
}


int mcecmd_write_block(mce_context_t context, const mce_param_t *param,
		       int count, const u32 *data)
{
	// This used to run the show, but these days we pass it to write_range
	return mcecmd_write_range(context, param, 0, data, count);
}

int mcecmd_read_block(mce_context_t context, const mce_param_t *param,
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

int mcecmd_send_command_simple(mce_context_t context, int card_id, int para_id,
			       u32 cmd_code)
{
	mce_command cmd = QUICK_FILL(cmd_code, card_id, para_id, 1);
	cmd.data[0] = 1;
	cmd.checksum = mcecmd_cmd_checksum(&cmd);

	mce_reply rep;

	return mcecmd_send_command(context, &cmd, &rep);
}

int mcecmd_start_application(mce_context_t context, const mce_param_t *param)
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

int mcecmd_stop_application(mce_context_t context,  const mce_param_t *param)
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

int mcecmd_reset(mce_context_t context,  const mce_param_t *param)
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


int mcecmd_read_size(const mce_param_t *p, int count)
{
	if (count < 0)
		count = p->param.count;
	return p->card.card_count * count;
}


/* MCE special commands - these provide additional logical support */

int mcecmd_write_range(mce_context_t context, const mce_param_t *param,
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

int mcecmd_read_range(mce_context_t context, const mce_param_t *param,
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

int mcecmd_write_element(mce_context_t context, const mce_param_t *param,
			 int data_index, u32 datum)
{
	return mcecmd_write_range(context, param, data_index, &datum, 1);
}

int mcecmd_read_element(mce_context_t context, const mce_param_t *param,
			int data_index, u32 *datum)
{
	return mcecmd_read_range(context, param, data_index, datum, 1);
}

int mcecmd_write_block_check(mce_context_t context, const mce_param_t *param,
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

int mcecmd_ioctl(mce_context_t context, unsigned long int req, int arg)
{
    return C_cmd.ioctl(context, req, arg);
}

int mcecmd_interface_reset(mce_context_t context)
{
    return C_cmd.ioctl(context, MCEDEV_IOCT_INTERFACE_RESET, 0);
}

int mcecmd_hardware_reset(mce_context_t context)
{
    return C_cmd.ioctl(context, MCEDEV_IOCT_HARDWARE_RESET, 0);
}
