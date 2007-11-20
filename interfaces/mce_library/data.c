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


int mcedata_acq_reset(mce_acq_t *acq, mcedata_t *mcedata)
{
	memset(acq, 0, sizeof(*acq));
	acq->mcedata = mcedata;
	return 0;
}


int mcedata_acq_setup(mce_acq_t *acq, int options, int cards, int frame_size)
{
	int ret_val = 0;

	acq->frame_size = frame_size;
	acq->cards = cards;
	acq->options = options;

	// Set frame size in driver.
	ret_val = mcedata_set_datasize(acq->mcedata,
				       acq->frame_size * sizeof(u32));
	if (ret_val != 0) {
 		fprintf(stdout, "Could not set data size [%i]\n", ret_val);
		return -1;
	}

	return 0;
}

/* int get_go_card(char *rc_str) */
/* { */
/* 	card_t *c; */
/* 	param_t *p; */
		
/* 	int ret_val = mceconfig_lookup( */

/* } */



static int copy_frames(mce_acq_t *acq);

int mcedata_acq_go(mce_acq_t *acq, int n_frames)
{
	int ret_val = 0;
	int card_id;
	int para_id;

	u32 args[2];

	// Set number of acq_frames (HOTWIRED)
	card_id = 0x02; //CC
	para_id = 0x53; //ret_dat_s
	args[0] = 0;
	args[1] = n_frames - 1;
	ret_val = mce_write_block(acq->mcedata->mcecmd_handle,
				  card_id, para_id, 2, args);
	if (ret_val != 0) {
		fprintf(stderr, "Could not set ret_dat_s! [%#x]\n", ret_val);
		return -1;
	}

	// Start acquisition
	para_id = 0x16; //ret_dat
	switch (acq->cards) {
	case MCEDATA_RC1:
		card_id = 3;
		break;
	case MCEDATA_RC2:
		card_id = 4;
		break;
	case MCEDATA_RC3:
		card_id = 5;
		break;
	case MCEDATA_RC4:
		card_id = 6;
		break;
	case MCEDATA_RC1 | MCEDATA_RC2 | MCEDATA_RC3 | MCEDATA_RC4:
		card_id = 0xb;
		break;
	default:
		fprintf(stderr, "Invalid card set selection [%#x]\n",
			acq->cards);
		return -1;
	}

	ret_val = mce_start_application(acq->mcedata->mcecmd_handle,
					card_id, para_id);
	if (ret_val != 0) {
		fprintf(stderr, "Could not set ret_dat_s! [%#x]\n", ret_val);
		return -1;
	}

	acq->n_frames = n_frames;

	if (acq->options & MCEDATA_THREAD) {

		data_thread_t d;
		d.state = MCETHREAD_IDLE;
		d.acq = acq;
		if (data_thread_launcher(&d)) {
			fprintf(stderr, "Thread launch failure\n");
			return -1;
		}

		while (d.state != MCETHREAD_IDLE) {
			printf("thread state = %i\n", d.state);
			sleep(1);
		}

	} else {
		/* Block for frames, and return */
		
		ret_val = copy_frames(acq);
		if (ret_val != n_frames) {
			fprintf(stderr, "Data write wanted %i frames and got %i\n",
				acq->n_frames, ret_val);
		}
	}

	return 0;
}


static int copy_frames(mce_acq_t *acq)
{
	int ret_val = 0;
	int done = 0;
	int count = 0;
	int index = 0;
	u32 *data = malloc(acq->frame_size * sizeof(*data));
	
	if (data==NULL) {
		fprintf(stderr, "Could not allocate frame buffer of size %i\n",
			acq->frame_size);
		return -1;
	}

	while (!done) {

		if (acq->actions.pre_frame != NULL &&
		    acq->actions.pre_frame(acq)) {
				fprintf(stderr, "pre_frame action failed\n");
		}
	
		ret_val = read(acq->mcedata->fd, (void*)data + index,
			       acq->frame_size*sizeof(*data) - index);

		if (ret_val<0) {
			if (errno==EAGAIN) {
				usleep(1000);
			} else {
				// Error: clear rest of frame and quit
				fprintf(stderr,
					"read failed with code %i\n", ret_val);
				memset((void*)data + index, 0,
				       acq->frame_size*sizeof(*data) - index);
				done = EXIT_READ;
				break;
			}
		} else if (ret_val==0) {
			done = EXIT_EOF;
		} else
			index += ret_val;

		// Only dump complete frames to disk
		if (index < acq->frame_size*sizeof(*data))
			continue;

		// Logical formatting
		sort_columns( acq, data );

		if ( (acq->actions.post_frame != NULL) &&
		     acq->actions.post_frame( acq, count, data ) ) {
			fprintf(stderr, "post_frame action failed\n");
		}

		index = 0;
		if (++count >= acq->n_frames)
			done = EXIT_COUNT;
	}

	return count;
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

int mcedata_qt_setup(mcedata_t *mcedata, int frame_index)
{
	return ioctl(mcedata->fd, DATADEV_IOCT_QT_CONFIG, frame_index);
}
