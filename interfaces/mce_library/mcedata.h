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


/* MCE card bits, why not? */

#define MCEDATA_RC1               (1 <<  0)
#define MCEDATA_RC2               (1 <<  1)
#define MCEDATA_RC3               (1 <<  2)
#define MCEDATA_RC4               (1 <<  3)

typedef struct {

	char dev_name[MCE_LONG];
	int fd;
	int mcecmd_handle;

//	logger_t logger;
	char errstr[MCE_LONG];

} mcedata_t;

typedef struct {

	mcedata_t *mcedata;

	int n_frames;
	int frame_size;
	char filename[1024];
	
	FILE *fout;

	int status;
	int cards;
	int options;
//	u32 *buffer;

} mce_acq_t;


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

int mcedata_acq_setup(mce_acq_t *acq, int options, int cards, int frame_size,
		      const char *filename);

int mcedata_acq_go(mce_acq_t *acq, int n_frames);


/* Data connection */

int mcedata_init(mcedata_t *mcedata, int mcecmd_handle, const char *dev_name);


/* REPATRIATED FROM MCECMD */

/* Ioctl related - note argument is fd, not handle (fixme) */

int mcedata_ioctl(mcedata_t *mcedata, int key, unsigned long arg);
int mcedata_framesize(mcedata_t *mcedata, int datasize);
int mcedata_clear(mcedata_t *mcedata);
int mcedata_fakestop(mcedata_t *mcedata);

int mcedata_qt_setup(mcedata_t *mcedata, int frame_index);
int mcedata_qt_enable(mcedata_t *mcedata, int on);


#endif
