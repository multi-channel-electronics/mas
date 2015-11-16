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

#include <mce_library.h>

#ifdef NO_MCE_OPS
#else
#include <mce/ioctl.h>

#include "context.h"
#include "data_thread.h"
#include "frame_manip.h"

/* #define LOG_LEVEL_CMD     LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_OK  LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_ER  LOGGER_INFO */

#define FRAME_USLEEP 1000


static int copy_frames_mmap(mce_acq_t *acq);

static int set_n_frames(mce_acq_t *acq, int n_frames, int dsp_only);

static int get_n_frames(mce_acq_t *acq);

static int card_count(int cards);

static int load_frame_params(mce_acq_t *acq, int cards);

static int load_data_params(mce_acq_t *acq);

static int load_ret_dat(mce_acq_t *acq);

/* static int cards_to_rcsflags(int c); */

static int rcsflags_to_cards(int c);

int mcedata_acq_create(mce_acq_t *acq, mce_context_t* context,
        int options, int cards, int rows_reported,
        mcedata_storage_t *storage)
{
	int ret_val = 0;

	// Zero the structure!
	memset(acq, 0, sizeof(*acq));

    // Save context, options, etc.
	acq->options = options;
	acq->context = context;
	acq->storage = storage;

	// Load frame size parameters from MCE
	ret_val = load_frame_params(acq, cards);
	if (ret_val != 0) return ret_val;

	// Load data description stuff
	ret_val = load_data_params(acq);
	if (ret_val != 0) return ret_val;

	// Save frame size and other options
	acq->frame_size = acq->rows * acq->cols * acq->n_cards + 
		MCEDATA_HEADER + MCEDATA_FOOTER;

    // Check that it is a reasonable size.
    if (acq->frame_size > MCEDATA_PACKET_MAX) {
        fprintf(stderr, "MCE packet size too large (%i dwords), failing.\n",
                acq->frame_size);
        return -MCE_ERR_FRAME_SIZE;
    }

	// Lookup "cc ret_dat_s" (frame count) or fail
	if ((ret_val=mcecmd_load_param(
		     acq->context, &acq->ret_dat_s, "cc", "ret_dat_s")) != 0) {
/* 		fprintf(stderr, "Could not load 'cc ret_dat_s'\n"); */
		return ret_val;
	}

	// Set frame size in driver.
	ret_val = mcedata_set_datasize(acq->context,
            acq->frame_size * sizeof(uint32_t));
	if (ret_val != 0) {
        mcelib_error(context, "Could not set data size to %i [%i]\n",
                acq->frame_size, ret_val);
		return -MCE_ERR_FRAME_SIZE;
	}

	if (acq->storage->init != NULL && acq->storage->init(acq) != 0) {
        mcelib_error(context, "Storage init action failed.\n");
		return -MCE_ERR_FRAME_OUTPUT;
	}

    // Sensible defaults.
    acq->timeout_ms = 1000;

	acq->ready = 1;
	return 0;
}

int mcedata_acq_destroy(mce_acq_t *acq)
{
	if (acq->storage->cleanup != NULL && acq->storage->cleanup(acq) != 0) {
        mcelib_error(acq->context, "Storage init action failed.\n");
		return -MCE_ERR_FRAME_OUTPUT;
	}

	acq->storage = mcedata_storage_destroy(acq->storage);

	return 0;
}

mce_acq_t *mcedata_acq_duplicate(mce_acq_t *acq)
{
    mce_acq_t *acq_copy = (mce_acq_t *)malloc(sizeof(*acq));
    if (acq_copy != NULL)
        memcpy(acq_copy, acq, sizeof(*acq));
    return acq_copy;
}

int mcedata_acq_go(mce_acq_t *acq, int n_frames)
{
	int ret_val = 0;

	// Assertion
	if (acq==NULL || !acq->ready) {
        fprintf(stderr,
                "mcedata_acq_go: acq structure (%p) null or not ready!\n", acq);
		return -MCE_ERR_FRAME_UNKNOWN;
    }

	// Does checking / setting ret_dat_s really slow us down?
	if (n_frames < 0) {
		n_frames = get_n_frames(acq);
		if (n_frames <= 0) return -MCE_ERR_FRAME_COUNT;
	}

	// Check if ret_dat_s needs changing...
	if ( n_frames != acq->last_n_frames || acq->last_n_frames <= 0 ) {
		ret_val = set_n_frames(acq, n_frames, 0);
		if (ret_val != 0)
			return -MCE_ERR_FRAME_COUNT;
	}

	// Issue the MCE 'GO' command.
	ret_val = mcecmd_start_application(acq->context, &acq->ret_dat);
	if (ret_val != 0) return ret_val;

	// Launch the data thread, or just block for data.
	acq->n_frames = n_frames;

	if (acq->options & MCEDATA_THREAD) {
		data_thread_t d;
		d.state = MCETHREAD_IDLE;
		d.acq = acq;
		if ((ret_val=data_thread_launcher(&d)) != 0)
			return ret_val;

		while (d.state != MCETHREAD_IDLE) {
/* 			printf("thread state = %i\n", d.state); */
			usleep(FRAME_USLEEP);
		}

	} else {
		/* Block for frames, and return */
		if (acq->context->data.map != NULL) {
			ret_val = copy_frames_mmap(acq);
		} else {
			ret_val = -1;
		}
	}

	return ret_val;
}


/* set_n_frames - must tell both the MCE and the DSP about the number
 * of frames to expect. */

static int set_n_frames(mce_acq_t *acq, int n_frames, int dsp_only)
{
	int ret_val;
    uint32_t args[2];

    // Inform DSP/driver
    if (mcedata_set_nframes(acq->context, n_frames)) {
        mcelib_error(acq->context, "Failed to set quiet transfer interval!\n");
        return -MCE_ERR_DEVICE;
    }
    if (dsp_only)
        return 0;

   // Write to cc ret_dat_s
 	args[0] = 0;
	args[1] = n_frames - 1;
	ret_val = mcecmd_write_block(acq->context, &acq->ret_dat_s, 2, args);
	if (ret_val != 0) {
        mcelib_warning(acq->context, "Could not set ret_dat_s! [%#x]\n",
                ret_val);
		acq->last_n_frames = -1;
	} else {
		acq->last_n_frames = n_frames;
	}

	return ret_val;
}


static int get_n_frames(mce_acq_t *acq)
{
	int ret_val;
    uint32_t args[2];
	ret_val = mcecmd_read_block(acq->context, &acq->ret_dat_s, 2, args);
	if (ret_val != 0) {
        mcelib_error(acq->context, "Error reading ret_dat_s: '%s'",
                mcelib_error_string(ret_val));
		return -1;
	}
	return args[1] - args[0] + 1;
}


static int load_frame_params(mce_acq_t *acq, int cards)
{
	/* Determine frame size parameters
	      acq->rows     number of rows returning data
	      acq->cols     number of columns returning data
	      acq->cards    bit-mask (MCEDATA_RC?) of cards returning data
	      acq->n_cards  number of cards returning data
	      acq->row0     per-card values of first row reporting data
	      acq->col0     per-card values of first col reporting data

	   The acq->cards parameter is determined as follows: if the
	   function argument "cards" is positive, it is ANDed with
	   MCEDATA_RCS and stored in acq->cards.  If "cards" is 0 or
	   negative, acq->cards is determined by reading the
	   "rcs_to_report_data" register and recasting the result into
	   the MCEDATA_RC? bit-mask form.
	*/
    mce_context_t *context = acq->context;
	mce_param_t para_nrow, para_ncol, para_0, para_rcs;
    uint32_t data[64];
	int fw_rectangle = 0;         //firmware supports rectangle readout
	int rcs_cards = 0;            //cards that rcs would return.
	int ret_val = 0;
	int i;

	/* Load MCE parameters to determine fw_* supported by this firmware */
	if (mcecmd_load_param(context, &para_nrow, "cc",
			      "num_rows_reported") != 0) {
		return -MCE_ERR_FRAME_ROWS;
	}
	if (mcecmd_load_param(context, &para_ncol, "cc",
			      "num_cols_reported") == 0) {
		fw_rectangle = 1;
	}
	if ((ret_val=mcecmd_load_param(context, &para_rcs, "cc",
                  "rcs_to_report_data")) == 0) {
        if (mcecmd_read_block(context, &para_rcs, 1, data) != 0)
            return -MCE_ERR_FRAME_DEVICE; // Close enough
        rcs_cards = rcsflags_to_cards((int)data[0]);
    } else
        rcs_cards = MCEDATA_RCS; // Old firmware default.

    /* Validate card selection, set acq->cards and acq->n_cards. */
	if (cards <= 0) {
        /* Assume RCS. */
		acq->cards = rcs_cards;
        acq->n_cards = card_count(acq->cards);
	} else {
        acq->cards = cards & MCEDATA_RCS;
        acq->n_cards = card_count(acq->cards);
        /* Single card is ok; otherwise insist on RCS match. */
        if (acq->n_cards != 1 && acq->cards != rcs_cards) {
            mcelib_error(acq->context, "Invalid card set selection [%#x]\n",
                    cards);
            return -MCE_ERR_FRAME_CARD;
        }
	}

    /* Since we think we understand acq->cards, look up ret_dat. */
    if (load_ret_dat(acq) != 0) {
        mcelib_error(acq->context,
                "Failed to find ret_data for card selection [%#x]\n", cards);
        return -MCE_ERR_FRAME_CARD;
    }

	/* Determine cols and rows reported */
	acq->cols = MCEDATA_COLUMNS;
	if (fw_rectangle) {
		if (mcecmd_read_block(context, &para_ncol, 1, data) != 0)
			return -MCE_ERR_FRAME_COLS;
		acq->cols = (int)data[0];
	}
	if (mcecmd_read_block(context, &para_nrow, 1, data) != 0)
		return -MCE_ERR_FRAME_ROWS;
	acq->rows = (int)data[0];

	// Load the row and column starting indices (for, e.g. dirfile field naming)
	for (i=0; i<MCEDATA_CARDS; i++) {
		char rc[4];
		if (!(acq->cards & (1<<i))) 
			continue;
		sprintf(rc, "rc%i", i+1);
		acq->row0[i] = 0;
		acq->col0[i] = 0;
		if ((mcecmd_load_param(context, &para_0, rc,
				       "readout_row_index")==0) &&
		    (mcecmd_read_block(context, &para_0, 1, data)==0))
			acq->row0[i] = data[0];
		if (!fw_rectangle)
			continue;
		if ((mcecmd_load_param(context, &para_0, rc,
				       "readout_col_index")==0) &&
		    (mcecmd_read_block(context, &para_0, 1, data)==0))
			acq->col0[i] = data[0];
	}
	return 0;
}


static int load_data_params(mce_acq_t *acq)
{
	/* Determine data content parameters
	      acq->data_mode  per-card data_mode settings
	      (eventually, full rectangle mode support will go here)
	*/
	mce_param_t para;
    uint32_t data[64];
	int i;

	// Load data_modes from each card.
	for (i=0; i<MCEDATA_CARDS; i++) {
		char rc[4];
		if (!(acq->cards & (1<<i))) 
			continue;
		sprintf(rc, "rc%i", 1+i);
		acq->data_mode[i] = 0;
		if ((mcecmd_load_param(acq->context, &para, rc,
				  "data_mode")==0) &&
		    (mcecmd_read_block(acq->context, &para, 1, data)==0))
			acq->data_mode[i] = data[0];
		else {
			acq->data_mode[i] = -1;
			//fprintf(stderr, "Failed to determine data_mode.\n");
		}
	}
	return 0;
}


int load_ret_dat(mce_acq_t *acq)
{
	/* Return value is non-zero on error, but is not an mcelib error
     * code!  Note this can't do error checking on multi-card
     * selections; but validity (compared with rcs_to_report_data) has
     * already been checked.  So this just assumes "rcs" for
     * any multi-card case.
     */

	// Lookup the go command location for the specified card set.
	switch (acq->cards) {
	case MCEDATA_RC1:
		return mcecmd_load_param(acq->context, &acq->ret_dat, "rc1", "ret_dat");
	case MCEDATA_RC2:
		return mcecmd_load_param(acq->context, &acq->ret_dat, "rc2", "ret_dat");
	case MCEDATA_RC3:
		return mcecmd_load_param(acq->context, &acq->ret_dat, "rc3", "ret_dat");
	case MCEDATA_RC4:
		return mcecmd_load_param(acq->context, &acq->ret_dat, "rc4", "ret_dat");
	}
    
    // Multi-card? Return RCS.
    return mcecmd_load_param(acq->context, &acq->ret_dat, "rcs", "ret_dat");
}


#define COPY_FRAMES_SLEEP_US 1000

int copy_frames_mmap(mce_acq_t *acq)
{
	int ret_val = 0;
	int done = 0;
	int count = 0;
    uint32_t *data;

    int max_waits = (acq->timeout_ms * 1000 + COPY_FRAMES_SLEEP_US/2)  /
        COPY_FRAMES_SLEEP_US;
    if (max_waits == 0 && acq->timeout_ms > 0)
        max_waits = 1;

	acq->n_frames_complete = 0;

	/* memmap loop */
	while (!done) {

        int waits = 0;
		while (!done) {
            if (mcedata_poll_offset(acq->context, &ret_val) != 0)
               break;
            usleep(COPY_FRAMES_SLEEP_US);
            if (max_waits > 0 && ++waits >= max_waits)
                done = EXIT_TIMEOUT;
        }
        if (done)
            break;

		if (acq->storage->pre_frame != NULL &&
            acq->storage->pre_frame(acq) != 0) {
            mcelib_warning(acq->context, "pre_frame action failed\n");
		}

		// New frame at offset ret_val
		data = acq->context->data.map + ret_val;

		// Logical formatting
		sort_columns( acq, data );

		if ( (acq->storage->post_frame != NULL) &&
		     acq->storage->post_frame( acq, count, data ) ) {
            mcelib_warning(acq->context, "post_frame action failed\n");
		}

		if (++count >= acq->n_frames)
			done = EXIT_COUNT;

		if (frame_property(data, &frame_header_v6, status_v6)
		    & FRAME_STATUS_V6_STOP)
			done = EXIT_STOP;

		if (frame_property(data, &frame_header_v6, status_v6)
		    & FRAME_STATUS_V6_LAST)
			done = EXIT_LAST;

		// Inform driver of consumption
		mcedata_consume_frame(acq->context);
	}

	switch (done) {
	case EXIT_COUNT:
	case EXIT_LAST:
		acq->status = MCEDATA_IDLE;
		break;

	case EXIT_TIMEOUT:
		acq->status = MCEDATA_TIMEOUT;
		break;

	case EXIT_STOP:
		acq->status = MCEDATA_STOP;
		break;

	case EXIT_READ:
	case EXIT_WRITE:
	case EXIT_EOF:
    case EXIT_KILL:
	default:
		acq->status = MCEDATA_ERROR;
		break;
	}

	acq->n_frames_complete = count;

	return 0;
}


int card_count(int cards)
{
	int n = 0;
	if (cards & MCEDATA_RC1) n++;
	if (cards & MCEDATA_RC2) n++;
	if (cards & MCEDATA_RC3) n++;
	if (cards & MCEDATA_RC4) n++;
	return n;
}

/*
static int cards_to_rcsflags(int c)
{
	//Sure, there are cuter ways.
	int out = 0;
	if (c & MCEDATA_RC1) out |= MCEDATA_RCSFLAG_RC1;
	if (c & MCEDATA_RC2) out |= MCEDATA_RCSFLAG_RC2;
	if (c & MCEDATA_RC3) out |= MCEDATA_RCSFLAG_RC3;
	if (c & MCEDATA_RC4) out |= MCEDATA_RCSFLAG_RC4;
	return out;
}
*/

static int rcsflags_to_cards(int c)
{
	//Sure, there are cuter ways.
	int out = 0;
	if (c & MCEDATA_RCSFLAG_RC1) out |= MCEDATA_RC1;
	if (c & MCEDATA_RCSFLAG_RC2) out |= MCEDATA_RC2;
	if (c & MCEDATA_RCSFLAG_RC3) out |= MCEDATA_RC3;
	if (c & MCEDATA_RCSFLAG_RC4) out |= MCEDATA_RC4;
	return out;
}

int mcelib_symlink(const char *newpath, const char *target)
{
    int err = 0;
    char *tmp;

    if (newpath == NULL || newpath[0] == 0)
        return 0;

    size_t l = strlen(newpath);

    tmp = malloc(l + 7);

    /* mktemp loop */
    do {
        strcpy(tmp, newpath);
        strcpy(tmp + l, "XXXXXX");

        /* glibc-2.15 and earlier incorrectly mark mktemp with __wur [#13908] */
        __attribute__((unused)) char *stupid_glibc = mktemp(tmp);

        if (tmp[0] == 0)
            break;
        err = symlink(target, tmp);
    } while (err != 0 && errno == EEXIST);

    if (err != 0) {
        free(tmp);
        return 1;
    }

    /* move the temporary symlink into place */
    if (rename(tmp, newpath)) {
        unlink(tmp);
        free(tmp);
        return 1;
    }
    
    free(tmp);
    return 0;
}
#endif
