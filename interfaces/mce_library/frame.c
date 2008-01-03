#include <string.h>
#include "frame.h"

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

	int header_size = 43;
	int footer_size = 1;

	int columns = 8;
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

