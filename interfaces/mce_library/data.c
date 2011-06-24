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
#include <sys/mman.h>

#include <mce/data_ioctl.h>

/* Local header files */

#include "mce_library.h"
#include "context.h"
#include "data_thread.h"

/* SDSU specific stuff */
static int mcedata_sdsu_connect(mce_context_t context)
{
	void *map;
	int map_size;
    char dev_name[20] = "/dev/mce_data";
    sprintf(dev_name + 13, "%u", (unsigned)context->dev_num);

	C_data.fd = open(dev_name, O_RDWR);
    if (C_data.fd < 0)
        return -MCE_ERR_DEVICE;

	// Non-blocking reads allow us to timeout
	/* Hey!  This makes single go (and thus ramp) very slow!! */
/* 	if (fcntl(C_data.fd, F_SETFL, fcntl(C_data.fd, F_GETFL) | O_NONBLOCK)) */
/* 		return -MCE_ERR_DEVICE; */

	// Obtain buffer size for subsequent mmap
	map_size = ioctl(C_data.fd, DATADEV_IOCT_QUERY, QUERY_BUFSIZE);
	if (map_size > 0) {
		map = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
			   MAP_SHARED, C_data.fd, 0);
		if (map != NULL) {
			C_data.map = map;
			C_data.map_size = map_size;
		}
	}

	C_data.connected = 1;
    C_data.dev_name = dev_name;

	return 0;
}

/* ethernet specific stuff */
static int mcedata_eth_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

/* mcenetd specific stuff */
static int mcedata_net_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

/* Data connection */
int mcedata_open(mce_context_t context)
{
    if (C_data.connected)
        mcedata_close(context);

    switch(context->dev_route) {
        case sdsu:
            return mcedata_sdsu_connect(context);
        case eth:
            return mcedata_eth_connect(context);
        case net:
            return mcedata_net_connect(context);
        default:
            fprintf(stderr, "mcedata: Unhandled route.\n");
            return -MCE_ERR_DEVICE;
    }
}

static int mcedata_sdsu_disconnect(mce_context_t context)
{
    if (C_data.map != NULL)
		munmap(C_data.map, C_data.map_size);

	C_data.map_size = 0;
	C_data.map = NULL;

	if (close(C_data.fd) < 0)
		return -MCE_ERR_DEVICE;

}

/* ethernet specific stuff */
static int mcedata_eth_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

/* mcenetd specific stuff */
static int mcedata_net_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedata_close(mce_context_t context)
{
    int err;
	C_data_check;

    switch(context->dev_route) {
        case sdsu:
            err = mcedata_sdsu_connect(context);
        case eth:
            err = mcedata_eth_connect(context);
        case net:
            err = mcedata_net_connect(context);
        default:
            fprintf(stderr, "mcedata: Unhandled route.\n");
            return -MCE_ERR_DEVICE;
    }
    if (err)
        return err;

	C_data.connected = 0;

	return 0;
}

/* ioctl on data device */

int mcedata_ioctl_generic(mce_context_t context, int key, void *arg)
{
    if (context->dev_route == sdsu)
        return ioctl(C_data.fd, key, arg);
    else {
        fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
                __FILE__);
        abort();
    }
}

int mcedata_set_datasize(mce_context_t context, int datasize)
{
    return mcedata_ioctl(context, DATADEV_IOCT_SET_DATASIZE, datasize);
}

int mcedata_empty_data(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_EMPTY, NULL);
}

int mcedata_fake_stopframe(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_FAKE_STOPFRAME, NULL);
}

int mcedata_qt_enable(mce_context_t context, int on)
{
    return mcedata_ioctl(context, DATADEV_IOCT_QT_ENABLE, on);
}

int mcedata_qt_setup(mce_context_t context, int frame_count)
{
    return mcedata_ioctl(context, DATADEV_IOCT_QT_CONFIG, frame_count);
}

void mcedata_buffer_query(mce_context_t context, int *head, int *tail, int *count)
{
    *head = mcedata_ioctl(context, DATADEV_IOCT_QUERY, QUERY_HEAD);
    *tail = mcedata_ioctl(context, DATADEV_IOCT_QUERY, QUERY_TAIL);
    *count = mcedata_ioctl(context, DATADEV_IOCT_QUERY, QUERY_MAX);
}

int mcedata_poll_offset(mce_context_t context, int *offset)
{
    *offset = mcedata_ioctl(context, DATADEV_IOCT_FRAME_POLL, NULL);
    return (*offset >= 0);
}

int mcedata_consume_frame(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_FRAME_CONSUME, NULL);
}

int mcedata_lock_query(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_LOCK, LOCK_QUERY);
}

int mcedata_lock_reset(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_LOCK, LOCK_RESET);
}

int mcedata_lock_down(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_LOCK, LOCK_DOWN);
}

int mcedata_lock_up(mce_context_t context)
{
    return mcedata_ioctl(context, DATADEV_IOCT_LOCK, LOCK_UP);
}
