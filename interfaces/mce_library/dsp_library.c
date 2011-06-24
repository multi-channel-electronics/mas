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

inline int mem_type_valid(dsp_memory_code mem) {
    return (mem==DSP_MEMX) || (mem==DSP_MEMY) || (mem==DSP_MEMP);
}

#define CHECK_OPEN(ctx)    if (!ctx->dsp.opened) \
                                   return -DSP_ERR_DEVICE
#define CHECK_MEM_TYPE(mt) if (!mem_type_valid(mt)) \
                                   return -DSP_ERR_MEMTYPE
#define CHECK_WORD(w)      if (w & ~0xffffff) \
                                   return -DSP_ERR_BOUNDS;

static int mcedsp_sdsu_connect(mce_context_t context)
{
    int fd;
    char dev_name[18] = "/dev/mce_dsp";
    sprintf(dev_name + 12, "%u", (unsigned)context->dev_num);

    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        return -DSP_ERR_DEVICE;
    }

    context->dsp.fd = fd;
    context->dsp.opened = 1;

    return 0;
}

static int mcedsp_eth_connect(mce_context_t context)
{
    fprintf(stderr,
            "mcedsp: Ethernet routing does not support DSP operations.");
    return -DSP_ERR_DEVICE;
}

static int mcedsp_net_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedsp_open(mce_context_t context)
{
    switch(context->dev_route) {
        case sdsu:
            return mcedsp_sdsu_connect(context);
        case eth:
            return mcedsp_eth_connect(context);
        case net:
            return mcedsp_net_connect(context);
        default:
            fprintf(stderr, "mcedsp: Unhandled device type.\n");
            return -DSP_ERR_DEVICE;
    }
}

int mcedsp_close(mce_context_t context)
{
    CHECK_OPEN(context);

    if (close(context->dsp.fd) < 0)
        return -DSP_ERR_DEVICE;
    context->dsp.opened = 0;
    context->dsp.fd = -1;

    return 0;
}


/* IOCTL wrappers */

int mcedsp_ioctl(mce_context_t context, unsigned int iocmd, unsigned long arg)
{
    CHECK_OPEN(context);

    if (context->dev_route == sdsu)
        return ioctl(context->dsp.fd, iocmd, arg);
    else {
        fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
                __FILE__);
        abort();
    }
}


int mcedsp_reset_flags(mce_context_t context)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_RESET, 0);
}

int mcedsp_error(mce_context_t context)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_ERROR, 0);
}

int mcedsp_speak(mce_context_t context, unsigned long arg)
{
    return mcedsp_ioctl(context, DSPDEV_IOCT_SPEAK, arg);
}

/* read hooks */
static ssize_t mcedsp_net_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

static ssize_t mcedsp_eth_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

static ssize_t mcedsp_read(mce_context_t context, void *buf, size_t count)
{
    switch(context->dev_route) {
        case sdsu:
            return read(context->dsp.fd, buf, count);
        case eth:
            return mcedsp_eth_read(context, buf, count);
        case net:
            return mcedsp_net_read(context, buf, count);
        default:
            fprintf(stderr, "mcedsp: Unhandled device type.\n");
            return -DSP_ERR_DEVICE;
    }
}

/* write hooks */
static ssize_t mcedsp_net_write(mce_context_t context, const void *buf, 
        size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

static ssize_t mcedsp_eth_write(mce_context_t context, const void *buf, 
        size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

static ssize_t mcedsp_write(mce_context_t context, const void *buf,
        size_t count)
{
    switch(context->dev_route) {
        case sdsu:
            return write(context->dsp.fd, buf, count);
        case eth:
            return mcedsp_eth_write(context, buf, count);
        case net:
            return mcedsp_net_write(context, buf, count);
        default:
            fprintf(stderr, "mcedsp: Unhandled device type.\n");
            return -DSP_ERR_DEVICE;
    }
}

/* COMMAND FUNCTIONALITY (wraps write, read) */

int mcedsp_send_command_now(mce_context_t context, dsp_command *cmd)
{
    if ( sizeof(*cmd) != mcedsp_write(context, cmd, sizeof(*cmd)) )
        return mcedsp_ioctl(context, DSPDEV_IOCT_ERROR, 0);

    dsp_message msg;

    if ( sizeof(msg) != mcedsp_read(context, &msg, sizeof(msg)) )
        return mcedsp_ioctl(context, DSPDEV_IOCT_ERROR, 0);

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
