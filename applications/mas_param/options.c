/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mas_param.h"

enum {	GET, SET, INFO, BASH, CSH, IDLT, FULL };

typedef struct {
	char *name;
	int  id;
} string_table_t;

string_table_t commands[] = {
	{"get"    , GET },
	{"info"   , INFO },
	{"set"    , SET },
	{"bash"   , BASH },
	{"csh"    , CSH },
	{"idl_template", IDLT },
    {"full", FULL },
	{NULL,-1}
};


/* String table functions */

/*
static
int st_get_id(const string_table_t *st, const char *name, int *id);
*/

static
int st_index(const string_table_t *st, const char *name);

#if !MULTICARD
# define USAGE_OPTION_N "  -n <card>              ignored\n"
#else
# define USAGE_OPTION_N "  -n <card>              fibre card number\n"
#endif

#define USAGE_MESSAGE \
"Usage:\n\tmas_param [options] <command>\n\n"\
"Commands:\n"\
"  bash [prefix]           output data as bash variable declarations\n"\
"  csh [prefix]            output data as csh variable declarations\n"\
"  idl_template <suffix>   output idl code for the target format\n"\
"  info [param]            print type info for param (or all params)\n"\
"  get <param>             output value of the variable <param>\n"\
"  set <param> [datum]...  set the value of variable <param>\n"\
"  full                    output full type and data for all params\n"\
"\nOptions:\n"\
USAGE_OPTION_N \
"  -s <source file>       config file to parse.  Default:\n"\
"                           %s\n"\
"  -m <mas config>        choose a particular mas config file.\n"\
"  -f <output filename>   filename for output (stdout/source file by default)\n"\
"\n"\
"  -v                     print version string and exit\n"\
""

void usage(void)
{
  const char * dflt_exp_cfg =
    mcelib_default_experimentfile(-1);
  printf(USAGE_MESSAGE, dflt_exp_cfg);
  exit(1);
}

int process_options(options_t* options, int argc, char **argv)
{
	int option;
#if MULTICARD
	char *s;
#endif

	while ( (option = getopt(argc, argv, "+?hm:n:f:s:v")) >=0) {

		switch(option) {
		case '?':
		case 'h':
            usage(); /* doesn't return */

		case 'm':
			if (options->config_file)
				free(options->config_file);
			options->config_file = strdup(optarg);
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

		case 'f':
			strcpy(options->output_file, optarg);
			options->output_on = 1;
			break;

		case 's':
			if (options->source_file)
				free(options->source_file);
			options->source_file = strdup(optarg);
			break;

		case 'v':
			printf("This is %s, version %s\n",
			       PROGRAM_NAME, VERSION_STRING);
			break;

		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}

	// Process command words
	int cmd_idx = -1;
	if (optind >= argc || (cmd_idx=st_index(commands, argv[optind])) < 0 ) {
        usage();
	}	
	switch (commands[cmd_idx].id) {
	case BASH:
		options->mode = MODE_CRAWL;
		options->format = FORMAT_BASH;
		if (optind + 1 >= argc) {
			options->output_word[0] = 0;
		} else {
			strcpy(options->output_word, argv[optind+1]);
		}
		break;
	case CSH:
		options->mode = MODE_CRAWL;
		options->format = FORMAT_CSH;
		if (optind + 1 >= argc) {
			options->output_word[0] = 0;
		} else {
			strcpy(options->output_word, argv[optind+1]);
		}
		break;
	case IDLT:
		options->mode = MODE_CRAWL;
		options->format = FORMAT_IDLTEMPLATE;
		if (optind + 1 >= argc) {
			fprintf(stderr, "Command '%s' takes at least one argument.\n",
				commands[cmd_idx].name);
			return -1;
		}
		strcpy(options->output_word, argv[optind+1]);
		break;
	case GET:
		options->mode = MODE_GET;
		if (optind + 1 >= argc) {
			fprintf(stderr, "Command '%s' takes at least one argument.\n",
				commands[cmd_idx].name);
			return -1;
		}
		options->param_name = argv[optind+1];
		break;
	case INFO:
        // Two possible modes depending on presence of param_name
		if (optind + 1 >= argc) {
            // No argument, crawl and describe
            options->mode = MODE_CRAWL;
            options->format = FORMAT_INFO;
        } else {
            // Argument, describe single parameter only
            options->mode = MODE_INFO;
            options->param_name = argv[optind+1];
        }
		break;
    case FULL:
        options->mode = MODE_CRAWL;
        options->format = FORMAT_FULL;
        break;
	case SET:
		options->mode = MODE_SET;
		if (optind + 2 >= argc) {
			fprintf(stderr, "Command '%s' takes at least two arguments.\n",
				commands[cmd_idx].name);
			return -1;
		}
		options->param_name = argv[optind+1];
		options->data_source = argv + optind + 2;
		options->data_count = argc - optind - 2;
		break;
	default:
		fprintf(stderr, "Unimplemented command '%s'\n", argv[optind]);
		return -1;
	}

    // Set defaults
    if (options->config_file == NULL) {
        options->config_file = mcelib_default_masfile();
        if (options->config_file == NULL) {
            fprintf(stderr, "Unable to obtain path to default mas.cfg!\n");
            return -1;
        }
    }
    if (options->source_file == NULL) {
        options->source_file = mcelib_default_experimentfile(options->fibre_card);
        if (options->source_file == NULL) {
            fprintf(stderr, "Unable to obtain path to default experiment.cfg!\n");
            return -1;
        }
    }

	return 0;
}

int st_index(const string_table_t *st, const char *name)
{
	int i = 0;
	while (st[i].name != NULL)
		if (strcmp(name, st[i++].name)==0)
			return i-1;
	return -1;
}

/*
int st_get_id(const string_table_t *st, const char *name, int *id)
{
	int index = st_index(st, name);
	if (index < 0) return -1;
	
	*id = st[index].id;
	return 0;
}
*/
