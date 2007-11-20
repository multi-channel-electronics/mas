/***************************************************
  mcedata.h - header file for MCE DATA ACQ API

  Matthew Hasselfield, 07.10.28

***************************************************/

#ifndef _MCEDATA_H_
#define _MCEDATA_H_

/*! \file mcedata.h
 *  \brief Data acq header file for libmce
 *
 *  Contains routines for managing frame data acquisition (rather than
 *  just command/reply)
 *
 */

#include <stdio.h>

#include <mce.h>
#include <mce_errors.h>
#include <mcecmd.h>


/* Modes:
    - auto-save to file via a blocking call
    - auto-save to file via data thread? (later...)
    - data available to user buffer on request (user's own thread)
*/

/* Status:
    - State: Idle, ready, going, done
    - Frames available/ no frames available
    - Error / no error.
*/


/* MCE data acquisition modes. */

#define MCEDATA_FILESTREAM        (1 <<  0) /* output to file */
#define MCEDATA_INCREMENT         (1 <<  1) /* keep ret_data_s up to date */
#define MCEDATA_FILESEQUENCE      (1 <<  2) /* switch output files regularly */
#define MCEDATA_THREAD            (1 <<  3) /* use non-blocking data thread */

/* MCE card bits, why not? */

#define MCEDATA_RC1               (1 <<  0)
#define MCEDATA_RC2               (1 <<  1)
#define MCEDATA_RC3               (1 <<  2)
#define MCEDATA_RC4               (1 <<  3)

#define MCEDATA_CARDS             4
#define MCEDATA_COMBOS            (1 << MCEDATA_CARDS)

/*struct {
	char name[MCE_SHORT];
	unsigned mask;
	int card_id;
	};*/


struct mce_acq_struct;
typedef struct mce_acq_struct mce_acq_t;

typedef struct mce_frame_actions_t {
	int (*init)(mce_acq_t*);
	int (*pre_frame)(mce_acq_t *);
	int (*flush)(mce_acq_t *);
	int (*post_frame)(mce_acq_t *, int, u32 *);
	int (*cleanup)(mce_acq_t *);
} mce_frame_actions_t;

typedef struct {

	char dev_name[MCE_LONG];
	int fd;
	int mcecmd_handle;

//	logger_t logger;
	char errstr[MCE_LONG];

} mcedata_t;

struct mce_acq_struct {

	mcedata_t *mcedata;

	int n_frames;
	int frame_size;                 // Active frame size

/* 	char filename[1024];            // Filename of current data file, if any. */
/* 	FILE *fout;                     // File pointer of current data file. */

	int status;
	int cards;

	mce_frame_actions_t actions;
	unsigned long action_data;

 	int options;
};


#define MCEDATA_PACKET_MAX 4096 /* Maximum frame size in dwords */


/*
   API: all functions return a negative error value on failure.  On
   success.

   Two parts: generic interfacing with data device (replaces many
   parts of mcecmd that deal with frame size, etc.), and acquisition
   managment (file management, automatic timeout/inform configuration,
   etc.)

*/


/* Operations for prep steps (args: frame_size, total frames, increment mode)

 - set total_frames, but not frames_per_go, which is acq specific.
 - set frame size in driver, DSP
 - set timeouts in DSP (but not inform)
 - initialize ret_dat_s or internal counter, depending on increment mode.
 - prepare output file, if necessary

*/

/* Operations for go (args: frames, cards)

 - set up inform interval in DSP
 - update MCE ret_dat_s if in that increment mode
 - issue MCE go
 - if outputting to file, dump the data.
 - exit and report success code

*/


/* char *mce_error_string(int error); */

int mcedata_acq_reset(mce_acq_t *acq, mcedata_t *mcedata);

int mcedata_acq_setup(mce_acq_t *acq, int options, int cards, int frame_size);

int mcedata_acq_go(mce_acq_t *acq, int n_frames);


/* Frame data handlers */

int mcedata_flatfile_create(mce_acq_t *acq, const char *filename);
void mcedata_flatfile_destroy(mce_acq_t *acq);

void mcedata_fileseq_destroy(mce_acq_t *acq);
int mcedata_fileseq_create(mce_acq_t *acq, const char *basename,
			   int interval, int digits);

/* Data connection */

int mcedata_init(mcedata_t *mcedata, int mcecmd_handle, const char *dev_name);


/* REPATRIATED FROM MCECMD */

/* Ioctl related - note argument is fd, not handle (fixme) */

int mcedata_ioctl(mcedata_t *mcedata, int key, unsigned long arg);
int mcedata_set_datasize(mcedata_t *mcedata, int datasize);
int mcedata_empty_data(mcedata_t *mcedata);
int mcedata_fake_stopframe(mcedata_t *mcedata);
int mcedata_qt_enable(mcedata_t *mcedata, int on);
int mcedata_qt_setup(mcedata_t *mcedata, int frame_index);

#endif
