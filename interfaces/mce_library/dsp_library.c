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

#include <mcedsp.h>
#include <mce/dsp_errors.h>
#include <mce/dsp_ioctl.h>

#include "context.h"
#include "eth.h"
#include "net.h"
#include "sdsu.h"

static inline int mem_type_valid(dsp_memory_code mem) {
    return (mem==DSP_MEMX) || (mem==DSP_MEMY) || (mem==DSP_MEMP);
}

#define CHECK_OPEN(ctx)    if (!ctx->dsp.opened) \
                                   return -DSP_ERR_DEVICE
#define CHECK_MEM_TYPE(mt) if (!mem_type_valid(mt)) \
                                   return -DSP_ERR_MEMTYPE
#define CHECK_WORD(w)      if (w & ~0xffffff) \
                                   return -DSP_ERR_BOUNDS;

int mcedsp_open(mce_context_t context)
{
    if (context->dev_endpoint == eth) {
        fprintf(stderr, "mcedsp: Cannot attach DSP: ethernet endpoint.");
        return -DSP_ERR_ATTACH;
    }

    switch(context->dev_route) {
        case none:
            fprintf(stderr, "mcecmd: Cannot attach DSP: Null device.\n");
            return -MCE_ERR_ATTACH;
        case sdsu:
            SET_IO_METHODS(context, dsp, sdsu);
            break;
        case net:
            SET_IO_METHODS(context, dsp, net);
            break;
        default:
            fprintf(stderr, "mcedsp: Cannot attach DSP: Unhandled route.\n");
            return -DSP_ERR_ATTACH;
    }

    return context->dsp.connect(context);
}

int mcedsp_close(mce_context_t context)
{
    int err = 0;
    CHECK_OPEN(context);

    if ((err = context->dsp.disconnect(context)))
        return err;

    context->dsp.opened = 0;

    return 0;
}


/* IOCTL wrappers */
int mcedsp_ioctl(mce_context_t context, unsigned int iocmd, unsigned long arg)
{
    CHECK_OPEN(context);

    return context->dsp.ioctl(context, iocmd, arg);
}


int mcedsp_reset_flags(mce_context_t context)
{
    return context->dsp.ioctl(context, DSPDEV_IOCT_RESET, 0);
}

int mcedsp_error(mce_context_t context)
{
    return context->dsp.ioctl(context, DSPDEV_IOCT_ERROR, 0);
}

int mcedsp_speak(mce_context_t context, unsigned long arg)
{
    return context->dsp.ioctl(context, DSPDEV_IOCT_SPEAK, arg);
}

/* COMMAND FUNCTIONALITY (wraps write, read) */

int mcedsp_send_command_now(mce_context_t context, dsp_command *cmd)
{
    if ( sizeof(*cmd) != context->dsp.write(context, cmd, sizeof(*cmd)) )
        return context->dsp.ioctl(context, DSPDEV_IOCT_ERROR, 0);

    dsp_message msg;

    if ( sizeof(msg) != context->dsp.read(context, &msg, sizeof(msg)) )
        return context->dsp.ioctl(context, DSPDEV_IOCT_ERROR, 0);

    if ( msg.type != DSP_REP ) return -DSP_ERR_UNKNOWN;
    if ( msg.command != cmd->command ) return -DSP_ERR_REPLY;
    if ( msg.reply != DSP_ACK ) return -DSP_ERR_FAILURE;

    return (int) (msg.data & DSP_DATAMASK);
}


int mcedsp_send_command(mce_context_t context, dsp_command *cmd)
{
    CHECK_OPEN(context);

    return mcedsp_send_command_now(context, cmd);
}

int mcedsp_version(mce_context_t context)
{
    CHECK_OPEN(context);

    dsp_command cmd = {DSP_VER, {0, 0, 0 }};
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_read_word(mce_context_t context, dsp_memory_code mem, int address)
{
    CHECK_OPEN(context);
    CHECK_MEM_TYPE(mem);

    dsp_command cmd = {DSP_RDM, {mem, address, 0 }};
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_read_word_X(mce_context_t context, int address)
{
    return mcedsp_read_word(context, DSP_MEMX, address);
}

int mcedsp_read_word_Y(mce_context_t context, int address)
{
    return mcedsp_read_word(context, DSP_MEMY, address);
}

int mcedsp_read_word_P(mce_context_t context, int address)
{
    return mcedsp_read_word(context, DSP_MEMP, address);
}

int mcedsp_write_word(mce_context_t context, dsp_memory_code mem, int address,
        u32 value)
{
    CHECK_OPEN(context);
    CHECK_MEM_TYPE(mem);
    CHECK_WORD(value);

    dsp_command cmd = {DSP_WRM, {mem, address, value}};
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_write_word_X(mce_context_t context, int address, u32 value)
{
    return mcedsp_write_word(context, DSP_MEMX, address, value);
}

int mcedsp_write_word_Y(mce_context_t context, int address, u32 value)
{
    return mcedsp_write_word(context, DSP_MEMY, address, value);
}

int mcedsp_write_word_P(mce_context_t context, int address, u32 value)
{
    return mcedsp_write_word(context, DSP_MEMP, address, value);
}


int mcedsp_reset(mce_context_t context)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
command : DSP_RST,
    };

    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_start_application(mce_context_t context, int data)
{
    CHECK_OPEN(context);
    CHECK_WORD(data);

    dsp_command cmd = {DSP_GOA, {data,0,0} };
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_stop_application(mce_context_t context)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
command : DSP_STP,
    };
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_reset_mce(mce_context_t context)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
command: DSP_RCO,
    };
    return mcedsp_send_command_now(context, &cmd);
}

int mcedsp_qt_set(mce_context_t context, int var, int arg1, int arg2)
{
    CHECK_OPEN(context);

    dsp_command cmd = {
command: DSP_QTS,
         args: {var, arg1, arg2},
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
