/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#define _GNU_SOURCE

/* File sequencing, flat file, ram-based data handling modules */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "context.h"

// #define FILEOPS_BASIC

#ifdef FILEOPS_BASIC
#define FILE_OPEN(X, filename) X->fd = open(filename, O_WRONLY | O_CREAT)
#define FILE_CLOSE(X) close(X->fd)
#define FILE_CLEAR(X) X->fd=-1
#define FILE_WRITE(X, data, size) write(X->fd, data, size)
#define FILE_OK(X) (X->fd >= 0)
#define FILE_FLUSH(X) X = X;
#else
#define FILE_OPEN(X, filename) X->fout = fopen(filename, "a");
#define FILE_CLOSE(X) fclose(X->fout)
#define FILE_CLEAR(X) X->fout=NULL
#define FILE_WRITE(X, data, size) fwrite(data, size, 1, X->fout)
#define FILE_OK(X) (X->fout != NULL)
#define FILE_FLUSH(X) fflush(X->fout)
#endif

/* Generic destructor (not to be confused with cleanup member function) */

static int storage_destructor(mcedata_storage_t *storage)
{
	// Free the private data for the storage module, clear the structure
	if (storage->action_data != NULL)
		free(storage->action_data);
	memset(storage, 0, sizeof(*storage));
	return 0;
}


/* File sequencing structure and operations */

typedef struct fileseq_struct {
	char filename[MCE_LONG];
	FILE *fout;
	char basename[MCE_LONG];
        const char *symlink;
	int interval;
	int digits;
	int next_switch;
	int frame_count;
	char format[MCE_LONG];
} fileseq_t;

static int fileseq_cycle(mce_acq_t *acq, fileseq_t *f, int this_frame)
{
	int new_idx = this_frame / f->interval;
	if (f->fout != NULL)
		fclose(f->fout);

	sprintf(f->filename, f->format, new_idx);
	f->fout = fopen64(f->filename, "a");

	if (f->fout == NULL) {
		sprintf(acq->errstr, "Failed to open file '%s'", f->filename);
		return -1;
	}

        /* Update the indirection, maybe */
        mcelib_symlink(f->symlink, f->filename);

	return 0;
}

static int fileseq_init(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->storage->action_data;
	fileseq_cycle(acq, f, 0);

	return 0;
}

static int fileseq_cleanup(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->storage->action_data;
	if (f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
	}
	
	return 0;
}
static int fileseq_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	fileseq_t *f = (fileseq_t*)acq->storage->action_data;

	// Don't use the provided counter, it counts only relative to this 'go'
	if (f->frame_count++ >= f->next_switch) {
		f->next_switch += f->interval;
		if (fileseq_cycle(acq, f, f->frame_count)) {
			return -1;
		}
	}	
	
	if (f->fout == NULL) return -1;
	fwrite(data, acq->frame_size*sizeof(*data), 1, f->fout);

	return 0;
}

static int fileseq_flush(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->storage->action_data;
	if (f->fout != NULL)
		fflush(f->fout);
	return 0;
}

mcedata_storage_t fileseq_actions = {
	.init = fileseq_init,
	.cleanup = fileseq_cleanup,
	.post_frame = fileseq_post,
	.flush = fileseq_flush,
	.destroy = storage_destructor,
};


/* Flat file structure and operations */

typedef struct flatfile_struct {

	char filename[MCE_LONG];
        const char *symlink;
	int frame_size;
#ifdef FILEOPS_BASIC
	int fd;
#else
 	FILE *fout;
#endif

} flatfile_t;


static int flatfile_init(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->storage->action_data;
	if (!FILE_OK(f)) {
		FILE_OPEN(f, f->filename);
		if (!FILE_OK(f)) {
			sprintf(acq->errstr, "Failed to open file '%s'", f->filename);
			return -1;
		}
	}

        /* Update the indirection, maybe */
        mcelib_symlink(f->symlink, f->filename);

	return 0;
}

static int flatfile_cleanup(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->storage->action_data;
	if (FILE_OK(f)) {
		FILE_CLOSE(f);
		FILE_CLEAR(f);
	}
	f->filename[0] = 0;

	return 0;
}

static int flatfile_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	flatfile_t *f = (flatfile_t*)acq->storage->action_data;
	if (!FILE_OK(f)) return -1;
        FILE_WRITE(f, data, acq->frame_size*sizeof(*data));
	return 0;
}

static int flatfile_flush(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->storage->action_data;
	FILE_FLUSH(f);
/* 	if (f->fd  != NULL) */
/* 		fflush(f->fout); */
	return 0;
}

mcedata_storage_t flatfile_actions = {
	.init = flatfile_init,
	.cleanup = flatfile_cleanup,
	.pre_frame = NULL,
	.post_frame = flatfile_post,
	.flush = flatfile_flush,
	.destroy = storage_destructor,
};


/* Ram buffer structure and operations */

typedef struct rambuff_struct {

	int frame_size;
	u32 *buffer;
	
	unsigned long user_data;
	rambuff_callback_t callback;

} rambuff_t;


static int rambuff_init(mce_acq_t *acq)
{
	rambuff_t *f = (rambuff_t*)acq->storage->action_data;
	int b_size = acq->frame_size*sizeof(f->buffer);
	if (f->buffer != NULL) free(f->buffer);
	f->buffer = (u32*) malloc(b_size);
	if (f->buffer == NULL) {
		sprintf(acq->errstr, "rambuff could not allocate %i bytes", b_size);
		return -1;
	}
	return 0;
}

static int rambuff_cleanup(mce_acq_t *acq)
{
	rambuff_t *f = (rambuff_t*)acq->storage->action_data;
	if (f->buffer != NULL) free(f->buffer);
	f->buffer = NULL;

	return 0;
}

static int rambuff_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	rambuff_t *f = (rambuff_t*)acq->storage->action_data;

	if (f->callback != NULL) {
		f->callback(f->user_data, acq->frame_size, data);
	}

	return 0;
}

mcedata_storage_t rambuff_actions = {
	.init = rambuff_init,
	.cleanup = rambuff_cleanup,
	.pre_frame = NULL,
	.post_frame = rambuff_post,
	.destroy = storage_destructor,
};






/* Construction routines:
  - allocate/create the private data
  - store the pointer and fill out the operations in the acq structure
*/


mcedata_storage_t* mcedata_flatfile_create(const char *filename,
                                           const char *symlink)
{
	flatfile_t *f = (flatfile_t*)malloc(sizeof(flatfile_t));
	mcedata_storage_t *storage = (mcedata_storage_t*)malloc(sizeof(mcedata_storage_t));
	if (f==NULL || storage==NULL) return NULL;

	//Initialize storage with the file operations, then set local data.
	memcpy(storage, &flatfile_actions, sizeof(flatfile_actions));
	storage->action_data = f;

	memset(f, 0, sizeof(*f));
	FILE_CLEAR(f);

        if (symlink!=NULL && *symlink)
            f->symlink = symlink;
        else
            f->symlink = NULL;

	strcpy(f->filename, filename);
	return storage;
}

mcedata_storage_t* mcedata_storage_destroy(mcedata_storage_t *storage)
{
	// Since we don't have access to mce_acq_t, we can't call the
	// cleanup function.  But we can call the destroy function...

	if (storage != NULL && storage->destroy != NULL)
		storage->destroy(storage);

	free(storage);
	return NULL;
}

/* Destructors by generic destructor which calls the cleanup routine
   to do any storage specific cleanup. */
/*
void mcedata_flatfile_destroy(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->storage->action_data;
	memset(&(acq->actions), 0, sizeof(acq->actions));

	if ( f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
		f->filename[0] = 0;
	}
	free(f);
	acq->storage = 0;
}
*/

mcedata_storage_t* mcedata_fileseq_create(const char *basename, int interval,
                                          int digits, const char *symlink)
{
	fileseq_t *f = (fileseq_t*)malloc(sizeof(fileseq_t));
	mcedata_storage_t *storage = (mcedata_storage_t*)malloc(sizeof(mcedata_storage_t));
	if (f==NULL || storage==NULL) return NULL;

	//Initialize storage with the file operations, then set local data.
	memcpy(storage, &fileseq_actions, sizeof(fileseq_actions));
	storage->action_data = f;

	memset(f, 0, sizeof(*f));

	strcpy(f->basename, basename);
	f->digits = digits;

	// Produce format like "basename.%03i"
	sprintf(f->format, "%s.%%0%ii", f->basename, f->digits);

	f->interval = interval;
	
        if (symlink!=NULL && *symlink)
            f->symlink = symlink;
        else
            f->symlink = NULL;

	return storage;
}

/*      
void mcedata_fileseq_destroy(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->storage->action_data;
	memset(&(acq->actions), 0, sizeof(acq->actions));

	if ( f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
		f->filename[0] = 0;
	}
	free(f);
	acq->storage = 0;
}
*/

mcedata_storage_t* mcedata_rambuff_create(rambuff_callback_t callback,
					  unsigned long user_data)
{
	rambuff_t *f = (rambuff_t*)malloc(sizeof(rambuff_t));
	mcedata_storage_t *storage = (mcedata_storage_t*)malloc(sizeof(mcedata_storage_t));
	if (f==NULL || storage==NULL) return NULL;

	//Initialize storage with the file operations, then set local data.
	memcpy(storage, &rambuff_actions, sizeof(rambuff_actions));
	storage->action_data = f;

	memset(f, 0, sizeof(*f));

	f->user_data = user_data;
	f->callback = callback;

	return storage;
}

/*      
void mcedata_rambuff_destroy(mce_acq_t *acq)
{
	rambuff_t *f = (rambuff_t*)acq->storage->action_data;
	free(f);

	memset(&(acq->actions), 0, sizeof(acq->actions));
	acq->storage = 0;
}
*/
