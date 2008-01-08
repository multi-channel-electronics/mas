#include <stdio.h>
#include <stdlib.h>

/* These are the MCE include files; they should all be in /usr/include */

#include <mcecmd.h>
#include <mceconfig.h>
#include <mcedata.h>


/* Default device, config files */

#define CMD_DEVICE "/dev/mce_cmd0"
#define CONFIG_FILE "/etc/mce.cfg"


/* Our buffer size */

#define SIZE 256

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
	int error = 0;
	int n = 0;
	u32 data[SIZE];
	u32 more_data[SIZE];

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


        /*
	  Read and write block examples
	*/

	// Lookup "rc1 fw_rev"
	mce_param_t rc1_fw_rev;
	if ((error=mce_load_param(handle, &rc1_fw_rev, "rc1", "fw_rev")) != 0) {
		fprintf(stderr, "Lookup failed.\n");
		return 1;
	}

	// Read.
	error = mce_read_block(handle,
			       &rc1_fw_rev  /* mce_param_t for the card/para */,
			       1            /* number of words to read, per card */,
			       data         /* buffer for the words */);
	if (error != 0) {
		fprintf(stderr, "MCE command failed: '%s'\n",
			mce_error_string(error));
		return 1;
	}

	printf("Read value %u = %#x\n", data[0], data[0]);

	// Try to read 2 words, this will fail.
	// Note that we are re-using rc1_fw_rev; it remains valid.

	error = mce_read_block(handle,
			       &rc1_fw_rev  /* mce_param_t for the card/para */,
			       2            /* number of words to read, per card */,
			       data         /* buffer for the words */);

	printf("Reading 2 words from rc1 fw_rev returns error -%#x and message '%s'\n",
	       -error, mce_error_string(error));

	// Multi-value read:

	mce_param_t gainp0;
	if ( mce_load_param(handle, &gainp0, "rc1", "gainp0") != 0) {
		fprintf(stdout, "Couldn't load gainp0.\n");
		return 1;
	}

	// Number of values in in gainp0.param.count
	n = gainp0.param.count;
	error = mce_read_block(handle, &gainp0, n, data);
	printf("rc1 gainp0: ");
	print_u32(data, n);

	// Manipulate
	printf("Replacing with 10*i...\n");
	for (int i=0; i<n; i++) {
		data[i] = 10*i;
	}
	error = mce_write_block(handle, &gainp0, n, data);

	error = mce_read_block(handle, &gainp0, n, data);
	printf("rc1 gainp0: ");
	print_u32(data, n);

	// Manipulate single elements
	printf("Setting elements 3 and 12...\n");
	error = mce_write_element(handle, &gainp0, 3, 66);
	error = mce_write_element(handle, &gainp0, 12, 88);

	error = mce_read_block(handle, &gainp0, n, data);
	printf("rc1 gainp0: ");
	print_u32(data, n);


	/*
	  Sys: multi-card read abstraction; other contents of mce_param_t structure
	*/
	
	mce_param_t sys_row_len;
	if (mce_load_param(handle, &sys_row_len, "sys", "row_len") != 0) {
		fprintf(stderr, "Couldn't load 'sys row_len'\n");
		return 1;
	}
	
	// Number of values read must be corrected for card count.
	int n_write = sys_row_len.param.count;
	int n_read  = n_write * sys_row_len.card.card_count;
	
	// mce_param_t contains much useful information...
	printf("'%s %s' operates on %i cards\n",
	       sys_row_len.card.name, sys_row_len.param.name,
	       sys_row_len.card.card_count);

	// Note that we can pass "-1" as the count to query for all data
	//  (i.e. -1 should return the same number of data as n_write)
	error = mce_read_block(handle, &sys_row_len, -1, data);
	printf(" data: ");
	print_u32(data, n_read);

	// Let's change those
	printf("Set to 50...\n");
	more_data[0] = 50;
	error = mce_write_block(handle, &sys_row_len, -1, more_data);

	error = mce_read_block(handle, &sys_row_len, -1, more_data);
	printf(" data: ");
	print_u32(more_data, n_read);

	// Restore...
	printf("Restore...\n");
	error = mce_write_block(handle, &sys_row_len, -1, data);

	error = mce_read_block(handle, &sys_row_len, -1, more_data);
	printf(" data: ");
	print_u32(more_data, n_read);


	/*
	  Clean-up - only really necessary if the program is not about to end...
	*/

	mceconfig_destroy(conf);
	mce_close(handle);

	return 0;
}
