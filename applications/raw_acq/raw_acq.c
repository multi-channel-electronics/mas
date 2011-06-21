/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This is the MCE include file; it should be in /usr/local/include or something */

#include <mce_library.h>
#include <mce/defaults.h>
#include "../../defaults/config.h"

/* Our buffer size */

#define RAW_SIZE 16392
#define HEADER_SIZE 43

#define NCOLS        8

/* Frame header */

#define FRAME_STATUS                  0
#define FRAME_COUNTER                 1
#define FRAME_NUM_ROWS_REP            3
#define FRAME_NUM_ROWS                9

enum STORE_SCHEME {
	SINGLE_ROW, MULTI_ROW
};


/* Our acquisition structure and callback */

typedef struct {
	u32 header[HEADER_SIZE];
	u32 *raw_data;
	int count;
	int header_offset;
	int data_size;
	int max_frames;
} raw_data_t;


int frame_callback(unsigned long user_data, int size, u32 *data)
{
	//Re-type 
	raw_data_t *c = (raw_data_t*)user_data;

	// Copy header from first frame
	if (c->count == 0)
		memcpy(c->header, data, c->header_offset*sizeof(data[0]));

	// Copy frame into buffer
	memcpy(c->raw_data + c->data_size * c->count, data + c->header_offset,
	       c->data_size * sizeof(data[0]));

	c->count ++;
	return 0;
}

void print_u32(u32 *data, int count)
{
	int i;
	for (i=0; i<count; i++) {
		printf("%u ", data[i]);
	}
	printf("\n");
}

mce_context_t* mce_connect(int fibre_card)
{
    char *ptr;

	// Get a library context structure (cheap)
    mce_context_t *mce = mcelib_create(fibre_card, NULL);

	// Load MCE config information ("xml")
    ptr = mcelib_default_hardwarefile(fibre_card);
    if (ptr == NULL) {
        fprintf(stderr, "Unable to obtain path to default mce.cfg!\n");
        return NULL;
    } else if (mceconfig_open(mce, ptr, NULL) != 0) {
		fprintf(stderr, "Failed to load MCE configuration file %s.\n", ptr);
		return NULL;
	}
    free(ptr);

	// Connect to an mce_cmd device.
    ptr = mcelib_cmd_device(fibre_card);
    if (ptr == NULL) {
        fprintf(stderr, "Unable to obtain path to default command device!\n");
        return NULL;
    } else if (mcecmd_open(mce, ptr) != 0) {
		fprintf(stderr, "Failed to open %s.\n", ptr);
		return NULL;
	}
    free(ptr);

	// Open data device
    ptr = mcelib_data_device(fibre_card);
    if (ptr == NULL) {
        fprintf(stderr, "Unable to obtain path to default data device!\n");
        return NULL;
    } else if (mcedata_open(mce, ptr) != 0) {
		fprintf(stderr, "Could not open '%s'\n", ptr);
		return NULL;
	}
    free(ptr);

	return mce;
}

int main(int argc, char **argv)
{
	int err = 0;
	char card_str[] = "rcx";
	char *filename = NULL;
	int card = 0;
    int fibre_card = -1;
	int store_scheme = SINGLE_ROW;
	mce_context_t *mce;

	if (argc >= 3 && strlen(argv[2])==1)
		card_str[2] = argv[2][0];
  
    if (argc >= 4)
        fibre_card = atoi(argv[3]);

	switch(card_str[2]) {
	case '1':
		card = MCEDATA_RC1;
		break;
	case '2':
		card = MCEDATA_RC2;
		break;
	case '3':
		card = MCEDATA_RC3;
		break;
	case '4':
		card = MCEDATA_RC4;
		break;
	}

	if (card == 0) {
		printf("Usage: %s <filename> <readout-card>"
#if MULTICARD
                " [fibre-card]"
#endif
                "\n\nwhere readout-card is 1 2 3 or 4"
#if MULTICARD
                " and fibre-card is your fibre card of choice"
#endif
                ".\n", argv[0]);
		printf("You should probably be running this from some kind of script...\n");
		exit(1);
	}
	filename = argv[1];

    mce = mce_connect(fibre_card);

	if (mce == NULL)
    exit(1);
	
	/* Lookups */
	mce_param_t p_row_len, p_num_rows, p_num_rows_reported, p_ret_dat_s,
              p_data_mode, p_captr_raw;
	
	err |= mcecmd_load_param(mce, &p_row_len, "cc", "row_len");
	err |= mcecmd_load_param(mce, &p_num_rows, "cc", "num_rows");
	err |= mcecmd_load_param(mce, &p_num_rows_reported, "cc", "num_rows_reported");
	err |= mcecmd_load_param(mce, &p_ret_dat_s, "cc", "ret_dat_s");
	err |= mcecmd_load_param(mce, &p_data_mode, card_str, "data_mode");
	err |= mcecmd_load_param(mce, &p_captr_raw, card_str, "captr_raw");
	if (err) {
		printf("Lookups failed!\n");
		exit(1);
	}

	/* Storage */
	u32 row_len, num_rows, num_rows_reported, data_mode;
	u32 ret_dat_s[2];
	u32 temp[32];

	err |= mcecmd_read_block(mce, &p_row_len, 1, &row_len);
	err |= mcecmd_read_block(mce, &p_num_rows, 1, &num_rows);
	err |= mcecmd_read_block(mce, &p_num_rows_reported, 1, &num_rows_reported);
	err |= mcecmd_read_block(mce, &p_data_mode, 1, &data_mode);
	err |= mcecmd_read_block(mce, &p_ret_dat_s, 2, ret_dat_s);
	if (err != 0) {
		printf("Reads failed!\n");
		exit(1);
	}

	// Our callback will update the counter in this structure
	raw_data_t raw_data;

	// There are row_len readout frames per internal MCE cycle,
	// and up to 2 internal frames in the dump.
	int n_internal = 2;

	// Configure local acq data structure.
	raw_data.max_frames = (int)row_len * n_internal;
	raw_data.data_size = NCOLS*num_rows;
	raw_data.header_offset = HEADER_SIZE;
	raw_data.raw_data = (u32*)malloc(raw_data.data_size * raw_data.max_frames * sizeof(u32));
	raw_data.count = 0;

	// Storage object calls our callback.
	mcedata_storage_t* ramb;
	ramb = mcedata_rambuff_create(frame_callback, (unsigned long)&raw_data);

	// Setup an acquisition structure, associated with the rambuff.
	mce_acq_t acq;
	mcedata_acq_create(&acq, mce, 0, card, -1, ramb);

	// Write data mode and trigger a capture
	temp[0] = num_rows;
	mcecmd_write_block(mce, &p_num_rows_reported, 1, temp);
	temp[0] = 3;
	mcecmd_write_block(mce, &p_data_mode, 1, temp);
	temp[0] = 1;
	mcecmd_write_block(mce, &p_captr_raw, 1, temp);

	// Acquire the raw data.
	mcedata_acq_go(&acq, raw_data.max_frames);

	// Write out the data
	FILE *fout = fopen(filename, "w");
	u32 *buf = (u32*)malloc((HEADER_SIZE + raw_data.data_size + 1) * sizeof(u32));

	switch (store_scheme) {

	case SINGLE_ROW:
		/* In this mode, the data appear as 8 columns of
		   single row data, time-ordered.  We write a header
		   for each time sample, which is inefficent but
		   necessary. */

		// Hack the header into shape
		memcpy(buf, raw_data.header, HEADER_SIZE*sizeof(u32));
		buf[FRAME_NUM_ROWS_REP] = 1;
				
		for (int i=0; i<raw_data.max_frames; i++) {
			for (int j=0; j<num_rows; j++) {
				buf[FRAME_COUNTER] = i*num_rows + j;
				memcpy(buf + HEADER_SIZE,
				       raw_data.raw_data + (i*num_rows + j)*NCOLS,
				       NCOLS*sizeof(u32));
				buf[HEADER_SIZE + NCOLS] =
					mcecmd_checksum(buf, HEADER_SIZE + raw_data.data_size);
				fwrite(buf, 1, (HEADER_SIZE + raw_data.data_size + 1) *
				       sizeof(u32), fout);
			}
		}
		break;

	case MULTI_ROW:
		/* In this mode, the data are organized by row and
		 column.  The data associated with the visits to a
		 certain row are grouped together. */

		for (int i=0; i<raw_data.max_frames; i++) {
			raw_data.header[1] = i;
			memcpy(buf, raw_data.header, HEADER_SIZE*sizeof(u32));
			for (int j=0; j<num_rows; j++) {
				memcpy(buf + HEADER_SIZE + j*NCOLS,
				       raw_data.raw_data + (j*row_len+i)*NCOLS,
				       NCOLS*sizeof(u32));
			}
			buf[HEADER_SIZE + raw_data.data_size] =
				mcecmd_checksum(buf, HEADER_SIZE + raw_data.data_size);

			fwrite(buf, 1, (HEADER_SIZE + raw_data.data_size + 1)*sizeof(u32), fout);
		}
		break;
	}

	fclose(fout);

	// Rewrite
	mcecmd_write_block(mce, &p_num_rows_reported, 1, &num_rows_reported);
	mcecmd_write_block(mce, &p_data_mode, 1, &data_mode);
	mcecmd_write_block(mce, &p_ret_dat_s, 2, ret_dat_s);

	// Done
	mcelib_destroy(mce);

	return 0;
}
