/******************************************************
  mcedata.h - header file for MCE API, data module

  Matthew Hasselfield, 08.01.02

******************************************************/

#ifndef _MCEDATA_H_
#define _MCEDATA_H_

/*! \file mcedata.h
 *
 *  \brief Main header file for data module.
 *
 *  Contains routines for managing frame data acquisition (rather than
 *  just command/reply).
 *
 *  This module is intended to connect to an mce_data character driver
 *  device file.
 */


/* Module information structure */

typedef struct mcedata {

	int connected;
	int fd;

	char dev_name[MCE_LONG];
	char errstr[MCE_LONG];

} mcedata_t;


/* MCE data acquisition modes. */

#define MCEDATA_FILESTREAM        (1 <<  0) /* output to file */
#define MCEDATA_INCREMENT         (1 <<  1) /* keep ret_data_s up to date */
#define MCEDATA_FILESEQUENCE      (1 <<  2) /* switch output files regularly */
#define MCEDATA_THREAD            (1 <<  3) /* use non-blocking data thread */


/* MCE data acquisition statuses. */

#define MCEDATA_IDLE               0
#define MCEDATA_TIMEOUT            1
#define MCEDATA_STOP               2
#define MCEDATA_ERROR              3


/* MCE card bits - fix this! */

#define MCEDATA_RC1               (1 <<  0)
#define MCEDATA_RC2               (1 <<  1)
#define MCEDATA_RC3               (1 <<  2)
#define MCEDATA_RC4               (1 <<  3)
#define MCEDATA_RCS               (MCEDATA_RC1 | MCEDATA_RC2 | MCEDATA_RC3 | MCEDATA_RC4)

#define MCEDATA_CARDS             4
#define MCEDATA_COMBOS            (1 << MCEDATA_CARDS)

struct mce_acq_struct;
typedef struct mce_acq_struct mce_acq_t;

typedef struct mce_frame_actions_t {

	int (*init)(mce_acq_t*);
	int (*pre_frame)(mce_acq_t *);
	int (*flush)(mce_acq_t *);
	int (*post_frame)(mce_acq_t *, int, u32 *);
	int (*cleanup)(mce_acq_t *);

} mce_frame_actions_t;

struct mce_acq_struct {

	mce_context_t *context;

	int n_frames;
	int n_frames_complete;
	int frame_size;                 // Active frame size

	int status;
	int cards;

	mce_frame_actions_t actions;
	unsigned long action_data;

	char errstr[MCE_LONG];

	int timeout_ms;
 	int options;

	/* Cached mce info */
	int know_ret_dat;
	int know_ret_dat_s;
	
	mce_param_t ret_dat;
	mce_param_t ret_dat_s;
	int last_n_frames;
		
};


#define MCEDATA_PACKET_MAX 4096 /* Maximum frame size in dwords */


/* Data connection */

int mcedata_open(mce_context_t *context, const char *dev_name);
int mcedata_close(mce_context_t *context);


/* ioctl access to driver */

int mcedata_ioctl(mce_context_t* context, int key, unsigned long arg);
int mcedata_set_datasize(mce_context_t* context, int datasize);
int mcedata_empty_data(mce_context_t* context);
int mcedata_fake_stopframe(mce_context_t* context);
int mcedata_qt_enable(mce_context_t* context, int on);
int mcedata_qt_setup(mce_context_t* context, int frame_index);


/* Frame data handlers */

/* rambuff: a user-defined callback routine services each frame */

typedef int (*rambuff_callback_t)(unsigned user_data,
				  int frame_size, u32 *buffer);

int mcedata_rambuff_create(mce_acq_t *acq, rambuff_callback_t callback,
			   unsigned user_data);

void mcedata_rambuff_destroy(mce_acq_t *acq);


/* flatfile: frames are stored in a single data file */

int mcedata_flatfile_create(mce_acq_t *acq, const char *filename);

void mcedata_flatfile_destroy(mce_acq_t *acq);


/* fileseq: frames are stored in a set of files, numbered sequentially */

int mcedata_fileseq_create(mce_acq_t *acq, const char *basename,
			   int interval, int digits);

void mcedata_fileseq_destroy(mce_acq_t *acq);


/* Acquisition sessions - once the mce_acq_t is ready, setup the
   acquisition and go. */

int mcedata_acq_setup(mce_acq_t *acq, mce_context_t *context,
		      int options, int cards, int rows_reported);

int mcedata_acq_go(mce_acq_t *acq, int n_frames);



#endif
