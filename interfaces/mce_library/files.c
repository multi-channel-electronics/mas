#define _GNU_SOURCE

/* File sequencing, flat file, ram-based data handling module */

#include <stdlib.h>
#include <string.h>

#include "mcecmd.h"
#include "mcedata.h"


/* File sequencing structure and operations */

typedef struct fileseq_struct {
	char filename[MCE_LONG];
	FILE *fout;
	char basename[MCE_LONG];
	int interval;
	int digits;
	int next_switch;
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

	return 0;
}

static int fileseq_init(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->action_data;

	fileseq_cycle(acq, f, 0);

	return 0;
}
static int fileseq_cleanup(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->action_data;
	if (f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
	}
	
	return 0;
}
static int fileseq_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	fileseq_t *f = (fileseq_t*)acq->action_data;

	if (frame_index >= f->next_switch) {
		f->next_switch += f->interval;
		if (fileseq_cycle(acq, f, frame_index)) {
			return -1;
		}
	}	
	
	if (f->fout == NULL) return -1;
	fwrite(data, acq->frame_size*sizeof(*data), 1, f->fout);

	return 0;
}

static int fileseq_flush(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->action_data;
	if (f->fout != NULL)
		fflush(f->fout);
	return 0;
}

mce_frame_actions_t fileseq_actions = {
	.init = fileseq_init,
	.cleanup = fileseq_cleanup,
	.post_frame = fileseq_post,
	.flush = fileseq_flush,
};


/* Flat file structure and operations */

typedef struct flatfile_struct {

	char filename[MCE_LONG];
	int frame_size;
	FILE *fout;

} flatfile_t;


static int flatfile_init(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->action_data;
	if (f->fout == NULL) {
		f->fout = fopen64(f->filename, "a");
		if (f->fout == NULL) {
			sprintf(acq->errstr, "Failed to open file '%s'", f->filename);
			return -1;
		}
	}
	return 0;
}

static int flatfile_cleanup(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->action_data;
	if (f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
	}
	f->filename[0] = 0;

	return 0;
}

static int flatfile_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	flatfile_t *f = (flatfile_t*)acq->action_data;

	if (f->fout==NULL) return -1;

	fwrite(data, acq->frame_size*sizeof(*data), 1, f->fout);

	return 0;
}

static int flatfile_flush(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->action_data;
	if (f->fout != NULL)
		fflush(f->fout);
	return 0;
}

mce_frame_actions_t flatfile_actions = {
	.init = flatfile_init,
	.cleanup = flatfile_cleanup,
	.pre_frame = NULL,
	.post_frame = flatfile_post,
	.flush = flatfile_flush,
};


/* Ram buffer structure and operations */

typedef struct rambuff_struct {

	int frame_size;
	u32 *buffer;
	
	unsigned user_data;
	rambuff_callback_t callback;

} rambuff_t;


static int rambuff_init(mce_acq_t *acq)
{
	rambuff_t *f = (rambuff_t*)acq->action_data;
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
	rambuff_t *f = (rambuff_t*)acq->action_data;
	if (f->buffer != NULL) free(f->buffer);
	f->buffer = NULL;

	return 0;
}

static int rambuff_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	rambuff_t *f = (rambuff_t*)acq->action_data;

	if (f->callback != NULL) {
		f->callback(f->user_data, acq->frame_size, data);
	}

	return 0;
}

mce_frame_actions_t rambuff_actions = {
	.init = rambuff_init,
	.cleanup = rambuff_cleanup,
	.pre_frame = NULL,
	.post_frame = rambuff_post,
};






/* Construction routines:
  - allocate/create the private data
  - store the pointer and fill out the operations in the acq structure
*/


int mcedata_flatfile_create(mce_acq_t *acq, const char *filename)
{
	flatfile_t *flat = (flatfile_t*)malloc(sizeof(flatfile_t));
	if (flat==0) return -1;

	strcpy(flat->filename, filename);

	memcpy(&(acq->actions), &flatfile_actions, sizeof(flatfile_actions));
	acq->action_data = (unsigned long)flat;

	return 0;
}
      
void mcedata_flatfile_destroy(mce_acq_t *acq)
{
	flatfile_t *f = (flatfile_t*)acq->action_data;
	memset(&(acq->actions), 0, sizeof(acq->actions));

	if ( f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
		f->filename[0] = 0;
	}
	free(f);
	acq->action_data = 0;
}


int mcedata_fileseq_create(mce_acq_t *acq, const char *basename,
			   int interval, int digits)
{
	fileseq_t *f = (fileseq_t*)malloc(sizeof(fileseq_t));
	if (f==NULL) return -1;

	strcpy(f->basename, basename);
	f->digits = digits;

	sprintf(f->format, "%s.%%0%ii", f->basename, f->digits);

	f->interval = interval;
	f->next_switch = 0;
	
	memcpy(&(acq->actions), &fileseq_actions, sizeof(fileseq_actions));
	acq->action_data = (unsigned long)f;

	return 0;
}
      
void mcedata_fileseq_destroy(mce_acq_t *acq)
{
	fileseq_t *f = (fileseq_t*)acq->action_data;
	memset(&(acq->actions), 0, sizeof(acq->actions));

	if ( f->fout != NULL) {
		fclose(f->fout);
		f->fout = NULL;
		f->filename[0] = 0;
	}
	free(f);
	acq->action_data = 0;
}


int mcedata_rambuff_create(mce_acq_t *acq, rambuff_callback_t callback,
			   unsigned user_data)
{
	rambuff_t *f = (rambuff_t*)malloc(sizeof(rambuff_t));
	if (f==NULL) return -1;

	f->user_data = user_data;
	f->callback = callback;

	memcpy(&(acq->actions), &rambuff_actions, sizeof(rambuff_actions));
	acq->action_data = (unsigned long)f;

	return 0;
}
      
void mcedata_rambuff_destroy(mce_acq_t *acq)
{
	rambuff_t *f = (rambuff_t*)acq->action_data;
	free(f);

	memset(&(acq->actions), 0, sizeof(acq->actions));
	acq->action_data = 0;
}
