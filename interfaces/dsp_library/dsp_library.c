/***************************

  API for SDSU DSP for MCE

***************************/

#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <stdio.h>

#include <mcedsp.h>
#include <dsp_errors.h>
#include <dsp_ioctl.h>

#define PATH_LENGTH

struct dsp_context {
	int opened;
	int fd;
	char dev_name[PATH_LENGTH];
};

#define MAX_CONS 16

struct dsp_context cons[MAX_CONS];
int n_cons = 0;

inline int handle_in_bounds(int handle) {
	return (handle>=0) && (handle<MAX_CONS);
}

inline int mem_type_valid(dsp_memory_code mem) {
	return (mem==DSP_MEMX) || (mem==DSP_MEMY) || (mem==DSP_MEMP);
}

#define CHECK_HANDLE(hndl) if (!handle_in_bounds(hndl)) \
                                   return -DSP_ERR_HANDLE
#define CHECK_OPEN(hndl)   if (!cons[hndl].opened) \
                                   return -DSP_ERR_DEVICE
#define CHECK_MEM_TYPE(mt) if (!mem_type_valid(mt)) \
                                   return -DSP_ERR_MEMTYPE
#define CHECK_WORD(w)      if (w & ~0xffffff) \
                                   return -DSP_ERR_BOUNDS;

int count_users() {
	int i;
	int count=0;
	for (i=0; i<MAX_CONS; i++)
		if (cons[i].opened) count++;
	return count;
}

int dsp_open(char *dev_name)
{
	int handle, fd;

	for (handle=0; handle<MAX_CONS; handle++) {
		if (!cons[handle].opened)
			break;
	}  
	CHECK_HANDLE(handle);
	if (strlen(dev_name)>=PATH_LENGTH-1) return -DSP_ERR_BOUNDS;

	fd = open(dev_name, O_RDWR);
	if (fd<0) return -DSP_ERR_DEVICE;

	cons[handle].fd = fd;
	cons[handle].opened = 1;
	strcpy(cons[handle].dev_name, dev_name);

	return handle;
}


int dsp_close(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	if (close(cons[handle].fd)<0) return -DSP_ERR_DEVICE;
	cons[handle].opened = 0;
	cons[handle].fd = -1;

	return 0;
}


/* IOCTL wrappers */

int dsp_ioctl(int handle, unsigned int iocmd, unsigned long arg)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	return ioctl(cons[handle].fd, iocmd, arg);
}


int dsp_reset_flags(int handle)
{
	return dsp_ioctl(handle, DSPDEV_IOCT_RESET, 0);
}

int dsp_error(int handle)
{
	return dsp_ioctl(handle, DSPDEV_IOCT_ERROR, 0);
}

int dsp_speak(int handle, unsigned long arg)
{
	return dsp_ioctl(handle, DSPDEV_IOCT_SPEAK, arg);
}



/* COMMAND FUNCTIONALITY (wraps write, read) */

int dsp_send_command_now(int handle, dsp_command *cmd)
{
	if ( sizeof(*cmd) != write(cons[handle].fd, cmd, sizeof(*cmd)) )
		return ioctl(cons[handle].fd, DSPDEV_IOCT_ERROR);

	dsp_message msg;
  
	if ( sizeof(msg) != read(cons[handle].fd, &msg, sizeof(msg)) )
		return ioctl(cons[handle].fd, DSPDEV_IOCT_ERROR);

	if ( msg.type != DSP_REP ) return -DSP_ERR_UNKNOWN;
	if ( msg.command != cmd->command ) return -DSP_ERR_REPLY;
	if ( msg.reply != DSP_ACK ) return -DSP_ERR_FAILURE;
    
	return (int) (msg.data & DSP_DATAMASK);
}


int dsp_send_command(int handle, dsp_command *cmd)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);

	return dsp_send_command_now(handle, cmd);
}

int dsp_version(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
  
	dsp_command cmd = {DSP_VER, {0, 0, 0 }};
	return dsp_send_command_now(handle, &cmd);
}



int dsp_read_word(int handle, dsp_memory_code mem, int address)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
	CHECK_MEM_TYPE(mem);
  
	dsp_command cmd = {DSP_RDM, {mem, address, 0 }};
	return dsp_send_command_now(handle, &cmd);
}

int dsp_read_word_X(int handle, int address)
{
	return dsp_read_word(handle, DSP_MEMX, address);
}

int dsp_read_word_Y(int handle, int address)
{
	return dsp_read_word(handle, DSP_MEMY, address);
}

int dsp_read_word_P(int handle, int address)
{
	return dsp_read_word(handle, DSP_MEMP, address);
}



int dsp_write_word(int handle, dsp_memory_code mem, int address, u32 value)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
	CHECK_MEM_TYPE(mem);
	CHECK_WORD(value);
  
	dsp_command cmd = {DSP_WRM, {mem, address, value}};
	return dsp_send_command_now(handle, &cmd);
}

int dsp_write_word_X(int handle, int address, u32 value)
{
	return dsp_write_word(handle, DSP_MEMX, address, value);
}

int dsp_write_word_Y(int handle, int address, u32 value)
{
	return dsp_write_word(handle, DSP_MEMY, address, value);
}

int dsp_write_word_P(int handle, int address, u32 value)
{
	return dsp_write_word(handle, DSP_MEMP, address, value);
}


int dsp_reset(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
  
	dsp_command cmd = {
		command : DSP_RST,
	};

	return dsp_send_command_now(handle, &cmd);
}


int dsp_clear(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
  
	return dsp_ioctl(handle, DSPDEV_IOCT_CLEAR, 0);
}


int dsp_start_application(int handle, int data)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
	CHECK_WORD(data);
  
	dsp_command cmd = {DSP_GOA, {data,0,0} };
	return dsp_send_command_now(handle, &cmd);
}

int dsp_stop_application(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
  
	dsp_command cmd = {
		command : DSP_STP,
	};
	return dsp_send_command_now(handle, &cmd);
}

int dsp_reset_mce(int handle)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
	
	dsp_command cmd = {
		command: DSP_RCO,
	};
	return dsp_send_command_now(handle, &cmd);
}

int dsp_qt_set(int handle, int var, int arg1, int arg2)
{
	CHECK_HANDLE(handle);
	CHECK_OPEN(handle);
	
	dsp_command cmd = {
		command: DSP_QTS,
		args: {var, arg1, arg2},
	};
	return dsp_send_command_now(handle, &cmd);
}

int dsp_atomem(char *mem_type) {
	if (strlen(mem_type) != 1) return -DSP_ERR_MEMTYPE;

	switch (toupper(mem_type[0])) {

	case 'X':
		return DSP_MEMX;

	case 'Y':
		return DSP_MEMY;

	case 'P':
		return DSP_MEMP;
	}

	return -DSP_ERR_MEMTYPE;
}

char *dsp_memtoa(int mem) {

	switch (mem) {

	case DSP_MEMX:
		return "X";

	case DSP_MEMY:
		return "Y";

	case DSP_MEMP:
		return "P";

	}

	return "?";
}
