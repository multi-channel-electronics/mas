/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/***************************

  API for SDSU DSP for MCE

***************************/
#include "mce_library.h"
#ifndef NO_MCE_OPS

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <stdio.h>

#include "context.h"
#include "mcedsp.h"

#include <mce/dsp.h>
#include <mce/dsp_errors.h>
#include <mce/ioctl.h>
#include <mce/defaults.h>

#define CHECK_OPEN(cntx)   if (!cntx->dsp.opened) \
                                   return -DSP_ERR_DEVICE
#define CHECK_WORD(w)      if (w & ~0xffffff) \
                                   return -DSP_ERR_BOUNDS;

int mcedsp_open(mce_context_t *context)
{
    int err = 0;
    if (!context->dsp.opened)
        err = mcedev_open(context, MCE_SUBSYSTEM_DSP);

    return err;
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

int mcedsp_driver_type(mce_context_t *context)
{
    /* this IOCTL request is supported by all drivers */
    return mcedsp_ioctl(context, DSPIOCT_GET_DRV_TYPE, 0);
}

int mcedsp_reset_flags(mce_context_t *context)
{
    if (mcelib_legacy(context))
        return mcedsp_ioctl(context, DSPDEV_IOCT_RESET, 0);
    errno = EINVAL;
    return -1;
}

int mcedsp_speak(mce_context_t *context, unsigned long arg)
{
    if (mcelib_legacy(context))
        return mcedsp_ioctl(context, DSPDEV_IOCT_SPEAK, arg);
    errno = EINVAL;
    return -1;
}


/* COMMAND FUNCTIONALITY (wraps write, read)
 *
 * For U0106- (c6), commands are written to the mce_dsp device file,
 * and then a reply as read from the same file.  The reply is
 * immediately decoded to return the interesting data. For U0107+
 * (c7), commands are loaded via ioctl, and the reply is read back
 * through ioctl.  The reply data must then be extracted from the
 * returne data structure.  */

int mcedsp_send_command_c6(mce_context_t *context, dsp_command *cmd)
{
    CHECK_OPEN(context);

    if (!mcelib_legacy(context))
        return -DSP_ERR_WRONG_VERSION;

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

int mcedsp_send_command_c7(mce_context_t *context,
                           struct dsp_command *cmd,
                           struct dsp_datagram *gram)
{
    CHECK_OPEN(context);

    if (mcelib_legacy(context))
        return -DSP_ERR_WRONG_VERSION;

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
    if (mcelib_legacy(context)) {
        CHECK_OPEN(context);
        dsp_command cmd = {DSP_VER, {0, 0, 0 }};
        return mcedsp_send_command_c6(context, &cmd);
    }
    return mcedsp_ioctl(context, DSPIOCT_GET_VERSION, 0);
}



int mcedsp_read_word(mce_context_t *context, dsp_memory_code mem, int address)
{
    if (mcelib_legacy(context)) {
        /* U0106- */
        CHECK_OPEN(context);
        uint32_t mem_code = 0;
        switch(mem) {
        case DSP_MEMX: mem_code = DSP_C6_MEMX; break;
        case DSP_MEMY: mem_code = DSP_C6_MEMY; break;
        case DSP_MEMP: mem_code = DSP_C6_MEMP; break;
        default:
            return -DSP_ERR_MEMTYPE;
        }
        
        dsp_command cmd = {DSP_RDM, {mem_code, address, 0 }};
        return mcedsp_send_command_c6(context, &cmd);

    } else {
        /* U0107+ */
        struct dsp_command cmd;
        struct dsp_datagram gram;

        cmd.flags = DSP_EXPECT_DSP_REPLY;
        cmd.size = -1;
        cmd.data_size = 1;
        cmd.data[0] = address;

        switch(mem) {
        case DSP_MEMX: cmd.cmd = DSP_CMD_READ_X; break;
        case DSP_MEMY: cmd.cmd = DSP_CMD_READ_Y; break;
        case DSP_MEMP: cmd.cmd = DSP_CMD_READ_P; break;
        default:
            return -DSP_ERR_MEMTYPE;
        }
    
        int err = mcedsp_send_command_c7(context, &cmd, &gram);
        if (err < 0)
            return err;

        return DSP_REPLY(&gram)->data[0];
    }
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
    if (mcelib_legacy(context)) {
        /* U0106- */
        CHECK_OPEN(context);
        CHECK_WORD(value);
        
        uint32_t mem_code = 0;
        switch(mem) {
        case DSP_MEMX: mem_code = DSP_C6_MEMX; break;
        case DSP_MEMY: mem_code = DSP_C6_MEMY; break;
        case DSP_MEMP: mem_code = DSP_C6_MEMP; break;
        default:
            return -DSP_ERR_MEMTYPE;
        }
  
        dsp_command cmd = {DSP_WRM, {mem_code, address, value}};
        return mcedsp_send_command_c6(context, &cmd);
    } else {
        /* U0107+ */
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
        case DSP_MEMX: cmd.cmd = DSP_CMD_WRITE_X; break;
        case DSP_MEMY: cmd.cmd = DSP_CMD_WRITE_Y; break;
        case DSP_MEMP: cmd.cmd = DSP_CMD_WRITE_P; break;
        default:
            return -DSP_ERR_MEMTYPE;
        }
    
        return mcedsp_send_command_c7(context, &cmd, &gram);
    }

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
    if (mcelib_legacy(context)) {
        /* U0106- */
        CHECK_OPEN(context);
  
        dsp_command cmd = {
            .command = DSP_RST,
        };

        return mcedsp_send_command_c6(context, &cmd);
    } 
    /* U0107+ */
    return mcedsp_ioctl(context, DSPIOCT_RESET_DSP, 0);
}

int mcedsp_hard_reset(mce_context_t *context)
{
    if (mcelib_legacy(context)) {
        /* U0106- */
        return mcedsp_ioctl(context, DSPDEV_IOCT_HARD_RESET, 0);
    }
    /* U0107+ -- same as mcedsp_reset. */
    return mcedsp_ioctl(context, DSPIOCT_RESET_DSP, 0);
}    

int mcedsp_reset_mce(mce_context_t *context)
{
    if (mcelib_legacy(context)) {
        /* U0106- */
        CHECK_OPEN(context);

        dsp_command cmd = {
            .command = DSP_RCO,
        };
        return mcedsp_send_command_c6(context, &cmd);
    } 
    /* U0107+ */
    return mcedsp_ioctl(context, DSPIOCT_RESET_MCE, 0);
}

int mcedsp_qt_set(mce_context_t *context, int var, int arg1, int arg2)
{
    if (!mcelib_legacy(context))
        return -DSP_ERR_WRONG_VERSION;

    /* U0106- */
    CHECK_OPEN(context);

    dsp_command cmd = {
        .command = DSP_QTS,
        .args = {var, arg1, arg2},
    };
    return mcedsp_send_command_c6(context, &cmd);
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
#endif
