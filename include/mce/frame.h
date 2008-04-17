#ifndef __FRAME_H__
#define __FRAME_H__

#include "mce.h"

#define EXIT_LAST      1
#define EXIT_READ      2
#define EXIT_WRITE     3
#define EXIT_STOP      4
#define EXIT_EOF       5
#define EXIT_COUNT     6 
#define EXIT_TIMEOUT   7

/* Generic frame header description.

The value of each variable is the offset in the frame header of the
associated parameter, ORed with 0x80000000.  A zero value indicates that
the parameter is not present in the active header version.  If the
meaning or bit assignment of parameters is likely to change in later
header versions, these should be explicitly identified by calling them
different things; e.g. status_v6 might be relevant to header v. 6 and
7 but status_v8 should be used for subsequent version.
*/

#define FRAME_OFFSET_PRESENT   0x80000000
#define FRAME_OFFSET_MASK      0x7fffffff

struct frame_header_abstraction {

	int _size;

	// If the status bits change meaning, define a new status word
	// for that header version

	int status_v6;

	int frame_counter;
	int row_len;
	int num_rows_reported;
	int data_rate;
	int rtz_counter;
	int header_version;
	int ramp_value;
	int ramp_address;
	int num_rows;
	int sync_number;
	int run_id;
	int user_word;

	int fpga_block_v6;
	int card_block_v6;
	int psc_block_v6;
	int box_block_v6;

	int data_block;
};

typedef struct frame_header_abstraction frame_header_abstraction_t;

#define frame_property( /* u32* */ data, /* struct frame_header_abstraction* */ format, property ) \
       (data[(format)->property & FRAME_OFFSET_MASK])

#define frame_has_property( /* struct frame_header_abstraction* */ format, property ) \
       (format->property & FRAME_OFFSET_PRESENT)

/* Frame descriptions are in frame.c; bit breakouts are defined here */

extern struct frame_header_abstraction frame_header_v6;


/* Bit defines for status_v6 word */

#define FRAME_STATUS_V6_LAST        ( 1 <<  0 )
#define FRAME_STATUS_V6_STOP        ( 1 <<  1 )
#define FRAME_STATUS_V6_SYNC_FREE   ( 1 <<  2 )
#define FRAME_STATUS_V6_SYNC_ERR    ( 1 <<  3 )
#define FRAME_STATUS_V6_ACTIVE      ( 1 <<  4 )
#define FRAME_STATUS_V6_TES_SQUARE  ( 1 <<  5 )
#define FRAME_STATUS_V6_RC1         ( 1 << 10 )
#define FRAME_STATUS_V6_RC2         ( 1 << 11 )
#define FRAME_STATUS_V6_RC3         ( 1 << 12 )
#define FRAME_STATUS_V6_RC4         ( 1 << 13 )

#endif
