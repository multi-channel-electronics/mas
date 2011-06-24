/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <string.h>

#include "mce_library.h"
#include "frame_manip.h"


/* Define frame header for header version 6 */

struct frame_header_abstraction frame_header_v6 = {
	_size:             43,

	status_v6:          0 | FRAME_OFFSET_PRESENT,
	frame_counter:      1 | FRAME_OFFSET_PRESENT,
	row_len:            2 | FRAME_OFFSET_PRESENT,
	num_rows_reported:  3 | FRAME_OFFSET_PRESENT,
	data_rate:          4 | FRAME_OFFSET_PRESENT,
	rtz_counter:        5 | FRAME_OFFSET_PRESENT,
	header_version:     6 | FRAME_OFFSET_PRESENT,
	ramp_value:         7 | FRAME_OFFSET_PRESENT,
	ramp_address:       8 | FRAME_OFFSET_PRESENT,
	num_rows:           9 | FRAME_OFFSET_PRESENT,
	sync_number:       10 | FRAME_OFFSET_PRESENT,
	run_id:            11 | FRAME_OFFSET_PRESENT,
	user_word:         12 | FRAME_OFFSET_PRESENT,
	fpga_block_v6:     13 | FRAME_OFFSET_PRESENT,
	card_block_v6:     23 | FRAME_OFFSET_PRESENT,
	psc_block_v6:      33 | FRAME_OFFSET_PRESENT,
	box_block_v6:      41 | FRAME_OFFSET_PRESENT,
	data_block:        43 | FRAME_OFFSET_PRESENT,
};


static int count_bits( int bits )
{
	int c = 0;
	while (bits!=0) {
		c += (bits & 1);
		bits = bits >> 1;
	}
	return c;
}


int sort_columns( mce_acq_t *acq, u32 *data )
{
	u32 temp[MCEDATA_PACKET_MAX];

	int header_size = MCEDATA_HEADER;
	int footer_size = MCEDATA_FOOTER;

	int columns = acq->cols;
	int cards_in = count_bits(acq->cards);
	int cards_out = cards_in;
	int rows = (acq->frame_size - header_size - footer_size) /
		columns / cards_in;

	int data_size_out = cards_out*rows*columns;
	int data_size_in = cards_out*rows*columns;

	int c, r, c_in = 0;

/* 	printf("temp=%lu data=%lu, c_bits=%i, r=%i c=%i c_in=%i c_out=%i\n", */
/* 	       temp, data, acq->cards, rows, columns, cards_in, cards_out); */
/* 	fflush(stdout); */

	if (cards_out == 1 && cards_in == 1)
		return 0;

	memcpy(temp, data, header_size*sizeof(*temp));
	memset(temp + header_size, 0, data_size_out*sizeof(*temp));
	memcpy(temp + header_size + data_size_out,
	       data + header_size + data_size_in,
	       footer_size*sizeof(*temp));
	       
	for (c=0; c<cards_out; c++) {
		if ( (acq->cards & (1 << c)) == 0 ) 
			continue;
		for (r=0; r<rows; r++) {
			memcpy(temp+header_size + (r*cards_out + c)*columns,
			       data+header_size + (rows*c_in + r)*columns,
			       columns*sizeof(*temp));
		}
		c_in++;				
	}
	
	memcpy(data, temp,
	       (header_size+footer_size+data_size_out)*sizeof(*data));

	return 0;	       
}
