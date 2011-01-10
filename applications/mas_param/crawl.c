#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mas_param.h"

int crawl_now(options_t *options)
{
	// Set up the action handler
	crawler_t crawler;

	switch (options->format) {

	case FORMAT_BASH:
		bash_crawler(options, &crawler);
		break;

	case FORMAT_CSH:
	       	csh_crawler(options, &crawler);
		break;

	case FORMAT_IDLTEMPLATE:
		idl_crawler(options, &crawler);
		break;

	case FORMAT_INFO:
		info_crawler(options, &crawler);
		break;

	case FORMAT_FULL:
		full_crawler(options, &crawler);
		break;

	default:
		fprintf(stderr, "Unexpected crawler encountered.\n");
		return 1;
	}

	// Loop through cards and parameters
	while (crawler.passes--) {
		if (crawl_festival(&crawler, options) != 0)
			return 1;
	}

	return 0;
}


int crawl_festival(crawler_t *crawler, options_t *options)
{
	int i;

       	int n_root = config_setting_length(options->root);

	if (options->output_path[0] != 0 &&
	    chdir(options->output_path)!=0) {
		fprintf(stderr, "Failed to move to working directory '%s'\n",
			options->output_path);
		return -1;
	}

	if (crawler->init != NULL &&
	    crawler->init(crawler->user_data, options)!=0 ) {
		fprintf(stderr, "Crawler failed to initialize.\n");
	}

	for (i=0; i<n_root; i++) {

		config_setting_t *cfg = config_setting_get_elem(options->root, i);
		if (cfg == NULL) {
			fprintf(stderr, "Null entry in %s at %i!\n", options->source_file, i);
			continue;
		}
		mas_param_t m;
		if (param_get(cfg, &m))
			continue;

		if (crawler->item != NULL)
			crawler->item(crawler->user_data, &m);
	}

	if (crawler->cleanup != NULL &&
	    crawler->cleanup(crawler->user_data)!= 0) {
		return 1;
	}

	return 0;
}
