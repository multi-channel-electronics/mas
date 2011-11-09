/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "mas_param.h"
#include "masconfig.h"

options_t options = {
	.mode = MODE_IDLE,
	.format = FORMAT_BASH,
    .fibre_card = MCE_DEFAULT_MCE
};

int main(int argc, char **argv)
{
	config_t cfg;
	int status = 0;

	if (process_options(&options, argc, argv))
		return 1;

	// Load the target config file
    if (options.source_file == NULL) {
        mce_context_t mce = mcelib_create(options.fibre_card,
                options.config_file, 0);
        options.source_file = mcelib_default_experimentfile(mce);
        mcelib_destroy(mce);
    }
	if (mas_load(options.source_file, &cfg) != 0)
		return 1;
	options.root = config_root_setting (&cfg);

	switch(options.mode) {
	case MODE_CRAWL:
		status = crawl_now(&options);
		break;

	case MODE_GET:
		status = param_report(&options);
		break;

	case MODE_INFO:
		status = param_info(&options);
		break;

	case MODE_SET:
		status = param_save(&options);
		if ((status == 0) &&
		    (config_write_file(&cfg, options.source_file) != CONFIG_TRUE )) {
			fprintf(stderr, "Could not save to '%s'.\n",
				options.source_file);
				status = 1;
		}
		break;

	case MODE_IDLE:
		break;

	default:
		fprintf(stderr, "Unexpected mode specification.\n");
		return 1;
	}

	config_destroy(&cfg);

	return status ? 1 : 0;
}
