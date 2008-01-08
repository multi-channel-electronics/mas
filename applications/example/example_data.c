#include <stdio.h>
#include <stdlib.h>

/* These are the MCE include files; they should all be in /usr/include */

#include <mcecmd.h>
#include <mceconfig.h>
#include <mcedata.h>


/* Default device, config files */

#define CMD_DEVICE "/dev/mce_cmd0"
#define DATA_DEVICE "/dev/mce_data0"
#define CONFIG_FILE "/etc/mce.cfg"

/* Our buffer size */

#define SIZE 8196


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

int main()
{
	/*
	  Initialization example
	*/

	// Load MCE config information ("xml")
	mceconfig_t *conf;
	if (mceconfig_load(CONFIG_FILE, &conf) != 0) {
		fprintf(stderr, "Failed to load MCE configuration file %s.\n",
			CONFIG_FILE);
		return 1;
	}

	// Connect to an mce_cmd device.
	int handle = mce_open(CMD_DEVICE);
	if (handle < 0) {
		fprintf(stderr, "Failed to open %s.\n", CMD_DEVICE);;
		return 1;
	}

	// Share the config information with the mce_cmd device
	mce_set_config(handle, conf);


	// Open data device
	mcedata_t mcedata;
	if (mcedata_init(&mcedata, handle, DATA_DEVICE)) {
		fprintf(stderr, "Could not open '%s'\n", DATA_DEVICE);
		return 1;
	}
		
	// Determine current num_rows_reported
	mce_param_t rows_rep;
	if (mce_load_param(handle, &rows_rep, "cc", "num_rows_reported")) {
		fprintf(stderr, "Failed to laod num_rows_reported\n");
		return 1;
	}
	u32 num_rows;
	if (mce_read_element(handle, &rows_rep, 0, &num_rows)) {
		fprintf(stderr, "Reading of num_rows_reported failed!\n");
		return 1;
	}
	
	// Pick a card (any card)
	int cards = MCEDATA_RC1;

	printf("Card bits=%#x, num_rows_reported=%i\n",
	       cards, (int)num_rows);

	// Setup an acqusition structure, associated with this data device.
	mce_acq_t acq;
	mcedata_acq_setup(&acq, &mcedata, 0, cards, (int)num_rows);

	// Our callback will update the counter in this structure
	my_counter_t counter = {
		frame_count: 0
	};

	// Setup the acqusition to call our callback function on each received frame:
	//   (other options here are to set up an output file, or a file sequence)
	mcedata_rambuff_create( &acq, frame_callback, (unsigned)&counter );

	// Now we are free to do as many go's as we want.

	printf("Single go's:\n");
	for (int i=0; i<10; i++) {
		mcedata_acq_go(&acq, 1);		
	}
	
	printf("Multi-go's:\n");
	mcedata_acq_go(&acq, 10);


	/*
	  Clean-up - only really necessary if the program is not about to end...
	*/

	mceconfig_destroy(conf);
	mce_close(handle);

	return 0;
}
