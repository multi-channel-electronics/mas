/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/*******************************************************

  cmd.c - most command module routines live here

 *******************************************************/

#include "mce_library.h"

#ifdef NO_MCE_OPS
MAS_UNSUPPORTED(int mcecmd_close(mce_context_t *context));
#else

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "mce/ioctl.h"
#include "mce/dsp.h"

#include "context.h"
#include "virtual.h"
#include "manip.h"

#define LOG_LEVEL_CMD     MASLOG_DETAIL
#define LOG_LEVEL_REP_OK  MASLOG_DETAIL
#define LOG_LEVEL_REP_ER  MASLOG_INFO

/* choose IOCTL based on driver version */
#define CMDIOCTL(ctx, new_req, old_req, ...) \
    ioctl(ctx->cmd.fd, mcelib_legacy(ctx) ? old_req : new_req, ##__VA_ARGS__)

static inline int get_last_error(mce_context_t *context)
{
    return ioctl(C_cmd.fd, MCEDEV_IOCT_LAST_ERROR);
}


static int log_data(maslog_t *logger, uint32_t *buffer, int count, int min_raw,
        char *msg, int level)
{
    char out[2048];
    char *s = out + sprintf(out, "%s", msg);

    if (count > 2048/5)
        count = 2048/5;

    if (min_raw > count)
        min_raw = count;

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

    return maslog_print_level(logger, out, level);
}


int mcecmd_open(mce_context_t *context)
{
    int err;

    if (C_cmd.connected)
        mcecmd_close(context);

    err = mcedev_open(context, MCE_SUBSYSTEM_CMD);
    if (err)
        return err;

    if (mcelib_legacy(context)) {
        /* Set up connection to prevent outstanding replies after release,
         * and to enforce that only commander can retrieve reply. */
        ioctl(C_cmd.fd, MCEDEV_IOCT_SET,
                ioctl(C_cmd.fd, MCEDEV_IOCT_GET) |
                (MCEDEV_CLOSE_CLEANLY | MCEDEV_CLOSED_CHANNEL));
    }

    /* connect to the logger, if necessary */
    if (context->maslog == NULL)
        context->maslog = maslog_connect(context, "lib_mce");

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
    int error;

    if (mcelib_legacy(context)) { /* U0106- */
        error = write(C_cmd.fd, cmd, sizeof(*cmd));
        if (error < 0)
            return -MCE_ERR_DEVICE;
        else if (error != sizeof(*cmd))
            return get_last_error(context);
    } else { /* U0107+ */
        error = ioctl(C_cmd.fd, DSPIOCT_MCE_COMMAND, (unsigned long)cmd);
        if (error < 0) {
            switch (errno) {
                case ENODATA:
                    return -MCE_ERR_INT_TIMEOUT;
                case EIO:
                    return -MCE_ERR_INT_FAILURE;
            }
            return -MCE_ERR_INT_UNKNOWN;
        }
    }

    return 0;
}

int mcecmd_read_reply_now(mce_context_t* context, mce_reply *rep)
{
    int error;

    if (mcelib_legacy(context)) { /* U0106- */
        error = read(C_cmd.fd, rep, sizeof(*rep));
        if (error < 0)
            return -MCE_ERR_DEVICE;
        else if (error != sizeof(*rep))
            return get_last_error(context);
    } else { /* U0107+ */
        struct dsp_datagram gram;
        struct mce_reply *rep0; /* ouch */
        error = ioctl(C_cmd.fd, DSPIOCT_GET_MCE_REPLY, (unsigned long)&gram);
        if (error < 0) {
            switch (errno) {
                case ENODATA:
                    return -MCE_ERR_TIMEOUT;
                case EIO:
                    return -MCE_ERR_INT_FAILURE;
            }
            return -MCE_ERR_INT_UNKNOWN;
        }

        /* Datagram->buffer contains "  RP", size, then only "size" valid
         * words. */
        rep0 = MCE_REPLY(&gram);
        memset(rep, 0, sizeof(*rep));
        memcpy(rep, rep0->data, rep0->size * sizeof(uint32_t));
    }

    return 0;
}


#define MAX_SEND_ATTEMPTS 5

int mcecmd_send_command(mce_context_t* context, mce_command *cmd, mce_reply *rep)
{
    int err = 0;
    int attempts = 0;
    char errstr[MCE_LONG];
    C_cmd_check;

    log_data(context->maslog, (uint32_t*)cmd + 2, 62, 2, "command",
            LOG_LEVEL_CMD);

    /* Loop the attempts to protect against very rare partial
       command packet transfers.  Reply packet will have zeros for
       card address and thus fail the consistency check. */

    do {
        err = mcecmd_send_command_now(context, cmd);
        if (err<0) {
            sprintf(errstr, "command not sent, error %#x.", -err);
            maslog_print_level(context->maslog, errstr, MASLOG_INFO);
            memset(rep, 0, sizeof(*rep));
            return err;
        }

        err = mcecmd_read_reply_now(context, rep);
        if (err != 0) {
            sprintf(errstr, "reply [communication error] %s",
                    mcelib_error_string(err));
            maslog_print_level(context->maslog, errstr, LOG_LEVEL_REP_ER);
            return err;
        }

        // Analysis of received packet
        err = mcecmd_checksum((uint32_t*)rep, sizeof(*rep) / sizeof(uint32_t)) ?
            -MCE_ERR_CHKSUM : 0;

        if (!err)
            err = mcecmd_cmd_match_rep(cmd, rep);

    } while (attempts++ < MAX_SEND_ATTEMPTS && err == -MCE_ERR_REPLY);

    switch (-err) {

        case MCE_ERR_CHKSUM:
            log_data(context->maslog, (uint32_t*)rep, 60, 2,
                    "reply [checksum error] ",
                    LOG_LEVEL_REP_ER);
            break;

        case MCE_ERR_FAILURE:
            log_data(context->maslog, (uint32_t*)rep, 60, 2,
                    "reply [command failed] ",
                    LOG_LEVEL_REP_ER);
            break;

        case MCE_ERR_REPLY:
            log_data(context->maslog, (uint32_t*)rep, 60, 2,
                    "reply [consistency error] ",
                    LOG_LEVEL_REP_ER);
            break;

        case 0:
            log_data(context->maslog, (uint32_t*)rep, 60, 2, "reply  ",
                    LOG_LEVEL_REP_OK);
            break;

        default:
            sprintf(errstr, "reply [strange error '%s'] ",
                    mcelib_error_string(err));
            log_data(context->maslog, (uint32_t*)rep, 60, 2,
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
        int count, uint32_t *data)
{
    // This used to run the show, but these days we pass it to write_range
    return mcecmd_write_range(context, param, 0, data, count);
}

int mcecmd_read_block(mce_context_t* context, const mce_param_t *param,
        int count, uint32_t *data)
{
    // Boring...
    return mcecmd_read_range(context, param, 0, data, count);
}


int mcecmd_send_command_simple(mce_context_t* context, int card_id, int para_id,
        uint32_t cmd_code)
{
    mce_command cmd = {
        .preamble = {PREAMBLE_0, PREAMBLE_1},
        .command  = (u32)cmd_code,
        .card_id  = (u16)card_id,
        .para_id  = (u16)para_id,
        .count    = 1
    };

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


int mcecmd_read_size(const mce_param_t *p, int count)
{
    if (count < 0)
        count = p->param.count;
    return p->card.card_count * count;
}


/* MCE special commands - these provide additional logical support */

int mcecmd_write_range(mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t *data, int count)
{
    int error = 0;
    uint32_t _block[MCE_CMD_DATA_MAX];
    uint32_t* block = _block;
    int i;

    C_cmd_check;

    if (count < 0)
        count = param->param.count - data_index;

    // Redirect banked access through decoder.  We should do this
    // before prewrite_manip, or it will get applied twice.
    if (param->param.bank_scheme == 1)
        return mcecmd_readwrite_banked(context, param, data_index, data, count, 'w');

    // Pre-process command data before writing to MCE?
    mcecmd_prewrite_manip(param, data, count);

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
            if (error)
                return error;

            error = mcecmd_send_command(context, &cmd, &rep);
            if (error)
                return error;

            memcpy(block, rep.data, data_index*sizeof(*block));
        }

        // Append our data
        memcpy(block+data_index, data, count*sizeof(*data));

        // Map the card, write the block
        error = mcecmd_load_command(
                &cmd, MCE_WB,
                param->card.id[i], param->param.id,
                count+data_index, count+data_index, block);
        if (error)
            return error;

        error = mcecmd_send_command(context, &cmd, &rep);
        if (error)
            return error;
    }
    return 0;
}

int mcecmd_read_range(mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t *data, int count)
{
    int error = 0, i;

    C_cmd_check;

    if (count < 0)
        count = param->param.count;

    // Redirect Virtual cards, though virtual system will recurse here
    if (param->card.nature == MCE_NATURE_VIRTUAL)
        return mcecmd_read_virtual(context, param, data_index, data, count);

    // Redirect banked access, though they, too, will recurse here.
    if (param->param.bank_scheme == 1)
        return mcecmd_readwrite_banked(context, param, data_index, data, count, 'r');

    for (i=0; i<param->card.card_count; i++) {
        mce_command cmd;
        mce_reply rep;
        error = mcecmd_load_command(&cmd, MCE_RB,
                param->card.id[i], param->param.id,
                count+data_index, 0, NULL);
        if (error)
            return error;

        error = mcecmd_send_command(context, &cmd, &rep);
        if (error)
            return error;

        // I guess the data must be valid then.
        memcpy(data+i*count, rep.data+data_index, count*sizeof(uint32_t));
    }

    // Post-process the reply data before returning?
    mcecmd_postread_manip(param, data, count);

    return 0;
}

int mcecmd_write_element(mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t datum)
{
    return mcecmd_write_range(context, param, data_index, &datum, 1);
}

int mcecmd_read_element(mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t *datum)
{
    return mcecmd_read_range(context, param, data_index, datum, 1);
}

int mcecmd_write_block_check(mce_context_t* context, const mce_param_t *param,
        int count, uint32_t *data, int retries)
{
    int i, error;
    uint32_t readback[MCE_CMD_DATA_MAX];
    int done = 0;

    if (param->card.card_count != 1)
        return -MCE_ERR_MULTICARD;

    do {
        error = mcecmd_write_block(context, param, count, data);

        if (error)
            return error;

        error = mcecmd_read_block(context, param, count, readback);
        if (error)
            return error;

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
    /* This amounts to a PCI card reset. */
    return CMDIOCTL(context, DSPIOCT_RESET_DSP, MCEDEV_IOCT_INTERFACE_RESET);
}

int mcecmd_hardware_reset(mce_context_t* context)
{
    return CMDIOCTL(context, DSPIOCT_RESET_MCE, MCEDEV_IOCT_HARDWARE_RESET);
}
#endif
