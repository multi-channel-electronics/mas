#define _GNU_SOURCE

/* Dirfile storage module */

#include <stdlib.h>
#include <string.h>

#include <mce_library.h>

/* Method: data buffers for all channels. */

#define TESDATA_NAME   "tesdatar00c00"
#define TESDATA_FORMAT "tesdatar%02ic%02i"

#define DIRFILE_CHANNELS      (MCEDATA_CARDS*MCEDATA_COLUMNS*MCEDATA_ROWS)

typedef struct dirfile_struct {

	u32 *data[DIRFILE_CHANNELS];     // Pointers to data buffers for each channel (c*41 + r)
	int count[DIRFILE_CHANNELS];     // Level of each buffer
	int turn_bias[DIRFILE_CHANNELS]; // Initial bias to cause staggered writing of buffers to disk

	int frame_offsets[DIRFILE_CHANNELS]; // Place in frame to find each datum
	int data_size;

	int column_count;
	int row_count;

	int channel_count;

	int writes_per_frame;
	int flush;

	char basename[MCE_LONG];
	char format[MCE_LONG];
	char *filenames[DIRFILE_CHANNELS];

//	struct frame_header_abstraction frame_description;

} dirfile_t;

static int dirfile_write(mce_acq_t *acq, dirfile_t *f)
{
	int i;
	FILE *fout;
	int thresh = f->data_size / 2;

	int writes = 0;
	for (i=0; i<f->channel_count; i++) {
		if (!f->flush && f->count[i] + f->turn_bias[i] < thresh)
			continue;
		fout = fopen64(f->filenames[i], "a");
		if (fout == NULL) continue;
		
		fwrite(f->data[i], f->count[i], sizeof(u32), fout);
		
		// If we are out-of-turn, use turn_bias to restore
		//  turn_bias = (count+turn_bias) - thresh
		f->turn_bias[i] += f->count[i] - thresh;
		f->count[i] = 0;
		
		fclose(fout);
		writes++;
	}

	return 0;
}

static int write_tesdata_info(char **name, int *ofs, int column, int n_columns, int n_rows)
{
	int c, r, i;
	for (c=0; c<n_columns; c++) 
		for (r=0; r<n_rows; r++) {
			i = c * n_rows + r;
			sprintf(name[i], TESDATA_FORMAT, r, c + column);
			ofs[i] = (c + column) * n_rows + r + 43;
		}
	return 0;
}


static int dirfile_init(mce_acq_t *acq)
{
	int i, n, m;
	dirfile_t *f = (dirfile_t*)acq->storage->action_data;

	// Setup data description
	f->column_count =
		((acq->cards & MCEDATA_RC1) ? 8 : 0) +
		((acq->cards & MCEDATA_RC2) ? 8 : 0) +
		((acq->cards & MCEDATA_RC3) ? 8 : 0) +
		((acq->cards & MCEDATA_RC4) ? 8 : 0);

	f->row_count = acq->rows;

	// Allocate buffer memory.  Size target 1 / 2 frames
	n = f->row_count * f->column_count;
	f->data_size = n * 2;
	
	f->data[0] = malloc(f->data_size * n * sizeof(u32));
	if (f->data[0] == NULL) return -1;
	for (i=1; i<n; i++)
		f->data[i] = f->data[0] + i*f->data_size;

	// Prewrite the filenames
	m = strlen(f->basename) + strlen(TESDATA_NAME) + 5;
	f->filenames[0] = malloc(m*n * sizeof(char));
	if (f->filenames[0] == NULL) return -2;

	for (i=0; i<n; i++) {
		f->filenames[i] = f->filenames[0] + i*m;
	}
	
	// Set up tesdata fields.
	f->channel_count = 0;
	if (acq->cards & MCEDATA_RC1) {
		write_tesdata_info(f->filenames + f->channel_count,
				   f->frame_offsets + f->channel_count,
				    0, 8, f->row_count);
		f->channel_count += 8 * f->row_count;
	}
	if (acq->cards & MCEDATA_RC2) {
		write_tesdata_info(f->filenames + f->channel_count,
				   f->frame_offsets + f->channel_count,
				     8, 8, f->row_count);
		f->channel_count += 8 * f->row_count;
	}
	if (acq->cards & MCEDATA_RC3) {
		write_tesdata_info(f->filenames + f->channel_count,
				   f->frame_offsets + f->channel_count,
				    16, 8, f->row_count);
		f->channel_count += 8 * f->row_count;
	}
	if (acq->cards & MCEDATA_RC4) {
		write_tesdata_info(f->filenames + f->channel_count,
				   f->frame_offsets + f->channel_count,
				    24, 8, f->row_count);
		f->channel_count += 8 * f->row_count;
	}

	// Should do an open test / touch here...
	
	// Set up turn bias so files are cycled.
	// Model 1: smooth spread.
	for (i=0; i<n; i++)
		f->turn_bias[i] = i * (f->data_size / 2) / m;

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
	for (i=0; i<f->channel_count; i++)
		f->data[i][f->count[i]++] = data[f->frame_offsets[i]];

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
	if (f != NULL) {
		if (f->data[0] != NULL) free(f->data[0]);
		if (f->filenames[0] != NULL) free(f->filenames[0]);
		free(f);
	}

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
	mcedata_storage_t *storage = (mcedata_storage_t*)malloc(sizeof(mcedata_storage_t));
	if (f==NULL || storage==NULL) return NULL;

	//Initialize storage with the file operations, then set local data.
	memcpy(storage, &dirfile_actions, sizeof(dirfile_actions));
	storage->action_data = f;

	memset(f, 0, sizeof(*f));

	strcpy(f->basename, basename);
	
	return storage;
}
