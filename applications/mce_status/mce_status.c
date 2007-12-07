/*! \file mce_cmd.c
 *
 *  \brief Program to read and record the status of the MCE.
 *
 *  How about we make this a generic tree traverser that you can plug
 *  your own operations into.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* #include <mcecmd.h> */
#include <mceconfig.h>
#include <libmaslog.h>

int handle = -1;
mceconfig_t *mce = NULL;

typedef struct {
	char device_file[MCE_LONG];
	char config_file[MCE_LONG];
	char hardware_file[MCE_LONG];
} options_t;

options_t options = {
	device_file: "/dev/mce_cmd0",
	config_file: "/etc/mas.cfg",
	hardware_file: "/etc/mce.cfg",
};

/* mce_status handler - output in XML like format */

typedef struct {
	FILE *out;
} stat_t;

void stat_item(unsigned long data, mce_param_t *p)
{
	stat_t *s = (stat_t*) data;

	if ((p->card.flags & MCE_PARAM_NOSTAT) ||
	    (p->param.flags & MCE_PARAM_NOSTAT))
		return;

	fprintf(s->out, "<RB %s %s>", p->card.name, p->param.name);
	
	// Then do a read...

	fprintf(s->out, "\n");
}



int load_settings( mceconfig_t *mce);

int main(int argc, char **argv)
{
	FILE *ferr = stderr;
	logger_t logger;
	char msg[MCE_LONG];

	logger_connect( &logger, options.config_file, "mce_status" );
	sprintf(msg, "initiated with hardware config '%s'", options.hardware_file);
	logger_print( &logger, msg );

/* 	if (process_options(argc, argv)) { */
/* 		err = ERR_OPT; */
/* 		goto exit_now; */
/* 	} */

/* 	handle = mce_open(options.device_file); */
/* 	if (handle<0) { */
/* 		fprintf(ferr, "Could not open mce device '%s'\n", */
/* 			options.device_file); */
/* 		return 1; */
/* 	} */

	// Ready data thing
	if (mceconfig_load(options.hardware_file, &mce)!=0) {
		fprintf(ferr, "Could not load MCE config file '%s'.\n",
			options.hardware_file);
		return 3;
	}

/* 	// Share config with MCE command library. */
/* 	mce_set_config(handle, mce); */


	// Loop through cards and parameters
	load_settings(mce);

	return 0;
}

/* int read_now() */
/* { */
/* 	u32 buf[100]; */
/* 	mce_param_t mcep; */
/* 	int err = mce_read_block(handle, &mcep, -1, buf); */
/* 	if (err) */
/* 		return err; */
	
/* 	return 0; */
/* } */

int load_settings( mceconfig_t *mce)
{
	int i,j;
	int n_cards = mce->card_count;
	
	for (i=0; i<n_cards; i++) {
		mce_param_t m;
		card_t *c = &m.card;
		cardtype_t ct;
		if (mceconfig_card(mce, i, c)) {
			fprintf(stderr, "Problem loading card data at index %i\n", i);
			return -1;
		}
		if (mceconfig_card_cardtype(mce, c, &ct)) {
			fprintf(stderr, "Problem loading cardtype data for '%s'\n", c->name);
			return -1;
		}
		
		int k;
		paramset_t ps;
		param_t *p = &m.param;
		
		stat_t s;
		s.out = stdout;

		for (j=0; j<ct.paramset_count; j++) {
			mceconfig_cardtype_paramset(mce, &ct, j, &ps);
			for (k=0; k<ps.param_count; k++) {
				mceconfig_paramset_param(mce, &ps, k, p);
				stat_item((unsigned long) &s, &m);
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
