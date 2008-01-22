#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mas_param.h"

enum {	GET, SET, INFO, BASH, CSH, IDLT };

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
	{NULL,-1}
};


/* String table functions */

/*
static
int st_get_id(const string_table_t *st, const char *name, int *id);
*/

static
int st_index(const string_table_t *st, const char *name);


#define USAGE_MESSAGE \
"Usage:\n\t%s [options] [command]\n\n"\
"Commands:\n"\
"  bash [<prefix>]         output data as bash variable declarations\n"\
"  csh [<prefix>]          output data as csh variable declarations\n"\
"  idl_template <suffix>   output idl code for the target format\n"\
"  get <param>             output value of the variable <param>\n"\
"  set <param> [ data...]  set the value of variable <param>\n\n"\
"Options:\n"\
"  -s <source file>       config file to parse\n"\
"  -m <mas config>        choose a particular mas config file\n"\
"  -f <output filename>   filename for output (stdout/source file by default)\n"\
"\n"\
"  -v                     print version string and exit\n"\
""

int process_options(options_t* options, int argc, char **argv)
{
	int i;
	int option;
	while ( (option = getopt(argc, argv, "?hm:f:s:v")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf(USAGE_MESSAGE, argv[0]);
			return -1;

		case 'm':
			strcpy(options->config_file, optarg);
			break;

		case 'f':
			strcpy(options->output_file, optarg);
			options->output_on = 1;
			break;

		case 's':
			strcpy(options->source_file, optarg);
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
		fprintf(stderr, "Specify one of:");
		for (i=0; commands[i].name != NULL; i++)
			fprintf(stderr, " %s", commands[i].name);
		fprintf(stderr, "\n");
		return 1;
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
		options->mode = MODE_INFO;
		if (optind + 1 >= argc) {
			fprintf(stderr, "Command '%s' takes at least one argument.\n",
				commands[cmd_idx].name);
			return -1;
		}
		options->param_name = argv[optind+1];
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
