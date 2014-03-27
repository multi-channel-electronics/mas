/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/***************************

  API for SDSU DSP for MCE

***************************/
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <stdio.h>

#include "context.h"
#include <mcedsp.h>
#include <mce/dsp_errors.h>
#include <mce/new_dspioctl.h>
#include <mce/defaults.h>

#define CHECK_OPEN(cntx)   if (!cntx->dsp.opened) \
                                   return -DSP_ERR_DEVICE
#define CHECK_WORD(w)      if (w & ~0xffffff) \
                                   return -DSP_ERR_BOUNDS;

int mcedsp_open(mce_context_t *context)
{
    int fd;
    char dev_name[20];
    sprintf(dev_name, "/dev/mce_dev%u", (unsigned)context->fibre_card);

    fd = open(dev_name, O_RDWR);
    if (fd < 0)
        return -DSP_ERR_DEVICE;

    context->dsp.fd = fd;
    context->dsp.opened = 1;

    return 0;
}


int mcedsp_close(mce_context_t *context)
{
    CHECK_OPEN(context);

    if (close(context->dsp.fd) < 0)
        return -DSP_ERR_DEVICE;

    context->dsp.opened = 0;
    context->dsp.fd = -1;

    return 0;
}


/* IOCTL wrappers */

int mcedsp_ioctl(mce_context_t *context, unsigned int iocmd, unsigned long arg)
{
    CHECK_OPEN(context);
    return ioctl(context->dsp.fd, iocmd, arg);
}


int mcedsp_reset_flags(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPIOCT_RESET_SOFT, 0);
}

/*
int mcedsp_error(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_ERROR, 0);
}

int mcedsp_speak(mce_context_t *context, unsigned long arg)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_SPEAK, arg);
}
*/

/* COMMAND FUNCTIONALITY (wraps write, read) */

/*
int mcedsp_send_command_now(mce_context_t *context, dsp_command *cmd)
{
    if (sizeof(*cmd) != write(context->dsp.fd, cmd, sizeof(*cmd)))
        return ioctl(context->dsp.fd, DSPDEV_IOCT_ERROR);

    dsp_message msg;
  
    if (sizeof(msg) != read(context->dsp.fd, &msg, sizeof(msg)))
        return ioctl(context->dsp.fd, DSPDEV_IOCT_ERROR);

    if (msg.type != DSP_REP)
        return -DSP_ERR_UNKNOWN;
    if (msg.command != cmd->command)
        return -DSP_ERR_REPLY;
    if (msg.reply != DSP_ACK)
        return -DSP_ERR_FAILURE;
    
    return (int)(msg.data & DSP_DATAMASK);
}

int mcedsp_send_command(mce_context_t *context, dsp_command *cmd)
{
    CHECK_OPEN(context);

    return mcedsp_send_command_now(context, cmd);
}
*/

int mcedsp_send_command(mce_context_t *context,
                        struct dsp_command *cmd,
                        struct dsp_datagram *gram)
{
    CHECK_OPEN(context);

    int err = 0;
    if (cmd->size < 0)
        cmd->size = cmd->data_size + 1;

    err = mcedsp_ioctl(context, DSPIOCT_COMMAND, (unsigned long)cmd);
    if (err < 0) goto exit_err;

    err = mcedsp_ioctl(context, DSPIOCT_GET_DSP_REPLY, (unsigned long)gram);
    if (err < 0) goto exit_err;
    
    return 0;

exit_err:
    switch(errno) {
    case ENODATA:
        return -DSP_ERR_TIMEOUT;
    case EINTR:
        return -DSP_ERR_INTERRUPTED;
    case EIO:
        return -DSP_ERR_IO;
    case EAGAIN:
        return -DSP_ERR_WOULDBLOCK;
    }
    return -DSP_ERR_UNKNOWN;
}

int mcedsp_version(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPIOCT_GET_VERSION, 0);
}


int mcedsp_read_word(mce_context_t *context, dsp_memory_code mem, int address)
{
    struct dsp_command cmd;
    struct dsp_datagram gram;

    cmd.flags = DSP_EXPECT_DSP_REPLY;
    cmd.size = -1;
    cmd.data_size = 1;
    cmd.data[0] = address;

    switch(mem) {
    case DSP_MEMP:
        cmd.cmd = DSP_CMD_READ_P;
        break;
    case DSP_MEMX:
        cmd.cmd = DSP_CMD_READ_X;
        break;
    case DSP_MEMY:
        cmd.cmd = DSP_CMD_READ_Y;
        break;
    default:
        return -DSP_ERR_MEMTYPE;
    }
    
    int err = mcedsp_send_command(context, &cmd, &gram);
    if (err < 0)
        return err;

    return DSP_REPLY(&gram)->data[0];
}

int mcedsp_read_word_X(mce_context_t *context, int address)
{
    return mcedsp_read_word(context, DSP_MEMX, address);
}

int mcedsp_read_word_Y(mce_context_t *context, int address)
{
    return mcedsp_read_word(context, DSP_MEMY, address);
}

int mcedsp_read_word_P(mce_context_t *context, int address)
{
    return mcedsp_read_word(context, DSP_MEMP, address);
}



int mcedsp_write_word(mce_context_t *context, dsp_memory_code mem, int address,
        uint32_t value)
{
    struct dsp_command cmd;
    struct dsp_datagram gram;

    CHECK_OPEN(context);
    CHECK_WORD(value);
  
    cmd.flags = DSP_EXPECT_DSP_REPLY;
    cmd.size = -1;
    cmd.data_size = 2;
    cmd.data[0] = address;
    cmd.data[1] = value;

    switch(mem) {
    case DSP_MEMP:
        cmd.cmd = DSP_CMD_WRITE_P;
        break;
    case DSP_MEMX:
        cmd.cmd = DSP_CMD_WRITE_X;
        break;
    case DSP_MEMY:
        cmd.cmd = DSP_CMD_WRITE_Y;
        break;
    default:
        return -DSP_ERR_MEMTYPE;
    }
    
    return mcedsp_send_command(context, &cmd, &gram);
}

int mcedsp_write_word_X(mce_context_t *context, int address, uint32_t value)
{
    return mcedsp_write_word(context, DSP_MEMX, address, value);
}

int mcedsp_write_word_Y(mce_context_t *context, int address, uint32_t value)
{
    return mcedsp_write_word(context, DSP_MEMY, address, value);
}

int mcedsp_write_word_P(mce_context_t *context, int address, uint32_t value)
{
    return mcedsp_write_word(context, DSP_MEMP, address, value);
}

int mcedsp_reset(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPIOCT_RESET_DSP, 0);
}
/*
int mcedsp_start_application(mce_context_t *context, int data)
{
    CHECK_OPEN(context);
    CHECK_WORD(data);
  
    dsp_command cmd = {DSP_GOA, {data,0,0} };
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_stop_application(mce_context_t *context)
{
    CHECK_OPEN(context);
  
    dsp_command cmd = {
        .command = DSP_STP,
    };
    return mcedsp_send_command_now(context, &cmd);
}
*/
int mcedsp_reset_mce(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPIOCT_RESET_MCE, 0);
}

/*
int mcedsp_qt_set(mce_context_t *context, int var, int arg1, int arg2)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
        .command = DSP_QTS,
        .args = {var, arg1, arg2},
    };
    return mcedsp_send_command_now(context, &cmd);
}
*/

int mcedsp_atomem(char *mem_type) {
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

char *mcedsp_memtoa(int mem) {

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
