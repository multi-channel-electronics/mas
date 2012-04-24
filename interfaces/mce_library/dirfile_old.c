/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#define _GNU_SOURCE

/* Dirfile storage module */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "context.h"

/* Method: data buffers for all channels. */

#define TESDATA_NAME   "tesdatar00c00"
#define TESDATA_FORMAT "tesdatar%02ic%02i"

#define DIRFILE_CHANNELS      (MCEDATA_CARDS*MCEDATA_COLUMNS*MCEDATA_ROWS)

typedef struct {
	u32 *data;
	int count;
	int turn_bias;
	int frame_offset;
	char *name;
	int free_on_destroy;
} channel_t;

typedef struct dirfile_struct {

	channel_t *channels;

	int data_size;

	int channel_count;

	int writes_per_frame;
	int flush;

	char basename[MCE_LONG];
	char format[MCE_LONG];

//	struct frame_header_abstraction frame_description;

} dirfile_t;

typedef struct {
	int offset;
	char *name;
} frame_item;

frame_item header_items[] = {
	// ACT naming scheme
	{  0,  "status" },
	{  1,  "frame_ctr" },
	{  2,  "row_len" },
	{  3,  "num_rows_reported" },
	{  4,  "data_rate" },
	{  5,  "address0_ctr" },
	{  6,  "header_version" },
	{  7,  "ramp_value" },
	{  8,  "ramp_addr" },
	{  9,  "num_rows" },
	{ 10,  "sync_box_num" },
	{ 11,  "runfile_id" },
	{ 12,  "userfield" },
	{ -1,  NULL }
};

/* The dirfile_t must already have been allocated.  This function
   allocates memory for the per-channel fields in the dirfile_t.  To
   do this it needs to know the number of fields, the maximum
   fieldname length, and the per-channel size of the data buffers. */

#define ALLOC_N(X, n, err)   X = malloc(n*sizeof(*X)); \
                             if (X==NULL) err |= -1; \
			     else memset(X, 0, n*sizeof(*X));
#define FREE_NOT_NULL(X)     if (X!=NULL) { free(X); X = NULL; }

static int dirfile_alloc(dirfile_t *d, int n, int fieldsize, int bufsize)
{
	int i;
	u32* base_data;
	char* base_names;
	int err = 0;

	// Surely all of these will succeed.
	ALLOC_N(d->channels, n, err);
	if (err) {
		fprintf(stderr, "First stage alloc failed in dirfile_alloc!\n");
		printf("n = %i\n", (int)(n*sizeof(*d->channels)));
		return -1;
	}

	// Large buffers
	ALLOC_N(base_data, n*bufsize, err);
	ALLOC_N(base_names, n*fieldsize, err);
	
	if (err) {
		fprintf(stderr, "Second stage alloc failed in dirfile_alloc!\n");
		return -1;
	}

	// Point
	for (i=0; i<n; i++) {
		d->channels[i].data = base_data + bufsize*i;
		d->channels[i].name = base_names + fieldsize*i;
		d->channels[i].free_on_destroy = (i==0);
	}
	
	return 0;
}


static int dirfile_free(dirfile_t *d)
{
	int i;

	// What am I, C++?
	for (i=0; i<d->channel_count; i++) {
		if (d->channels[i].free_on_destroy) {
			FREE_NOT_NULL(d->channels[i].data);
			FREE_NOT_NULL(d->channels[i].name);
		}
	}

	FREE_NOT_NULL(d->channels);
			     
	return 0;
}

/* Checks data buffers and calls fwrite on any that are more than
 * half-full.
 */

static int dirfile_write(mce_acq_t *acq, dirfile_t *f)
{
	int i;
	FILE *fout;
	int thresh = f->data_size / 2;
	char filename[MCE_LONG];
	int name_offset;

	strcpy(filename, f->basename);
	name_offset = strlen(filename);

	int writes = 0;
	for (i=0; i<f->channel_count; i++) {
		channel_t *c = f->channels + i;
		if (!f->flush && c->count + c->turn_bias < thresh)
			continue;
		strcpy(filename+name_offset, c->name);
		fout = fopen64(filename, "a");
		if (fout == NULL) continue;
		
		fwrite(c->data, c->count, sizeof(u32), fout);
		
		// If we are out-of-turn, use turn_bias to restore
		//  turn_bias = (count+turn_bias) - thresh
		c->turn_bias += c->count - thresh;
		c->count = 0;
		
		fclose(fout);
		writes++;
	}

	return 0;
}

/* Write the format file into the dirfile folder */

int write_format_file(dirfile_t* f)
{
	char filename[MCE_LONG];
	FILE* format;
	int i;

	strcpy(filename, f->basename);
	strcat(filename, "format");
	format = fopen(filename, "w");
	if (format == NULL) return -1;

	for (i=0; i<f->channel_count; i++) {
		fprintf(format, "%-20s RAW U 400\n", f->channels[i].name);
	}

	fclose(format);
	return 0;
}


/* Appends tesdatarXXcXX fields to the dirfile_t structure, setting
 * channel names, frame indices.
 *
 * data_start is index into frame data
 * col_id is the identity of the column to add (appears in the label)
 * col_ofs is the offset of this column, in the data, from the left
 * n_cols
 * cols_width is the total number of columns in the data stream
 * n_rows is the number of rows in the data stream
 */

static int add_column_info(dirfile_t *dirfile, int data_start,
			   int n_rows, int n_cols, int row_id,
			   int col_id, int col_count, int col_ofs)
{
	int c, r;
	for (c=0; c<col_count; c++) 
		for (r=0; r<n_rows; r++) {
			channel_t *ch = dirfile->channels + dirfile->channel_count;
			sprintf(ch->name, TESDATA_FORMAT, r + row_id, c + col_id);
			ch->frame_offset = r*n_cols + (c + col_ofs) + data_start;
			dirfile->channel_count++;
		}
	return 0;
}

/* Appends a field to the dirfile_t structure. */

static void add_item(dirfile_t *dirfile, frame_item* item)
{
	channel_t *c = dirfile->channels + dirfile->channel_count;
	strcpy(c->name, item->name);
	c->frame_offset = item->offset;
	dirfile->channel_count++;
}

/* Appends an array of fields (terminated with index<0) to the
 * dirfile_t structure.
 */

static int add_items(dirfile_t *dirfile, frame_item* items)
{
	int n = 0;
	while (items[n].offset >= 0) {
		add_item(dirfile, items + n);
		n++;
	}
	return n;
}

/* Counts the items in an array of frame_items
 */

static int count_items(frame_item* items)
{
	int n = 0;
	while (items[n].offset >=0 ) n++;
	return n;
}


/* Methods */

static int dirfile_init(mce_acq_t *acq)
{
	int i, ofs;
	int n_fields = 0;
	int n_header = 0;
	int n_data = 0;
	int n_cols = 0;
	frame_item checksum;
	int cards[4];
	dirfile_t *f = (dirfile_t*)acq->storage->action_data;

	// Should do an open test / touch here...
	if (strlen(f->basename)!=0 && f->basename[strlen(f->basename)-1] != '/') {
		strcat(f->basename, "/");
	}
	if (mkdir(f->basename, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
		fprintf(stderr, "Could not create dirfile %s\n", f->basename);
		return -1;
	}

	cards[0] = (acq->cards & MCEDATA_RC1) ? 1 : 0;
	cards[1] = (acq->cards & MCEDATA_RC2) ? 1 : 0;
	cards[2] = (acq->cards & MCEDATA_RC3) ? 1 : 0;
	cards[3] = (acq->cards & MCEDATA_RC4) ? 1 : 0;

	// Setup data description
	for (i=0; i<MCEDATA_CARDS; i++) 
		n_cols += acq->cols*cards[i];
	n_data += n_cols*acq->rows;

	n_header = count_items(header_items);
	n_fields = n_header + n_data + 1;         //checksum!

	// Allocate buffer memory.  Size target 1 / 2 frames
	f->data_size = n_fields * 2;
	dirfile_alloc(f, n_fields, MCE_SHORT, f->data_size);
	
	// Header data
	add_items(f, header_items);
		
	// Set up tesdata fields; assumes cards requested are exactly
        // the cards in the data stream
	ofs = 0;
	for (i=0; i<MCEDATA_CARDS; i++) {
		if (cards[i]) {
			add_column_info(f, MCEDATA_HEADER, acq->rows, n_cols, acq->row0[i],
					MCEDATA_COLUMNS*i+acq->col0[i], acq->cols, ofs);
			ofs += acq->cols;
		}
	}

	// Checksum!
	checksum.offset = n_header + n_data;
	checksum.name = "checksum";
	add_item(f, &checksum);

	// Write format file
	if (write_format_file(f)) {
		fprintf(stderr, "Could not write format file.\n");
		return -1;
	}

	// Set up turn bias so files are cycled.
	// Model 1: smooth spread.
	for (i=0; i<n_fields; i++)
		f->channels[i].turn_bias = i * (f->data_size / 2) / n_fields;

/* 	// Model 2: 4 gangs */
/* 	for (i=0; i<n; i++) */
/* 		f->turn_bias[i] = (i % 4) * (f->data_size / 2) / 4; */

	f->writes_per_frame = 10;

	return 0;
}

static int dirfile_cleanup(mce_acq_t *acq)
{
	dirfile_t *f = (dirfile_t*)acq->storage->action_data;
	
	// Force all channels to write out.
	f->flush = 1;
	dirfile_write(acq, f);
	f->flush = 0;

	return 0;
}

static int dirfile_post(mce_acq_t *acq, int frame_index, u32 *data)
{
	dirfile_t *f = (dirfile_t*)acq->storage->action_data;
	int i;

	// Break frame into streams
	// FIXME - bounds checking on f->data
	for (i=0; i<f->channel_count; i++) {
		channel_t* c = f->channels + i;
//		printf("%i(%u) ", i, c->count); fflush(stdout);
		c->data[c->count++] = data[c->frame_offset];
	}
//	printf("out\n");fflush(stdout);

	return dirfile_write(acq, f);
}

static int dirfile_flush(mce_acq_t *acq)
{
	dirfile_t *f = (dirfile_t*)acq->storage->action_data;
	
	f->flush = 1;
	dirfile_write(acq, f);
	f->flush = 0;

	return 0;
}


/* Generic destructor (not to be confused with cleanup member function) */

static int dirfile_destructor(mcedata_storage_t *storage)
{
	if (storage->action_data == NULL) return 0;

	// Free the private data for the storage module, clear the structure
	dirfile_t *f = (dirfile_t*)storage->action_data;
	dirfile_free(f);
	FREE_NOT_NULL(f)

	memset(storage, 0, sizeof(*storage));
	return 0;
}

mcedata_storage_t dirfile_actions = {
	.init = dirfile_init,
	.cleanup = dirfile_cleanup,
	.post_frame = dirfile_post,
	.flush = dirfile_flush,
	.destroy = dirfile_destructor,
};


mcedata_storage_t* mcedata_dirfile_create(const char *basename, int options)
{
	dirfile_t *f = (dirfile_t*)malloc(sizeof(dirfile_t));
	mcedata_storage_t *storage =
		(mcedata_storage_t*)malloc(sizeof(mcedata_storage_t));
	if (f==NULL || storage==NULL) return NULL;

	//Initialize storage with the file operations, then set local data.
	memcpy(storage, &dirfile_actions, sizeof(dirfile_actions));
	storage->action_data = f;

	memset(f, 0, sizeof(*f));
	strcpy(f->basename, basename);

	return storage;
}


