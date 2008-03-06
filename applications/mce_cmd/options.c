#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

#define USAGE_MESSAGE \
"Usage:\n\t%s [options]\n"\
"  -i                     interactive mode, don't exit on errors\n"\
"  -q                     quiet mode, only print errors or output data\n"\
"  -p                     prefix-supression, don't print line number\n"\
"  -e                     echo mode, print each command as well as response\n"\
"  -r                     don't use readline on stdin (faster input in scripts)\n"\
"\n"\
"  -d <device file>       choose a particular mce device\n"\
"  -c <config file>       choose a particular mce config file\n"\
"  -f <batch file>        run commands from file instead of stdin\n"\
"  -x <command>           execute a command now\n"\
"  -C <data file>         DAS compatibility mode\n"\
"  -o <directory>         data file path\n"\
""

//"  -a <filename> <rc>     configure acquisition\n"
//"  -g <frames>            go now! (only valid with -a option)\n"

int process_options(options_t *options, int argc, char **argv)
{
	char *s;
	int option;
	while ( (option = getopt(argc, argv, "?hiqepf:c:d:C:ro:xag:")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf(USAGE_MESSAGE,
			       argv[0]);
			return -1;

		case 'i':
			options->interactive = 1;
			break;

		case 'q':
			options->nonzero_only = 1;
			break;

		case 'e':
			options->echo = 1;
			break;

		case 'p':
			options->no_prefix = 1;
			break;

		case 'f':
			strcpy(options->batch_file, optarg);
			options->batch_now = 1;
			break;

		case 'c':
			strcpy(options->config_file, optarg);
			break;

		case 'd':
			strcpy(options->cmd_device, optarg);
			break;

		case 'o':
			strcpy(options->acq_path, optarg);
			break;

		case 'C':
			options->das_compatible = 1;
			strcpy(options->acq_filename, optarg);
			break;

		case 'r':
			options->use_readline = 0;
			break;

/* 		case 'g': */
/* 			options->acq_now = 1; */
/* 			options->acq_frames = strtol(optarg, NULL, 0); */
/* 			break; */

		case 'x':
			s = options->cmd_command;
			while (optind < argc) {
				s += sprintf(s, "%s ", argv[optind++]);
			}
			options->cmd_now = 1;
			break;

		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}

	// Readline has to be off when reading from a file stream!
	if (options->batch_now)
		options->use_readline = 0;

	return 0;
}


