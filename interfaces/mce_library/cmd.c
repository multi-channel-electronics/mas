/***************************

  API for MCE

***************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <libmaslog.h>

#include "data_ioctl.h"
#include "mcecmd.h"

#define LOG_LEVEL_CMD     LOGGER_DETAIL
#define LOG_LEVEL_REP_OK  LOGGER_DETAIL
#define LOG_LEVEL_REP_ER  LOGGER_INFO


struct mce_context {
	int opened;
	int fd;

	char dev_name[MCE_LONG];

	logger_t logger;

	char errstr[MCE_LONG];
};

#define MAX_CONS 16

struct mce_context cons[MAX_CONS];
int n_cons = 0;

inline int handle_in_bounds(int handle) {
	return (handle>=0) && (handle<MAX_CONS);
}

#define CON cons[handle]

#define CHECK_HANDLE(hndl) if (!handle_in_bounds(hndl)) \
                                   return -MCE_ERR_HANDLE
#define CHECK_OPEN(hndl)   if (!cons[hndl].opened) \
                                   return -MCE_ERR_HANDLE

char *mce_error_string(int error)
{
	switch (-error) {

	case MCE_ERR_FAILURE:
		return "MCE could not complete command.";

	case MCE_ERR_HANDLE:
		return "Bad mce_library handle.";

	case MCE_ERR_DEVICE:
		return "I/O error on driver device file.";

	case MCE_ERR_FORMAT:
		return "Bad packet structure.";

	case MCE_ERR_REPLY:
		return "Reply does not match command.";

	case MCE_ERR_BOUNDS:
		return "Data size is out of bounds.";

	case MCE_ERR_CHKSUM:
		return "Checksum failure.";

	case MCE_ERR_XML:
		return "Configuration file not loaded.";

	case 0:
		return "Success.";
	}

	return "Unknown mce_library error.";
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


int mce_open(char *dev_name)
{
	int handle, fd;

	for (handle=0; handle<MAX_CONS; handle++) {
		if (!CON.opened)
			break;
	}
	CHECK_HANDLE(handle);
	if (strlen(dev_name)>=MCE_LONG-1)
		return -MCE_ERR_BOUNDS;

	fd = open(dev_name, O_RDWR);
	if (fd<0) return -MCE_ERR_DEVICE;

	CON.fd = fd;
	CON.opened = 1;
	strcpy(CON.dev_name, dev_name);

	// No error if logger_connect fails
	logger_connect(&CON.logger, NULL, "lib_mce");

	return handle;
}


int mce_close(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	logger_close(&CON.logger);

	if (close(CON.fd)<0) return -MCE_ERR_DEVICE;
	CON.opened = 0;
	CON.fd = -1;

	return 0;
}


int mce_send_command_now(int handle, mce_command *cmd)
{	
	if ( sizeof(*cmd) != write(CON.fd, cmd, sizeof(*cmd)) )
		return -MCE_ERR_DEVICE;

	return 0;
}

int mce_read_reply_now(int handle, mce_reply *rep)
{
	if ( sizeof(*rep) != read(CON.fd, rep, sizeof(*rep)) )
		return -MCE_ERR_DEVICE;

	return 0;
}


int mce_send_command(int handle, mce_command *cmd, mce_reply *rep)
{
	int err = 0;
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	err = mce_send_command_now(handle, cmd);
	if (err<0) {
		memset(rep, 0, sizeof(*rep));
		return err;
	}

	log_data(&CON.logger, (u32*)cmd + 2, 62, 2, "command",
		 LOG_LEVEL_CMD);
	
	err = mce_read_reply_now(handle, rep);
	if (err != 0) {
		//Log message "reply! read error."
		logger_print_level(&CON.logger, "reply! read error.",
				   LOG_LEVEL_REP_ER);
		return err;
	}
	
	//Check for data integrity
	if (mce_checksum((u32*)rep, sizeof(*rep) / sizeof(u32)) != 0) {
		log_data(&CON.logger, (u32*)rep, 60, 2,
			 "reply [CHECKSUM error] ",
			 LOG_LEVEL_REP_ER);
		return -MCE_ERR_CHKSUM;
	}

	//Check that reply matches command, and command OK
	err = mce_cmd_match_rep(cmd, rep);
	if (err == -MCE_ERR_FAILURE) {
		log_data(&CON.logger, (u32*)rep, 60, 2,
			 "reply [command failed] ",
			 LOG_LEVEL_REP_ER);
		return -MCE_ERR_FAILURE;
	} else if (err != 0)  {
		log_data(&CON.logger, (u32*)rep, 60, 2,
			 "reply [CONSISTENCY error] ",
			 LOG_LEVEL_REP_ER);
		return -MCE_ERR_REPLY;
	}

	log_data(&CON.logger, (u32*)rep, 60, 2, "reply  ",
		 LOG_LEVEL_REP_OK);

	return 0;
}

/* Specialty */

#define QUICK_FILL(cmd, card, para, n) { \
        .preamble = {PREAMBLE_0, PREAMBLE_1},\
        .command  = cmd,\
        .card_id  = card,\
        .para_id  = para,\
        .count    = n }

u32 mce_cmd_checksum( const mce_command *cmd )
{
	u32 chksum = 0;
	u32 *p = (u32*) &cmd->command;
	while (p < (u32*) &cmd->checksum )
		chksum ^= *p++;
	return chksum;
}

u32 mce_checksum( const u32 *data, int count )
{
	u32 chksum = 0;
	while (count>0)
		chksum ^= data[--count];	
	return chksum;
}


int mce_cmd_match_rep( const mce_command *cmd, const mce_reply *rep )
{
	// Check sanity of response
	if ( (rep->command | 0x20200000) != cmd->command)
		return -MCE_ERR_REPLY;
	if (rep->para_id != cmd->para_id)
		return -MCE_ERR_REPLY;
	if (rep->card_id != cmd->card_id)
		return -MCE_ERR_REPLY;

	// Check success of command
	switch (rep->ok_er) {
	case MCE_OK:
		break;
	case MCE_ER:
		// Error code should be extracted; checksum; ...
		return -MCE_ERR_FAILURE;
	default:
		return -MCE_ERR_REPLY;
	}
	return 0;
}

int mce_write_block(int handle, int card_id, int para_id,
		    int n_data, const u32 *data)
{
	mce_command cmd = QUICK_FILL(MCE_WB, card_id, para_id, n_data);
		
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	if (n_data < 0 || n_data >= MCE_CMD_DATA_MAX)
		return -MCE_ERR_BOUNDS;

	int i;
	for (i=0; i<n_data; i++)
		cmd.data[i] = data[i];

	for (i=n_data; i<MCE_CMD_DATA_MAX; i++)
		cmd.data[i] = 0;

	cmd.checksum = mce_cmd_checksum(&cmd);

	mce_reply rep;

	return mce_send_command(handle, &cmd, &rep);
}

int mce_read_block(int handle, int card_id, int para_id,
		   int n_data, u32 *data, int n_cards)
{
	mce_command cmd = QUICK_FILL(MCE_RB, card_id, para_id, n_data);

	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	// Correct n_data to account n_cards
	if (n_cards <=0) n_cards = 1;
	n_data *= n_cards;

	if (n_data < 0 || n_data >= MCE_CMD_DATA_MAX)
		return -MCE_ERR_BOUNDS;

	memset(&cmd.data, 0, MCE_CMD_DATA_MAX * sizeof(u32));

	cmd.checksum = mce_cmd_checksum(&cmd);

	mce_reply rep;

	int err = mce_send_command(handle, &cmd, &rep);
	if (err<0) return err;

	// I guess the data must be valid then.
	memcpy(data, rep.data, n_data*sizeof(u32));
	
	return 0;
}




int mce_send_command_simple(int handle, int card_id, int para_id,
			    u32 cmd_code)
{
	mce_command cmd = QUICK_FILL(cmd_code, card_id, para_id, 1);
	cmd.data[0] = 1;
	cmd.checksum = mce_cmd_checksum(&cmd);

	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	mce_reply rep;

	return mce_send_command(handle, &cmd, &rep);
}

int mce_start_application(int handle, int card_id, int para_id)
{
	return mce_send_command_simple(handle, card_id, para_id, MCE_GO);
}

int mce_stop_application(int handle, int card_id, int para_id)
{
	return mce_send_command_simple(handle, card_id, para_id, MCE_ST);
}

int mce_reset(int handle, int card_id, int para_id)
{
	return mce_send_command_simple(handle, card_id, para_id, MCE_RS);
}


/* MCE special commands - these provide additional logical support */

int mce_write_element(int handle, int card_id, int para_id,
		      int data_index, u32 datum)
{
	int error = 0;
	u32 data[MCE_CMD_DATA_MAX];
	if ( (error = mce_read_block(handle, card_id, para_id, data_index+1, data, 1)) != 0)
		return error;

	data[data_index] = datum;

	return mce_write_block(handle, card_id, para_id, data_index+1, data);
}

int mce_read_element(int handle, int card_id, int para_id,
		     int data_index, u32 *datum)
{
	int error = 0;
	u32 data[MCE_CMD_DATA_MAX];
	if ( (error = mce_read_block(handle, card_id, para_id, data_index+1, data, 1)) != 0)
		return error;
	*datum = data[data_index];
	return 0;
}

int mce_write_block_check(int handle, int card_id, int para_id,
			  int n_data, const u32 *data, int retries)
{
	int i, error;
	u32 readback[MCE_CMD_DATA_MAX];
	int done = 0;

	do {
		error = mce_write_block(handle, card_id, para_id,
					n_data, data);
		
		if (error) return error;
		
		error = mce_read_block(handle, card_id, para_id,
				       n_data, readback, 1);
		if (error) return error;

		done = 1;
		for (i=0; i<n_data; i++) {
			if (data[i] != readback[i]) {
				done = 0;
				break;
			}
		}

	} while (!done && retries-- > 0);

	return (done ? 0 : -MCE_ERR_READBACK);
}
