#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frames.h"

struct string_pair formats[N_FORMATS] = {
	{FORMAT_RAW,      "raw"    },
	{FORMAT_LOGICAL,  "logical"},
};


/* FRAME stuff */

int  frame_size(frame_setup_t *frame)
{
	switch (frame->format) {
	case FORMAT_RAW:
		return frame_data_size(frame);

	case FORMAT_LOGICAL:
		return 4*(frame->header + frame->extra +
			  frame->rows * frame->columns);
	}
	return 0;
}


int  frame_data_size(frame_setup_t *frame)
{
	switch (frame->format) {
	case FORMAT_RAW:
	case FORMAT_LOGICAL:
		return 4*(frame->header + frame->extra +
			  frame->rows
			  * frame->colcfgs[frame->colcfg_index].cards
			  * frame->columns_card);
	}
	return 0;
}


void frame_reload_sizes(frame_setup_t *frame)
{
	frame->frame_size = frame_size(frame);
	frame->data_size = frame_data_size(frame);
}


int frame_set_size(frame_setup_t *frame, int size)
{
	frame->frame_size = size;
	return 0;
}

int frame_set_data_size(frame_setup_t *frame, int size)
{
	frame->data_size = size;
	return 0;
}


int  frame_set_card(frame_setup_t *frame, char *code, char *errstr)
{
	int i;
	if (frame==NULL) {
		sprintf(errstr, "Null frame_setup_t* !");
		return -1;
	}
	if (code != NULL) {
		for (i=0; i<frame->colcfg_count; i++) {
			if (strcmp(code, frame->colcfgs[i].code)==0) {
				frame->colcfg_index = i;
				frame_reload_sizes(frame);
				return i;
			}
		}
	}

	// Give them a list.
	errstr += sprintf(errstr, "card must be one of [ ");
	for (i=0; i<frame->colcfg_count; i++) {
		if ( i == frame->colcfg_index )
			*errstr++ = '*';
		errstr += sprintf(errstr, "%s ", frame->colcfgs[i].code);
	}
	errstr += sprintf(errstr, "]");
	
	return -1;
}


int frame_set_format(frame_setup_t *frame, char *code, char *errstr)
{
	if (frame==NULL || code==NULL) return -1;

	int key = -1;
	if (code != NULL)
		key = lookup_key(formats, N_FORMATS, code);
	
	if (key<0) {
		sprintf(errstr, "format must be one of [ %s]",
			key_list_marker(formats, N_FORMATS, " ",
					frame->format, "*"));
		return -1;
	}

	frame->format = key;
	frame_reload_sizes(frame);
	return 0;
}


int frame_set_sequence(frame_setup_t *frame, int first, int last)
{
	frame->seq_first = first;
	frame->seq_last  = last;
	return 0;
}
