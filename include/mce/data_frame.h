#ifndef MCE_DATA_FRAME_H_
#define MCE_DATA_FRAME_H_

#define uint32 u32

/* Data frame header information structure:
 *
 * This exposes the information in the MCE data packet house-keeping
 * block, which for our purposes is defined as the set of 32-bit words
 * following the 4 word (preamble, preamble, reply type, size) packet
 * descriptor that is removed by the DSP.
 *
 * The header information consists of 5 major blocks, and some
 * unassigned addresses.  The major blocks describe sequencing counts,
 * FPGA temperatures, card temperatures, power supply readings, and
 * box temperature.  Each block begins with a flag set, which identify
 * any errors and the cards that inititate them.  The bits in each
 * flag have the same rough meaning, and are accessed via the macro
 * DATA_FLAGS_CARD_BIT as described below.
 */


#define DATA_HEADER_CARDS 9

enum {
	DATA_HEADER_AC  = 0,
	DATA_HEADER_BC1 = 1,
	DATA_HEADER_BC2 = 2,
	DATA_HEADER_BC3 = 3,
	DATA_HEADER_RC1 = 4,
	DATA_HEADER_RC2 = 5,
	DATA_HEADER_RC3 = 6,
	DATA_HEADER_RC4 = 7,
	DATA_HEADER_CC  = 8,
};

#define DATA_POWER_LINES 10

enum {
	DATA_POWER_V1 = 1,
	DATA_POWER_V2 = 0,
	DATA_POWER_V3 = 3,
	DATA_POWER_V4 = 2,
	DATA_POWER_V5 = 5,
	DATA_POWER_I1 = 4,
	DATA_POWER_I2 = 7,
	DATA_POWER_I3 = 6,
	DATA_POWER_I4 = 9,
	DATA_POWER_I5 = 8,
};

#define HEADER_SPARE      9

typedef struct data_header_struct {

/* 	u32 data_flags; */
/* 	u16 card_id; */
/* 	u16 para_id; */

	/* sequencing block */

	u32 status;              // DATA_STATUS_*
	u32 sequence;
	u32 internal_sync;
	u32 external_sync;

	/* fpga temperatures */

	u32 fpga_flags;          // DATA_FLAGS_*
	u32 fpga_temps[DATA_HEADER_CARDS];
	                         // DATA_HEADER_*

	/* card temperatures */

	u32 card_flags;          // DATA_FLAGS_*
	u32 card_temps[DATA_HEADER_CARDS];
	                         // DATA_HEADER_*
	/* power supply readings */

	u32 psuc_flags;          // DATA_FLAGS_*
	u8  psuc_temp1;
	u8  psuc_tach2;
	u8  psuc_tach1;
	u8  psuc_version;
	u16 psuc_adc_offset;
	u8  psuc_temp3;
	u8  psuc_temp2;
	u16 psuc_stats[DATA_POWER_LINES];
	                         // DATA_POWER_*

	
/* case temperature */

	u32 box_flags;
	u32 box_temp;

	/* un-used? */

	u32 spare[HEADER_SPARE];

} data_header_t;


/*  status bits  */

enum {
	DATA_STATUS_FRAME_LAST   =  0x00000001,
	DATA_STATUS_FRAME_STOP   =  0x00000002,
	DATA_STATUS_SYNC_FREE    =  0x00000004,
	DATA_STATUS_SYNC_ERROR   =  0x00000008,
	DATA_STATUS_CLOCK        =  0x00000010,
	DATA_STATUS_TES_BIAS     =  0x00000020,
};


/*  _flags bits  
 *
 *  Bits 31 and 30 are _state and _reset, while other bits are triples
 *  (absent|error|readonly), with cards in DATA_HEADER_* order
 *  starting with bit 0 is CC read only.  The bit mask for these is
 *  thus accessed through
 *
 *  DATA_FLAGS_CARD_BIT(DATA_HEADER_BC1, DATA_FLAGS_ABSENT)
 */

enum {
	DATA_FLAGS_READ_ONLY     = 1,
	DATA_FLAGS_ERROR         = 2,
	DATA_FLAGS_ABSENT        = 4,
};


#define DATA_FLAGS_STATE           0x80000000
#define DATA_FLAGS_RESET           0x40000000

#define DATA_FLAGS_PSUC_BIT(flag)  (flag)
#define DATA_FLAGS_CARD_BIT(card, flag)  \
                           ( flag << ((DATA_HEADER_CARDS - card - 1)*3) )


#endif
