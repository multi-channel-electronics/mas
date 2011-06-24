/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdlib.h>

/* This is the MCE include file; it should be in /usr/local/include or something */

#include <mce_library.h>


/* Default device, config files */

#define CMD_DEVICE "/dev/mce_cmd0"
#define CONFIG_FILE "/etc/mce/mce.cfg"


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

	// Get a library context structure (cheap)
  mce_context_t mce = mcelib_create();

	// Load MCE config information ("xml")
	if (mceconfig_open(mce, CONFIG_FILE, NULL) != 0) {
		fprintf(stderr, "Failed to load MCE configuration file %s.\n",
			CONFIG_FILE);
		return 1;
	}

	// Connect to an mce_cmd device.
	if (mcecmd_open(mce, CMD_DEVICE) != 0) {
		fprintf(stderr, "Failed to open %s.\n", CMD_DEVICE);;
		return 1;
	}


        /*
	  Read and write block examples
	*/

	// Lookup "rc1 fw_rev"
	mce_param_t rc1_fw_rev;
	if ((error=mcecmd_load_param(mce, &rc1_fw_rev, "rc1", "fw_rev")) != 0) {
		fprintf(stderr, "Lookup failed.\n");
		return 1;
	}

	// Read.
	error = mcecmd_read_block(mce,
			       &rc1_fw_rev  /* mce_param_t for the card/para */,
			       1            /* number of words to read, per card */,
			       data         /* buffer for the words */);
	if (error != 0) {
		fprintf(stderr, "MCE command failed: '%s'\n",
			mcelib_error_string(error));
		return 1;
	}

	printf("Read value %u = %#x\n", data[0], data[0]);

	// Try to read 2 words, this will fail.
	// Note that we are re-using rc1_fw_rev; it remains valid.

	error = mcecmd_read_block(mce,
			       &rc1_fw_rev  /* mce_param_t for the card/para */,
			       2            /* number of words to read, per card */,
			       data         /* buffer for the words */);

	printf("Reading 2 words from rc1 fw_rev returns error -%#x and message '%s'\n",
	       -error, mcelib_error_string(error));

	// Multi-value read:

	mce_param_t gainp0;
	if ( mcecmd_load_param(mce, &gainp0, "rc1", "gainp0") != 0) {
		fprintf(stdout, "Couldn't load gainp0.\n");
		return 1;
	}

	// Number of values in in gainp0.param.count
	n = gainp0.param.count;
	error = mcecmd_read_block(mce, &gainp0, n, data);
	printf("rc1 gainp0: ");
	print_u32(data, n);

	// Manipulate
	printf("Replacing with 10*i...\n");
	for (int i=0; i<n; i++) {
		data[i] = 10*i;
	}
	error = mcecmd_write_block(mce, &gainp0, n, data);

	error = mcecmd_read_block(mce, &gainp0, n, data);
	printf("rc1 gainp0: ");
	print_u32(data, n);

	// Manipulate single elements
	printf("Setting elements 3 and 12...\n");
	error = mcecmd_write_element(mce, &gainp0, 3, 66);
	error = mcecmd_write_element(mce, &gainp0, 12, 88);

	error = mcecmd_read_block(mce, &gainp0, n, data);
	printf("rc1 gainp0: ");
	print_u32(data, n);


	/*
	  Sys: multi-card read abstraction; other contents of mce_param_t structure
	*/
	
	mce_param_t sys_row_len;
	if (mcecmd_load_param(mce, &sys_row_len, "sys", "row_len") != 0) {
		fprintf(stderr, "Couldn't load 'sys row_len'\n");
		return 1;
	}
	
	// The natural 'size' of the parameter is obtained like this:
	int n_write = sys_row_len.param.count;

	// On reads, some parameters query multiple cards and return
        // n_cards * n_write data elements.
	int n_read  = mcecmd_read_size(&sys_row_len, n_write);
	
	// mce_param_t contains much useful information...
	printf("'%s %s' operates on %i cards\n",
	       sys_row_len.card.name, sys_row_len.param.name,
	       sys_row_len.card.card_count);

	// Note that we can pass "-1" as the count to query for all data
	//  (i.e. -1 should return the same number of data as n_write)
	error = mcecmd_read_block(mce, &sys_row_len, -1, data);
	printf(" data: ");
	print_u32(data, n_read);

	// Let's change those
	printf("Set to 50...\n");
	more_data[0] = 50;
	error = mcecmd_write_block(mce, &sys_row_len, -1, more_data);

	error = mcecmd_read_block(mce, &sys_row_len, -1, more_data);
	printf(" data: ");
	print_u32(more_data, n_read);

	// Restore...
	printf("Restore...\n");
	error = mcecmd_write_block(mce, &sys_row_len, -1, data);

	error = mcecmd_read_block(mce, &sys_row_len, -1, more_data);
	printf(" data: ");
	print_u32(more_data, n_read);


	/*
	  Clean-up - only really necessary if the program is not about to end...
	*/

	mcelib_destroy(mce);

	return 0;
}
