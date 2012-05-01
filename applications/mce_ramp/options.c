/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

#if !MULTICARD
#  define USAGE_OPTION_N "  -n <card number>       ignored\n"
#else
#  define USAGE_OPTION_N "  -n <card number>       use the specified fibre card\n"
#endif

#define USAGE_MESSAGE \
"Usage:\n\t%s [options]\n"\
"  -M <msg>               preamble/postamble output\n"\
"  -L <iters>             new loop\n"\
"  -V <start> <incr>      new incrementing value (associated to last -L)\n"\
"  -P <string> <repeat>   new output string (associated to last -V)\n"\
"\n"\
"  -f <batch file>        run commands from file instead of stdin\n"\
"  -m <mas file>          choose a particular mas config file\n"\
USAGE_OPTION_N \
"  -s                     output a status block describing the ramp instead of the ramp itself\n"\
""

int process_options(options_t *options, int argc, char **argv)
{
	int option;
#if MULTICARD
    char *s;
#endif

	amble_t* amble = NULL;
	loop_t* loop = NULL;
	value_t* value = NULL;
	operation_t* operation = NULL;

    while ((option = getopt(argc, argv, "?hf:m:n:o:M:L:VPs")) >= 0) {

		switch(option) {
		case '?':
		case 'h':
			printf(USAGE_MESSAGE,
			       argv[0]);
			return -1;

        case 'm':
            if (options->mas_file)
                free(options->mas_file);
            options->mas_file = strdup(optarg);
            break;
            
        case 'n':
#if MULTICARD
            options->fibre_card = (int)strtol(optarg, &s, 10);
            if (*optarg == '\0' || *s != '\0' || options->fibre_card < 0 ||
                    options->fibre_card >= MAX_FIBRE_CARD)
            {
                fprintf(stderr, "%s: invalid fibre card number: %s\n", argv[0],
                        optarg);
                return -1;
            }
#endif
            break;

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


