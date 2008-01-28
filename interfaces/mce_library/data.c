#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "mce_library.h"
#include "data_ioctl.h"

/* Local header files */

#include "frame.h"
#include "data_thread.h"

/* #define LOG_LEVEL_CMD     LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_OK  LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_ER  LOGGER_INFO */


/* Data connection */

int mcedata_open(mce_context_t *context, const char *dev_name)
{
	if (C_data.connected) mcedata_close(context);

	if (strlen(dev_name)>=MCE_LONG-1)
		return -MCE_ERR_BOUNDS;

	C_data.fd = open(dev_name, O_RDWR);
	if (C_data.fd<0) return -MCE_ERR_DEVICE;

	// Non-blocking reads allow us to timeout
	if (fcntl(C_data.fd, F_SETFL, fcntl(C_data.fd, F_GETFL) | O_NONBLOCK))
		return -MCE_ERR_DEVICE;

	C_data.connected = 1;
	strcpy(C_data.dev_name, dev_name);

	return 0;
}

int mcedata_close(mce_context_t *context)
{
	C_data_check;

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
