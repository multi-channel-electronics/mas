/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <mce/data_ioctl.h>

/* Local header files */

#include "mce_library.h"
#include "context.h"
#include "eth.h"
#include "net.h"
#include "sdsu.h"
#include "data_thread.h"

/* Data connection */
int mcedata_open(mce_context_t context)
{
    int err;

    if (C_data.connected)
        mcedata_close(context);

    switch(context->dev_route) {
        case none:
            fprintf(stderr, "mcecmd: Cannot attach DATA: Null device.\n");
            return -MCE_ERR_ATTACH;
        case sdsu:
            SET_IO_METHODS(context, data, sdsu);
            break;
        case eth:
            SET_IO_METHODS(context, data, eth);
            break;
        case net:
            SET_IO_METHODS(context, data, net);
            break;
        default:
            fprintf(stderr, "mcedata: Cannot attach DATA: Unhandled route.\n");
            return -MCE_ERR_DEVICE;
    }

    if ((err = C_data.connect(context)))
        return err;

	C_data.connected = 1;

    return 0;
}

int mcedata_close(mce_context_t context)
{
    int err;
	C_data_check;

    if ((err = C_data.disconnect(context)))
        return err;

	C_data.connected = 0;

	return 0;
}

/* ioctl on data device */
int mcedata_set_datasize(mce_context_t context, int datasize)
{
    return C_data.ioctl(context, DATADEV_IOCT_SET_DATASIZE, datasize);
}

int mcedata_empty_data(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_EMPTY, 0);
}

int mcedata_fake_stopframe(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_FAKE_STOPFRAME, 0);
}

int mcedata_qt_enable(mce_context_t context, int on)
{
    return C_data.ioctl(context, DATADEV_IOCT_QT_ENABLE, on);
}

int mcedata_qt_setup(mce_context_t context, int frame_count)
{
    return C_data.ioctl(context, DATADEV_IOCT_QT_CONFIG, frame_count);
}

void mcedata_buffer_query(mce_context_t context, int *head, int *tail, int *count)
{
    *head = C_data.ioctl(context, DATADEV_IOCT_QUERY, QUERY_HEAD);
    *tail = C_data.ioctl(context, DATADEV_IOCT_QUERY, QUERY_TAIL);
    *count = C_data.ioctl(context, DATADEV_IOCT_QUERY, QUERY_MAX);
}

int mcedata_poll_offset(mce_context_t context, int *offset)
{
    *offset = C_data.ioctl(context, DATADEV_IOCT_FRAME_POLL, 0);
    return (*offset >= 0);
}

int mcedata_consume_frame(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_FRAME_CONSUME, 0);
}

int mcedata_lock_query(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_LOCK, LOCK_QUERY);
}

int mcedata_lock_reset(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_LOCK, LOCK_RESET);
}

int mcedata_lock_down(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_LOCK, LOCK_DOWN);
}

int mcedata_lock_up(mce_context_t context)
{
    return C_data.ioctl(context, DATADEV_IOCT_LOCK, LOCK_UP);
}
