#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mce_status.h"

#define USAGE_MESSAGE \
"Usage:\n\t%s [options]\n"\
"  -d <device file>       choose a particular mce device\n"\
"  -c <hardware config>   choose a particular mce config file\n"\
"  -m <mas config>        choose a particular mce config file\n"\
"  -o <output directory>  destination folder for output\n"\
"  -f <output filename>   filename for output (stdout by default)\n"\
"  -g                     dump parameter mapping\n"\
"\n"\
"  -v                     print version string and exit\n"\
""

int process_options(options_t* options, int argc, char **argv)
{
	int option;
	while ( (option = getopt(argc, argv, "?hd:c:m:o:f:gv")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf(USAGE_MESSAGE, argv[0]);
			return -1;

		case 'c':
			strcpy(options->hardware_file, optarg);
			break;

		case 'd':
			strcpy(options->device_file, optarg);
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

		case 'v':
			printf("This is %s, version %s, using mce library version %s\n",
			       PROGRAM_NAME, VERSION_STRING, mcelib_version());
			break;

		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}

	return 0;
}
