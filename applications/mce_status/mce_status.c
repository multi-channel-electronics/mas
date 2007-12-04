/*! \file mce_cmd.c
 *
 *  \brief Program to read and record the status of the MCE.
 *
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <mcecmd.h>
#include <mceconfig.h>

int handle = -1;
mceconfig_t *mce = NULL;

typedef struct {
	char device_file[MCE_LONG];
	char config_file[MCE_LONG];
} options_t;

options_t options = {
	device_file: "/dev/mce_cmd0",
	config_file: "/etc/mas.cfg",
};


int load_settings( mceconfig_t *mce);

int main(int argc, char **argv)
{
	FILE *ferr = stderr;

/* 	if (process_options(argc, argv)) { */
/* 		err = ERR_OPT; */
/* 		goto exit_now; */
/* 	} */

	handle = mce_open(options.device_file);
	if (handle<0) {
		fprintf(ferr, "Could not open mce device '%s'\n",
			options.device_file);
		return 1;
	}

	// Ready data thing
	if (mceconfig_load(options.config_file, &mce)!=0) {
		fprintf(ferr, "Could not load MCE config file '%s'.\n",
			options.config_file);
		return 3;
	}

	// Share config with MCE command library.
	mce_set_config(handle, mce);


	// Loop through cards and parameters
	load_settings(mce);

	return 0;
}

int read_now()
{
	u32 buf[100];
	mce_param_t mcep;
	int err = mce_read_block(handle, &mcep, -1, buf);
	if (err)
		return err;
	
	return 0;
}

int load_settings( mceconfig_t *mce)
{
	int i,j;
	int n_cards = mce->card_count;
	
	for (i=0; i<n_cards; i++) {
		card_t card;
		cardtype_t ct;
		if (mceconfig_card(mce, i, &card)) {
			fprintf(stderr, "Problem loading card data at index %i\n", i);
			return -1;
		}
		if (mceconfig_card_cardtype(mce, &card, &ct)) {
			fprintf(stderr, "Problem loading cardtype data for '%s'\n", card.name);
			return -1;
		}
		
		int k;
		paramset_t ps;
		param_t p;
		
		for (j=0; j<ct.paramset_count; j++) {
			mceconfig_cardtype_paramset(mce, &ct, j, &ps);
			for (k=0; k<ps.param_count; k++) {
				mceconfig_paramset_param(mce, &ps, k, &p);
				printf("%s %s\n", card.name, p.name);
			}
		}
	}
	return 0;
}


int get_int(char *card, int *card_id)
{
	char *end = card;
	if (end==NULL || *end==0) return -1;
	*card_id = strtol(card, &end, 0);
	if (*end!=0) return -1;
	return 0;
}
