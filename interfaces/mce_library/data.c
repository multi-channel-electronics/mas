#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <libmaslog.h>

#include "data_ioctl.h"
#include "mcedata.h"

#include "frame.h"
#include "data_thread.h"

/* #define LOG_LEVEL_CMD     LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_OK  LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_ER  LOGGER_INFO */


/* Data connection */

int mcedata_init(mcedata_t *mcedata, int mcecmd_handle, const char *dev_name)
{
	if (mcedata==NULL) return -1;
	mcedata->mcecmd_handle = mcecmd_handle;

	strcpy(mcedata->dev_name, dev_name);

	mcedata->fd = open(mcedata->dev_name, 0);
	if (mcedata->fd <= 0) {
		fprintf(stderr, "Could not open mce data '%s'\n",
			mcedata->dev_name);
		return -1;
	}		
	
	return 0;
}


/* ioctl on data device */

int mcedata_ioctl(mcedata_t *mcedata, int key, unsigned long arg)
{
	return ioctl(mcedata->fd, key, arg);
}

int mcedata_set_datasize(mcedata_t *mcedata, int datasize)
{
	return ioctl(mcedata->fd, DATADEV_IOCT_SET_DATASIZE, datasize);
}

int mcedata_empty_data(mcedata_t *mcedata)
{
	return ioctl(mcedata->fd, DATADEV_IOCT_EMPTY);
}

int mcedata_fake_stopframe(mcedata_t *mcedata)
{
	return ioctl(mcedata->fd, DATADEV_IOCT_FAKE_STOPFRAME);
}

int mcedata_qt_enable(mcedata_t *mcedata, int on)
{
	return ioctl(mcedata->fd, DATADEV_IOCT_QT_ENABLE, on);
}

int mcedata_qt_setup(mcedata_t *mcedata, int frame_count)
{
	return ioctl(mcedata->fd, DATADEV_IOCT_QT_CONFIG, frame_count);
}
