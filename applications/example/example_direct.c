/**
 * Driver-direct data processing example.
 * Matthew Hasselfield, Oct 7, 2008.
 *
 * This example shows how to read frame data directly from the MAS
 * driver.  It can be used in place of the special acquisition
 * handlers in the API.  To do driver-direct frame reads, the
 * acquisition must be started using low-level commands to the MCE via
 * mce_cmd, and not through the usual "acq_config" and "acq_go"
 * commands.
 *
 * The MCE commands required to start an acquisition (without frame
 * data processing) are as follows:
 *
 *      frame <size>
 *      wb cc ret_dat_s 0 <n_frames - 1>
 *      go rcs ret_dat
 *
 * The "size" argument to the "frame" command is the size of the
 * readout frame, in bytes.  The frame size can be calculated as
 *
 *      size = 176 + 32 * n_rows * n_rc
 *
 * where n_rows is the value of "cc num_rows_reported" and n_rc is the
 * number of readout cards reporting (i.e. 4 for "go rcs ret_dat").
 *
 * The data should begin to appear immediately in the driver buffer;
 * this will be apparent from the increase in the "head" index of the
 * buffer in the output of /proc/mce_dsp.  When the example_direct
 * application is running, the "tail" index should keep pace with the
 * "head" index as the data are consumed and processed.
 *
 * Anyway, to test this application:
 *
 * - compile and run example_direct
 * - in an mce_cmd window, issue the following:
 *        wb sys num_rows 33
 *        wb cc num_rows_reported 33
 *        wb cc ret_dat_s 0 999
 *        size 4400
 *        go rcs ret_dat
 *
 * - you can repeat the "go rcs ret_dat", but make sure you wait until
 *   the previous go has completed...
 *
 **/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* This is the MCE include file; it should be in /usr/local/include or something */

#include <mce_library.h>


/* Default device, config files */

#define CMD_DEVICE "/dev/mce_cmd0"
#define DATA_DEVICE "/dev/mce_data0"
#define CONFIG_FILE "/etc/mce/mce.cfg"

/* Our buffer size */

#define SIZE 8196

/* Some frame structure parameters for rearranging the data */

#define MCE_COLUMNS 32
#define MCE_ROWS 41
#define MCE_RC_COLS 8
#define MCE_RCS 4
#define DATA_OFFSET 43

/* Our acquisition structure and callback */

typedef struct {
	int frame_count;
} my_counter_t;


int frame_callback(unsigned user_data, int size, u32 *data)
{
	//Re-type 
	my_counter_t *c = (my_counter_t*)user_data;

	// Increment!
	c->frame_count ++;

	// Print the first two words of each frame as it comes in
	printf("Frame %3i :  %08x %08x\n",
	       c->frame_count, data[0], data[1]);

	printf("RC1234: %08x %08x %08x %08x\n",
	       data[DATA_OFFSET+8*0],
	       data[DATA_OFFSET+8*1],
	       data[DATA_OFFSET+8*2],
	       data[DATA_OFFSET+8*3]);
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

void sort_columns(u32 *data, int count)
{
	/* This routine puts the frame data into "logical ordering",
	   where within the data payload the tes data for c,r are
	   located at c+r*32.

	   This routine assumes the following:
	    - the data include a 43 word header
	    - the data were acquired for 4 readout-cards
	    - the data include a 1 word footer (checksum)
	*/
	int n_rows = (count - DATA_OFFSET - 1) / MCE_COLUMNS;
	u32 temp[MCE_ROWS * MCE_COLUMNS];
	int r, rc;
	for (r=0; r<n_rows; r++) {
		for (rc=0; rc<MCE_RCS; rc++) {
			memcpy(temp+(rc+r*MCE_RCS)*MCE_RC_COLS,
			       data+DATA_OFFSET+(r+n_rows*rc)*MCE_RC_COLS,
			       MCE_RC_COLS*sizeof(u32));
		}
	}
	memcpy(data + DATA_OFFSET, temp, MCE_ROWS*MCE_COLUMNS*sizeof(u32));
}

int main()
{
	/*
	  Initialization example
	*/
	mce_context_t* mce = mcelib_create();

	// Open data device
	if (mcedata_open(mce, DATA_DEVICE) != 0) {
		fprintf(stderr, "Could not open '%s'\n", DATA_DEVICE);
		return 1;
	}
		
	// Pick a card (any card)
	int cards = MCEDATA_RCS;
	printf("Card bits=%#x\n", cards);

	// Our callback will update the counter in this structure
	my_counter_t counter = {
		frame_count: 0
	};

	// Read full frames from the data device file descriptor
	int frame_words = 44 + 32 * 33;
	u32 *buf = (u32*)malloc(frame_words * sizeof(u32));
	
	while (1) {
		int ret = read(mce->data.fd, buf, frame_words*sizeof(u32));

		if (ret <= 0) {
			printf("Exiting after read error\n");
		}

		// Reorder the data
		sort_columns(buf, frame_words);
		
		// Handle the data
		frame_callback((unsigned long)(&counter), frame_words, buf);
	}

	return 0;
}
