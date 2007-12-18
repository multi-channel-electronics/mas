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


// We'll have to generalize this if the frame structure ever changes.

#define FRAME_HEADER 43
#define FRAME_COLUMNS 8 /* per card */
#define FRAME_FOOTER 1


static int copy_frames(mce_acq_t *acq);

static int set_n_frames(mce_acq_t *acq, int n_frames);

static int card_count(int cards);

static int load_ret_dat(mce_acq_t *acq);


int mcedata_acq_setup(mce_acq_t *acq, mcedata_t *mcedata,
		      int options, int cards, int rows_reported)
{
	int ret_val = 0;
	int n_cards = card_count(cards);

	// Zero the structure!
	memset(acq, 0, sizeof(*acq));

	if (n_cards <= 0) {
		fprintf(stdout, "Invalid card selection\n");
		return -1;
	}

	acq->frame_size = rows_reported * FRAME_COLUMNS * n_cards + 
		FRAME_HEADER + FRAME_FOOTER;
	acq->cards = cards;
	acq->options = options;
	acq->mcedata = mcedata;

	// Set frame size in driver.
	ret_val = mcedata_set_datasize(acq->mcedata,
				       acq->frame_size * sizeof(u32));
	if (ret_val != 0) {
 		fprintf(stdout, "Could not set data size [%i]\n", ret_val);
		return -1;
	}
	return 0;
}


int mcedata_acq_go(mce_acq_t *acq, int n_frames)
{
	int ret_val = 0;

	// Check if ret_dat_s needs changing...
	if ( n_frames != acq->last_n_frames || acq->last_n_frames <= 0 ) {
		ret_val = set_n_frames(acq, n_frames);
		if (ret_val != 0) {
			fprintf(stderr,
				"Could not set ret_dat_s! [%#x]\n", ret_val);
			return -1;
		}
	}

	if (!acq->know_ret_dat && (ret_val = load_ret_dat(acq)) != 0)
		return ret_val;
		
	ret_val =
		mce_start_application(acq->mcedata->mcecmd_handle, &acq->ret_dat);

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


/* set_n_frames - must tell both the MCE and the DSP about the number
 * of frames to expect. */

static int set_n_frames(mce_acq_t *acq, int n_frames)
{
	int ret_val;
	u32 args[2];
	acq->know_ret_dat_s = acq->know_ret_dat_s || 
		(mce_load_param(acq->mcedata->mcecmd_handle,
				&acq->ret_dat_s, "cc", "ret_dat_s")==0);

	if (!acq->know_ret_dat_s) {
		fprintf(stderr, "Could not load 'cc ret_dat_s'\n");
		return -MCE_ERR_XML;
	}
	
	args[0] = 0;
	args[1] = n_frames - 1;
	ret_val = mce_write_block(acq->mcedata->mcecmd_handle,
				  &acq->ret_dat_s, 2, args);
	if (ret_val != 0) {
		fprintf(stderr, "Could not set ret_dat_s! [%#x]\n", ret_val);
		acq->last_n_frames = -1;
	} else {
		acq->last_n_frames = n_frames;
	}

	// Inform DSP/driver, also.
	if (mcedata_qt_setup(acq->mcedata, n_frames)) {
		fprintf(stderr, "Failed to set quiet transfer interval!\n");
		return -MCE_ERR_DEVICE;
	}

	return ret_val;
}


int load_ret_dat(mce_acq_t *acq)
{
	// Start acquisition
	switch (acq->cards) {
	case MCEDATA_RC1:
		mce_load_param(acq->mcedata->mcecmd_handle, &acq->ret_dat,
			       "rc1", "ret_dat");
		break;
	case MCEDATA_RC2:
		mce_load_param(acq->mcedata->mcecmd_handle, &acq->ret_dat,
			       "rc2", "ret_dat");
		break;
	case MCEDATA_RC3:
		mce_load_param(acq->mcedata->mcecmd_handle, &acq->ret_dat,
			       "rc3", "ret_dat");
		break;
	case MCEDATA_RC4:
		mce_load_param(acq->mcedata->mcecmd_handle, &acq->ret_dat,
			       "rc4", "ret_dat");
		break;
	case MCEDATA_RCS:
		mce_load_param(acq->mcedata->mcecmd_handle, &acq->ret_dat,
			       "rcs", "ret_dat");
		break;
	default:
		fprintf(stderr, "Invalid card set selection [%#x]\n",
			acq->cards);
		return -1;
	}

	acq->know_ret_dat = 1;
	return 0;
}


int copy_frames(mce_acq_t *acq)
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

int card_count(int cards)
{
	switch (cards) {

	case MCEDATA_RC1:
	case MCEDATA_RC2:
	case MCEDATA_RC3:
	case MCEDATA_RC4:
		return 1;

	case MCEDATA_RCS:
		return 4;
	}
	return -1;
}
