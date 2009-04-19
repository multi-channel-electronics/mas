#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

#define USAGE_MESSAGE \
"Usage:\n\t%s [options]\n"\
"  -M <msg>               preamble/postamble output\n"\
"  -L <iters>             new loop\n"\
"  -V <start> <incr>      new incrementing value (associated to last -L)\n"\
"  -P <string> <repeat>   new output string (associated to last -V)\n"\
"  -s                     output a status block describing the ramp instead of the ramp itself\n"\
"\n"\
"  -d <device file>       choose a particular mce device\n"\
"  -c <config file>       choose a particular mce config file\n"\
"  -f <batch file>        run commands from file instead of stdin\n"\
""

int process_options(options_t *options, int argc, char **argv)
{
	int option;

	amble_t* amble = NULL;
	loop_t* loop = NULL;
	value_t* value = NULL;
	operation_t* operation = NULL;

	while ( (option = getopt(argc, argv, "?hf:c:d:o:M:L:VPs")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf(USAGE_MESSAGE,
			       argv[0]);
			return -1;
/*
		case 'c':
			strcpy(options->config_file, optarg);
			break;

		case 'd':
			strcpy(options->cmd_device, optarg);
			break;
*/
		case 'o':
			options->output_file_now = 1;
			strcpy(options->output_file, optarg);
			break;

		case 's':
			options->status_block = 1;
			break;

		case 'M':
			if (amble==NULL) amble = options->ambles;
			else amble++;
			if (loop==NULL && options->postambles==NULL) {
				if (options->preambles==NULL) options->preambles = amble;
				options->preamble_count++;
			} else {
				if (options->postambles==NULL) {
					options->postambles = amble;
					loop = NULL;
				}
				options->postamble_count++;
			}
			strcpy((*amble), optarg);
			break;

		case 'L':
			if (options->postambles!=NULL) {
				fprintf(stderr, "-L error: new loop after postamble\n");
				exit(1);
			}
			if (loop==NULL) loop=options->loops;
			else {
				loop->sub_loop = loop+1;
				loop++;
			}
			loop->count = atoi(optarg);
			break;

		case 'V':
			if (loop==NULL) {
				fprintf(stderr, "-V error: no loop defined\n");
				exit(1);
			}
			if (value==NULL) value=options->values;
			else value++;
			if (loop->values == NULL) loop->values=value;
			value->start = atoi(argv[optind++]);
			value->step  = atoi(argv[optind++]);
			loop->value_count++;
			break;

		case 'P':
			if (loop==NULL || loop->values==NULL) {
				fprintf(stderr, "-P error: no value defined\n");
				exit(1);
			}
			if (operation==NULL) operation=options->operations;
			else operation++;
			if (value->operations==NULL) value->operations = operation;
			operation->command = argv[optind++];
			operation->repeat = atoi(argv[optind++]);
			value->operation_count++;
			break;

		default:
			fprintf(stderr, "Unimplemented option '-%c'!\n", option);
		}
	}

	return 0;
}


