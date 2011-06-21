/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/*! \file mce_status.c
 *
 *  \brief Program to read and record the status of the MCE.
 *
 *  How about we make this a generic tree traverser that you can plug
 *  your own operations into.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "mce_status.h"
#include "das.h"
#include "mas.h"
#include "cfg_dump.h"

options_t options = {
	.fibre_card = -1,
	.config_file = NULL,
	.hardware_file = NULL,
	.mode = CRAWLER_DAS,
};


void error_log_exit(maslog_t maslog, const char *msg, int error);

int crawl_festival(crawler_t *crawler);

int main(int argc, char **argv)
{
    maslog_t maslog;
	char msg[MCE_LONG];

    if (process_options(&options, argc, argv)) {
        fprintf(stderr, "invalid arguments.");
        return 2;
    }

	// Connect to MCE
    if ((options.context = mcelib_create(options.fibre_card,
          options.config_file))==NULL)
    {
        error_log_exit(&maslog, "failed to create mce library structure", 3);
	}

    maslog = maslog_connect(options.context, "mce_status");
    sprintf(msg, "initiated with hardware config '%s'", options.hardware_file);
    maslog_print(maslog, msg);

	if (mcecmd_open(options.context, options.device_file) != 0) {
		sprintf(msg, "Could not open mce device '%s'\n",
			options.device_file);
        error_log_exit(maslog, msg, 3);
	}

	// Load configuration
    if (mceconfig_open(options.context, options.hardware_file, "hardware")!=0) {
		sprintf(msg, "Could not load MCE config file '%s'.\n",
			options.hardware_file);
        error_log_exit(maslog, msg, 3);
	}


	// Set up the action handler
	crawler_t crawler;

	switch (options.mode) {

	case CRAWLER_DAS:
		das_crawler(&options, &crawler);
		break;

	case CRAWLER_MAS:
		mas_crawler(&options, &crawler);
		break;

	case CRAWLER_CFG:
		cfg_crawler(&options, &crawler);
		break;

	default:
		fprintf(stderr, "Craler not implemented.\n");
		return 1;
	}

	// Loop through cards and parameters
	if (crawl_festival(&crawler) != 0)
        error_log_exit(maslog, "failed commands", 3);

    maslog_print(maslog, "successful");
	return 0;
}

int crawl_festival(crawler_t *crawler)
{
	int i,j;
	int n_cards = mceconfig_card_count(options.context);

    if (options.output_path[0] != 0 &&
	    chdir(options.output_path)!=0) {
		fprintf(stderr, "Failed to move to working directory '%s'\n",
			options.output_path);
		return -1;
	}

	if (crawler->init != NULL &&
	    crawler->init(crawler->user_data, &options)!=0 ) {
		fprintf(stderr, "Crawler failed to initialize.\n");
	}

	for (i=0; i<n_cards; i++) {
		mce_param_t m;
		card_t *c = &m.card;
		param_t *p = &m.param;

		if (mceconfig_card(options.context, i, c)) {
			fprintf(stderr, "Problem loading card data at index %i\n", i);
			return -1;
		}

		for (j=0; mceconfig_card_param(options.context, c, j, p)==0; j++) {
			if (crawler->item != NULL)
				crawler->item(crawler->user_data, &m);
		}
	}

	if (crawler->cleanup != NULL &&
	    crawler->cleanup(crawler->user_data)!= 0) {
		return 1;
	}

	return 0;
}

void error_log_exit(maslog_t maslog, const char *msg, int error)
{
    maslog_print(maslog, msg);
	fprintf(stderr, "%s", msg);
	exit(error);
}
