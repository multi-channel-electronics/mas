/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
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

#include <mce/frame.h>
#include <mce/acq.h>
#include <mce/data_mode.h>

/* Data connection */

int mcedata_open(mce_context_t *context);
int mcedata_close(mce_context_t *context);


/* ioctl access to driver */

int mcedata_ioctl(mce_context_t* context, int key, unsigned long arg);
int mcedata_set_datasize(mce_context_t* context, int datasize);
int mcedata_set_nframes(mce_context_t* context, int frame_count);
int mcedata_empty_data(mce_context_t* context);
int mcedata_fake_stopframe(mce_context_t* context);
void mcedata_buffer_query(mce_context_t* context, int *head, int *tail,
			  int *count);
int mcedata_poll_offset(mce_context_t* context, int *offset);
int mcedata_consume_frame(mce_context_t* context);
int mcedata_lock_query(mce_context_t* context);
int mcedata_lock_reset(mce_context_t* context);
int mcedata_lock_down(mce_context_t* context);
int mcedata_lock_up(mce_context_t* context);


/* Frame data handlers */

/* rambuff: a user-defined callback routine services each frame */

typedef int (*rambuff_callback_t)(unsigned long user_data,
				  int frame_size, u32 *buffer);

mcedata_storage_t* mcedata_rambuff_create(rambuff_callback_t callback,
					  unsigned long user_data);

//void mcedata_rambuff_destroy(mce_acq_t *acq);


/* flatfile: frames are stored in a single data file */

mcedata_storage_t* mcedata_flatfile_create(const char *filename,
                                           const char *symlink);

//void mcedata_flatfile_destroy(mce_acq_t *acq);


/* fileseq: frames are stored in a set of files, numbered sequentially */

mcedata_storage_t* mcedata_fileseq_create(const char *basename, int interval,
                                          int digits, const char *symlink);

//void mcedata_fileseq_destroy(mce_acq_t *acq);

/* generic destructor for mcedata_storage_t; it will be called automatically by mcedata_acq_destroy. */

mcedata_storage_t* mcedata_storage_destroy(mcedata_storage_t *storage);

/* Acquisition sessions - once the mce_acq_t is ready, setup the
   acquisition and go. */

mcedata_storage_t* mcedata_dirfile_create(const char *basename, int options,
    const char *include, int spf, int vers, const char *symlink);

mcedata_storage_t* mcedata_dirfileseq_create(const char *basename, int interval,
    int digits, int options, const char *include, int spf, int vers,
    const char *symlink);


/* multisync storage class -- container for multiple storage objects */

mcedata_storage_t* mcedata_multisync_create(int options);

int mcedata_multisync_add(mce_acq_t *multisync_acq,
                          mcedata_storage_t *sync);


int mcedata_acq_create(mce_acq_t* acq, mce_context_t* context,
		       int options, int cards, int rows_reported, 
		       mcedata_storage_t* storage);

int mcedata_acq_destroy(mce_acq_t *acq);

mce_acq_t *mcedata_acq_duplicate(mce_acq_t *acq);

int mcedata_acq_go(mce_acq_t *acq, int n_frames);



#endif
