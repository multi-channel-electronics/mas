/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <mce/defaults.h>

#include "options.h"

#define USAGE_MESSAGE \
"Usage:\n"\
"\t%s [options] [-x cmd...]\n"\
"  -i                     interactive mode, don't exit on errors\n"\
"  -q                     quiet mode, only print errors or output data\n"\
"  -p                     prefix-supression, don't print line number\n"\
"  -e                     echo mode, print each command as well as response\n"\
"  -r                     don't use readline on stdin (faster input in scripts)\n"\
"\n"\
"  -c <config file>       choose a particular mce config file\n"\
"  -m <config file>       choose a particular mas config file\n"\
"  -n <dev index>         choose a specific MCE device to use\n"\
"  -f <batch file>        run commands from file instead of stdin\n"\
"  -X \"cmd string\"        execute this command and exit (these can be stacked)\n"\
"  -o <directory>         data file path\n"\
"\n"\
"  -x <command>           execute remainder of line as an mce_cmd command\n"\
"                         The -X command may be more useful as it allows multiple\n"\
"                         commands to be executed, e.g.\n"\
"                              mce_cmd -X \"acq_config test_1 rc1\" -X \"acq_go 100\"\n"\
"\n"\
"  -v                     print version string and exit\n"\
""

int process_options(options_t *options, int argc, char **argv)
{
	char *s = NULL;
	int option;
    while ( (option = getopt(argc, argv, "?X:c:m:ef:hin:o:pqrvx")) >=0)
	{

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

        case 'm':
            if (options->mas_file)
                free(options->mas_file);
            options->mas_file = strdup(optarg);
            break;

		case 'c':
            if (options->hardware_file)
                free(options->hardware_file);
			options->hardware_file = strdup(optarg);
			break;

		case 'o':
			strcpy(options->acq_path, optarg);
			break;

		case 'r':
			options->use_readline = 0;
			break;

		case 'X':
			options->cmd_set[options->cmds_now++] = optarg;
            break;

		case 'x':
			s = (char*)malloc(LINE_LEN);
			options->cmd_set[options->cmds_now++] = s;

			while (optind < argc) {
				s += sprintf(s, "%s ", argv[optind++]);
			}
			break;

		case 'v':
			options->version_only = 1;
			break;

		case 'n':
			options->fibre_card = (int)strtol(optarg, &s, 10);
			if (*optarg == '\0' || *s != '\0' || options->fibre_card < 0) {
				fprintf(stderr, "%s: invalid fibre card number\n", argv[0]);
				return -1;
			}
			break;

		default:
			fprintf(stderr, "Unimplemented option '-%c'!\n", option);
		}
	}

	// Check for stragglers (these are files we should be reading...)
	if (optind < argc) {
        fprintf(stderr,
                "Stray arguments!  Use '%s -f <file>' to execute a script.\n",
                argv[0]);
		return -1;
	}

	// Readline has to be off when reading from a file stream!
	if (options->batch_now)
		options->use_readline = 0;

	return 0;
}
