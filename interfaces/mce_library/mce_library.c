/***************************

  API for MCE

***************************/

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

#ifdef USE_XML
#  include "mce_xml.h"
#endif

#ifdef USE_CFG
#  include "mceconfig.h"
#endif

#define PATH_LENGTH 1024

struct mce_context {
	int opened;
	int fd;

/*   int fd_data; */
/*   int mode; */
/* #    MODE_NONE           0 */
/* #    MODE_CMD            1 */
/* #    MODE_DATA           2 */
/* #    MODE_NORMAL         3 */

	char dev_name     [PATH_LENGTH];
/*   char dev_name_mode[PATH_LENGTH]; */
	mce_data_t *mce_data;

	logger_t logger;

	char errstr[PATH_LENGTH];
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


int mce_param_init(param_properties_t *p)
{
	memset(p, 0, sizeof(*p));

	p->card_count = 1;
	p->count = 1;

	return 0;
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
	if (strlen(dev_name)>=PATH_LENGTH-1)
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

	mce_free_config(handle);

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
	if (checksum_data((u32*)rep, sizeof(*rep) / sizeof(u32)) != 0) {
		log_data(&CON.logger, (u32*)rep, 60, 2,
			 "reply [CHECKSUM error] ",
			 LOG_LEVEL_REP_ER);
		return -MCE_ERR_CHKSUM;
	}

	//Check that reply matches command, and command OK
	err = match_cmd_rep(cmd, rep);
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

u32 calc_checksum( const mce_command *cmd )
{
	u32 chksum = 0;
	u32 *p = (u32*) &cmd->command;
	while (p < (u32*) &cmd->checksum )
		chksum ^= *p++;
	return chksum;
}

u32 checksum_data( const u32 *data, int count )
{
	u32 chksum = 0;
	while (count>0)
		chksum ^= data[--count];	
	return chksum;
}


int match_cmd_rep( const mce_command *cmd, const mce_reply *rep )
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

	cmd.checksum = calc_checksum(&cmd);

	mce_reply rep;

	int err = mce_send_command(handle, &cmd, &rep);
/* 	if (err<0) return err; */

/* 	err = match_cmd_rep(&cmd, &rep); */
/* 	if (err<0) return err; */

/* 	return 0; */
	return err;
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

	cmd.checksum = calc_checksum(&cmd);

	mce_reply rep;

	int err = mce_send_command(handle, &cmd, &rep);
	if (err<0) return err;

/* 	err = match_cmd_rep(&cmd, &rep); */
/* 	if (err<0) return err; */

	// I guess the data must be valid then.
	memcpy(data, rep.data, n_data*sizeof(u32));
	
	return 0;
}




int mce_send_command_simple(int handle, int card_id, int para_id,
			    u32 cmd_code)
{
	mce_command cmd = QUICK_FILL(cmd_code, card_id, para_id, 1);
	cmd.data[0] = 1;
	cmd.checksum = calc_checksum(&cmd);

	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	mce_reply rep;

	int err = mce_send_command(handle, &cmd, &rep);
/* 	if (err<0) return err; */

/* 	err = match_cmd_rep(&cmd, &rep); */
/* 	if (err<0) return err; */

	return err;
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


/* Ioctl related */

int mce_ioctl(int fd, int key, unsigned long arg)
{
	return ioctl(fd, key, arg);
}

int mce_set_datasize(int fd, int datasize)
{
	return ioctl(fd, DATADEV_IOCT_SET_DATASIZE, datasize);
}

int mce_empty_data(int fd)
{
	return ioctl(fd, DATADEV_IOCT_EMPTY);
}

int mce_fake_stopframe(int fd)
{
	return ioctl(fd, DATADEV_IOCT_FAKE_STOPFRAME);
}


#ifdef USE_XML

/* XML functionality */

#define CHECK_XML(hndl)    if (cons[hndl].mce_data==NULL) \
                                   return -MCE_ERR_XML

int mce_free_config(int handle)
{
	CHECK_HANDLE(handle);

	if (CON.mce_data!=NULL) {
		free(CON.mce_data);
		CON.mce_data = NULL;
	}
	return 0;
}

int mce_load_config(int handle, char *filename)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	mce_free_config(handle);
	int err = mcexml_load(filename, &CON.mce_data, 0);

	return err;
}

int mce_lookup(int handle,
	       int *card_id, int *para_id, int *para_type, int *para_count,
	       char *card_str, char *para_str, int para_index)
{
	CHECK_HANDLE(handle);
	CHECK_XML(handle);

	return mcexml_lookup(CON.mce_data,
			     card_id, para_id, para_type, para_count,
			     card_str, para_str, para_index);

}

#endif /* USE_XML */

#ifdef USE_CFG

int mce_free_config(int handle)
{
	CHECK_HANDLE(handle);
	return mceconfig_destroy(CON.mce_data);
}

int mce_load_config(int handle, char *filename)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	return mceconfig_load(filename, &CON.mce_data);
}

int mce_lookup(int handle, param_properties_t *props,
			  char *card_str, char *para_str, int para_index)
{
	CHECK_HANDLE(handle);

	return mceconfig_lookup(props, CON.mce_data,
				card_str, para_str, para_index);

}

#endif /* USE_CFG */
