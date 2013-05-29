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
#include <mce/dsp_ioctl.h>
#include <mce/defaults.h>

static inline int mem_type_valid(dsp_memory_code mem) {
    return (mem==DSP_MEMX) || (mem==DSP_MEMY) || (mem==DSP_MEMP);
}

#define CHECK_OPEN(cntx)   if (!cntx->dsp.opened) \
                                   return -DSP_ERR_DEVICE
#define CHECK_MEM_TYPE(mt) if (!mem_type_valid(mt)) \
                                   return -DSP_ERR_MEMTYPE
#define CHECK_WORD(w)      if (w & ~0xffffff) \
                                   return -DSP_ERR_BOUNDS;

int mcedsp_open(mce_context_t *context)
{
    int fd;
    char dev_name[20];
    sprintf(dev_name, "/dev/mce_dsp%u", (unsigned)context->fibre_card);

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
    return ioctl(context->dsp.fd, iocmd, arg);
}


int mcedsp_reset_flags(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_RESET, 0);
}

int mcedsp_error(mce_context_t *context)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_ERROR, 0);
}

int mcedsp_speak(mce_context_t *context, unsigned long arg)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_SPEAK, arg);
}


/* COMMAND FUNCTIONALITY (wraps write, read) */

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

int mcedsp_version(mce_context_t *context)
{
    CHECK_OPEN(context);
  
    dsp_command cmd = {DSP_VER, {0, 0, 0 }};
    return mcedsp_send_command_now(context, &cmd);
}



int mcedsp_read_word(mce_context_t *context, dsp_memory_code mem, int address)
{
    CHECK_OPEN(context);
    CHECK_MEM_TYPE(mem);
  
    dsp_command cmd = {DSP_RDM, {mem, address, 0 }};
    return mcedsp_send_command_now(context, &cmd);
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
    CHECK_OPEN(context);
    CHECK_MEM_TYPE(mem);
    CHECK_WORD(value);
  
    dsp_command cmd = {DSP_WRM, {mem, address, value}};
    return mcedsp_send_command_now(context, &cmd);
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
    CHECK_OPEN(context);
  
    dsp_command cmd = {
        .command = DSP_RST,
    };

    return mcedsp_send_command_now(context, &cmd);
}

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

int mcedsp_reset_mce(mce_context_t *context)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
        .command = DSP_RCO,
    };
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_qt_set(mce_context_t *context, int var, int arg1, int arg2)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
        .command = DSP_QTS,
        .args = {var, arg1, arg2},
    };
    return mcedsp_send_command_now(context, &cmd);
}

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
