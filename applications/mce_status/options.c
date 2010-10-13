/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mce_status.h"

#if !MULTICARD
#  define USAGE_OPTION_N "  -n <card number>       ignored\n"
#else
#  define USAGE_OPTION_N "  -n <card number>       use the specified fibre card\n"
#endif

#define USAGE_MESSAGE \
"Usage:\n\t%s [options]\n"\
USAGE_OPTION_N \
"  -c <hardware config>   choose a particular mce config file\n"\
"  -m <mas config>        choose a particular mce config file\n"\
"  -o <output directory>  destination folder for output\n"\
"  -f <output filename>   filename for output (stdout by default)\n"\
"  -s                     snapshot style, civilized output\n"\
"  -g                     dump parameter mapping\n"\
"\n"\
"  -v                     print version string and exit\n"\
""

int process_options(options_t* options, int argc, char **argv)
{
	char *s;
	int option;
	while ( (option = getopt(argc, argv, "?hn:c:m:o:f:gvs")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf(USAGE_MESSAGE, argv[0]);
			return -1;

		case 'c':
            if (options->hardware_file)
                free(options->hardware_file);
            options->hardware_file = strdup(optarg);
			break;

		case 'n':
#if MULTICARD
			options->fibre_card = (int)strtol(optarg, &s, 10);
			if (*optarg == '\0' || *s != '\0' || options->fibre_card < 0) {
				fprintf(stderr, "%s: invalid fibre card number\n", argv[0]);
				return -1;
			}
#endif
			break;

		case 'm':
            if (options->config_file)
                free(options->config_file);
			options->config_file = strdup(optarg);
			break;

		case 'f':
			strcpy(options->output_file, optarg);
			options->output_on = 1;
			break;

		case 'o':
			strcpy(options->output_path, optarg);
			break;

		case 'g':
			options->mode = CRAWLER_CFG;
			break;

		case 's':
			options->mode = CRAWLER_MAS;
			break;

		case 'v':
			printf("This is %s, version %s, using mce library version %s\n",
			       PROGRAM_NAME, VERSION_STRING, mcelib_version());
			break;

		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}

    /* set defaults, if necessary */
    if ((options->cmd_device = mcelib_cmd_device(options->fibre_card)) == NULL) {
        fprintf(stderr, "Unable to obtain path to default command device!\n");
        return -1;
    }
    if (options->config_file == NULL) {
        options->config_file = mcelib_default_masfile(options->fibre_card);
        if (options->config_file == NULL) {
            fprintf(stderr, "Unable to obtain path to default mas.cfg!\n");
            return -1;
        }
    }
    if (options->hardware_file == NULL) {
        options->hardware_file = mcelib_default_hardwarefile(options->fibre_card);
        if (options->hardware_file == NULL) {
            fprintf(stderr, "Unable to obtain path to default mce.cfg!\n");
            return -1;
        }
    }

	return 0;
}
