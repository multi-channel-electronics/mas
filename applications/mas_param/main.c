#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "mas_param.h"
#include "masconfig.h"

options_t options = {
	config_file: "/etc/mce/mas.cfg",
	source_file: "/data/cryo/current_data/experiment.cfg",
	mode:        MODE_IDLE,
	format:      FORMAT_BASH,
};

int main(int argc, char **argv)
{
	config_t cfg;

	if (process_options(&options, argc, argv))
		return 1;

	// Load the target config file
	if (mas_load(options.source_file, &cfg) != 0)
		return 1;
	options.root = config_root_setting (&cfg);

	switch(options.mode) {
	case MODE_CRAWL:
		crawl_now(&options);
		break;

	case MODE_GET:
		param_report(&options);
		break;

	case MODE_INFO:
		param_report_info(&options);
		break;

	case MODE_SET:
		param_save(&options);
		config_write_file(&cfg, options.source_file);
		break;

	case MODE_IDLE:
		break;

	default:
		fprintf(stderr, "Unexpected mode specification.\n");
		return 1;
	}

	config_destroy(&cfg);

	return 0;
}
