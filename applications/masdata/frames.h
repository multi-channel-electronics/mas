#ifndef _FRAMES_H_
#define _FRAMES_H_

#include "main.h"

#define MAX_COLCONF  8

#define N_FORMATS 2
extern struct string_pair formats[N_FORMATS];

/* Enumerator for frame output formats */

typedef enum {
	FORMAT_RAW = 0,
	FORMAT_LOGICAL = 1,
} format_t;


typedef struct column_config_struct {

	char code[SHORTLEN];
	int cards;
	int card_idx;

	int  card_id;
	int  para_id;

} column_config_t;


typedef struct frame_setup_struct {

	int header;
	int rows;
	int columns;
	int extra;

	int columns_card;
	int colcfg_count;
	int colcfg_index;
	column_config_t colcfgs[MAX_COLCONF];

	int  data_size;
	int  frame_size;
	int  seq_first;
	int  seq_last;

	int format;

} frame_setup_t;


/* Frame data has different sizes based on:
 *   - number of readout cards (1 or 4)
 *   - output format (raw or logical)
 *
 * frame_size returns the _output_ frame size.
 * frame_data_size returns the _input_ frame size. */

int  frame_size(frame_setup_t *frame);
int  frame_data_size(frame_setup_t *frame);

/* Overriding input/output frame data size is accomplished with
 *   frame_set_size and frame_set_data_size
 * These settings will be lost once frame_set_columns is called. */

int frame_set_size(frame_setup_t *frame, int size);
int frame_set_data_size(frame_setup_t *frame, int size);

/* frame_set_card
 *   is used to select a read-out card configuration,
 *   e.g. "rcs" or "rc2" */

int  frame_set_card(frame_setup_t *frame, char *code, char *errstr);

/* frame_set_format
 *   is used to set the output frame format,
 *   e.g. "raw" or "logical" */

int  frame_set_format(frame_setup_t *frame, char *code, char *errstr);

/* frame_set_sequence
 *   is used to store the first and last frames for the next acquisition
 *   e.g. 1 10000   */

int frame_set_sequence(frame_setup_t *frame, int first, int last);

#endif
