#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "mce_library.h"
#include <mce/data_ioctl.h>

/* Local header files */

#include "data_thread.h"

/* #define LOG_LEVEL_CMD     LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_OK  LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_ER  LOGGER_INFO */


/* Data connection */

int mcedata_open(mce_context_t *context, const char *dev_name)
{
	void *map;
	int map_size;

	if (C_data.connected) mcedata_close(context);

	if (strlen(dev_name)>=MCE_LONG-1)
		return -MCE_ERR_BOUNDS;

	C_data.fd = open(dev_name, O_RDWR);
	if (C_data.fd<0) return -MCE_ERR_DEVICE;

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
	strcpy(C_data.dev_name, dev_name);

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
	return ioctl(C_data.fd, DATADEV_IOCT_SET_DATASIZE, datasize);
}

int mcedata_empty_data(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_EMPTY);
}

int mcedata_fake_stopframe(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_FAKE_STOPFRAME);
}

int mcedata_qt_enable(mce_context_t* context, int on)
{
	return ioctl(C_data.fd, DATADEV_IOCT_QT_ENABLE, on);
}

int mcedata_qt_setup(mce_context_t* context, int frame_count)
{
	return ioctl(C_data.fd, DATADEV_IOCT_QT_CONFIG, frame_count);
}

void mcedata_buffer_query(mce_context_t* context, int *head, int *tail, int *count)
{
	*head = ioctl(C_data.fd, DATADEV_IOCT_QUERY, QUERY_HEAD);
	*tail = ioctl(C_data.fd, DATADEV_IOCT_QUERY, QUERY_TAIL);
	*count = ioctl(C_data.fd, DATADEV_IOCT_QUERY, QUERY_MAX);
}

int mcedata_poll_offset(mce_context_t* context, int *offset)
{
	*offset = ioctl(C_data.fd, DATADEV_IOCT_FRAME_POLL);
	return (*offset >= 0);
}

int mcedata_consume_frame(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_FRAME_CONSUME);
}

int mcedata_lock_query(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_LOCK, LOCK_QUERY);
}

int mcedata_lock_reset(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_LOCK, LOCK_RESET);
}

int mcedata_lock_down(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_LOCK, LOCK_DOWN);
}

int mcedata_lock_up(mce_context_t* context)
{
	return ioctl(C_data.fd, DATADEV_IOCT_LOCK, LOCK_UP);
}

