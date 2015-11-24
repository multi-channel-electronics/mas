/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#define _GNU_SOURCE

#include "mce_library.h"

#ifdef NO_MCE_OPS
MAS_UNSUPPORTED(int mcedata_close(mce_context_t *context))
#else

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "mce/defaults.h"
#include <mce/ioctl.h>

/* Local header files */

#include "context.h"
#include "data_thread.h"

/* choose IOCTL based on driver version */
#define DATAIOCTL(ctx, new_req, old_req, ...) \
    ioctl(ctx->data.fd, mcelib_legacy(ctx) ? old_req : new_req, ##__VA_ARGS__)

/* Data connection */

int mcedata_open(mce_context_t *context)
{
	void *map;
	int err, map_size;
 
    if (C_data.connected)
        mcedata_close(context);

    err = mcedev_open(context, MCE_SUBSYSTEM_CMD);
    if (err)
        return err;

	// Obtain buffer size for subsequent mmap
	map_size = DATAIOCTL(context, DATADEV_IOCT_QUERY, DSPIOCT_QUERY,
                         QUERY_BUFSIZE);
	if (map_size > 0) {
		map = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
			   MAP_SHARED, C_data.fd, 0);
		if (map != NULL) {
			C_data.map = map;
			C_data.map_size = map_size;
		}
	}

	return 0;
}

int mcedata_close(mce_context_t *context)
{
	C_data_check;

	if (C_data.map != NULL) 
		munmap(C_data.map, C_data.map_size);

	C_data.map_size = 0;
	C_data.map = NULL;

	if (close(C_data.fd) < 0)
		return -MCE_ERR_DEVICE;

	C_data.connected = 0;

	return 0;
}

/* ioctl on data device */

int mcedata_ioctl(mce_context_t* context, int key, unsigned long arg)
{
	return ioctl(C_data.fd, key, arg);
}

int mcedata_set_datasize(mce_context_t* context, int datasize)
{
    return DATAIOCTL(context, DATADEV_IOCT_SET_DATASIZE, DSPIOCT_SET_DATASIZE,
            datasize);
}

int mcedata_empty_data(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_EMPTY, DSPIOCT_EMPTY);
}

int mcedata_fake_stopframe(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_FAKE_STOPFRAME,
            DSPIOCT_FAKE_STOPFRAME);
}

int mcedata_set_nframes(mce_context_t* context, int frame_count)
{
    return DATAIOCTL(context, DATADEV_IOCT_QT_CONFIG, DSPIOCT_SET_NFRAMES,
            frame_count);
}

void mcedata_buffer_query(mce_context_t* context, int *head, int *tail,
        int *count)
{
    *head = DATAIOCTL(context, DATADEV_IOCT_QUERY, DSPIOCT_QUERY, QUERY_HEAD);
    *tail = DATAIOCTL(context, DATADEV_IOCT_QUERY, DSPIOCT_QUERY, QUERY_TAIL);
    *count = DATAIOCTL(context, DATADEV_IOCT_QUERY, DSPIOCT_QUERY, QUERY_MAX);
}

int mcedata_poll_offset(mce_context_t* context, int *offset)
{
    *offset = DATAIOCTL(context, DSPIOCT_FRAME_POLL, DSPIOCT_FRAME_POLL);
    if (*offset < 0) {
        *offset = errno;
        return 0;
    }
    return 1;
}

int mcedata_consume_frame(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_FRAME_CONSUME,
            DSPIOCT_FRAME_CONSUME);
}

int mcedata_lock_query(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_LOCK, DSPIOCT_DATA_LOCK, LOCK_QUERY);
}

int mcedata_lock_reset(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_LOCK, DSPIOCT_DATA_LOCK, LOCK_RESET);
}

int mcedata_lock_down(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_LOCK, DSPIOCT_DATA_LOCK, LOCK_DOWN);
}

int mcedata_lock_up(mce_context_t* context)
{
    return DATAIOCTL(context, DATADEV_IOCT_LOCK, DSPIOCT_DATA_LOCK, LOCK_UP);
}
#endif
