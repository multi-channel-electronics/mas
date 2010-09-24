#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mce_status.h"

#define USAGE_MESSAGE \
"Usage:\n\t%s [options]\n"\
"  -n <card number>       choose a particular fibre card\n"\
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
			strcpy(options->hardware_file, optarg);
			break;

		case 'n':
			options->fibre_card = (int)strtoul(optarg, &s, 10);
			if (*optarg == '\0' || *s != '\0' || options->fibre_card > 4) {
				fprintf(stderr, "%s: invalid fibre card number\n", argv[0]);
				return -1;
			}
			break;

		case 'm':
			strcpy(options->config_file, optarg);
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

	/* set fibre card devices */
	sprintf(options->device_file, "/dev/mce_cmd%i", options->fibre_card);

	return 0;
}
