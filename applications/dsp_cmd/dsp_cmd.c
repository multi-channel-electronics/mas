/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <mcedsp.h>

#include <mce/cmdtree.h>
#include <mce/defaults.h>

#define PROGRAM_NAME "dsp_cmd"

#define LINE_LEN 1024
#define NARGS 64

mce_context_t mce;

enum {
	ENUM_COMMAND_LOW,
	COMMAND_VER,
	COMMAND_RDM,
	COMMAND_WRM,
	COMMAND_GOA,
	COMMAND_STP,
	COMMAND_RST,
	COMMAND_RCO,
	COMMAND_QTS,
	ENUM_COMMAND_HIGH,
	SPECIAL_COMMENT,
	SPECIAL_SLEEP,
	SPECIAL_RESETFLAGS,
	DATA
};

cmdtree_opt_t anything_opts[] = {
	{ CMDTREE_INTEGER, "", 0, -1, 0, anything_opts },
	{ CMDTREE_STRING , "", 0, -1, 0, anything_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t integer_opts[] = {
	{ CMDTREE_INTEGER   , "", 0, -1, 0, integer_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t mem_opts[] = {
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "X", 0,-1, DSP_MEMX, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "Y", 0,-1, DSP_MEMY, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "P", 0,-1, DSP_MEMP, integer_opts},
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL}
};

cmdtree_opt_t qt_opts[] = {
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "BASE"  , 1,1, DSP_QT_BASE  , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "DELTA" , 1,1, DSP_QT_DELTA , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "NUMBER", 1,1, DSP_QT_NUMBER, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "INFORM", 1,1, DSP_QT_INFORM, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "SIZE"  , 1,1, DSP_QT_SIZE  , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "TAIL"  , 1,1, DSP_QT_TAIL  , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "HEAD"  , 1,1, DSP_QT_HEAD  , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "DROPS" , 1,1, DSP_QT_DROPS , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "FLUSH" , 1,1, DSP_QT_FLUSH , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ENABLE", 1,1, DSP_QT_ENABLE, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "TON" ,0,0, 10, NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "TOFF",0,0, 11, NULL},
	{ CMDTREE_TERMINATOR, "", 0,0, 0, NULL}
};

cmdtree_opt_t root_opts[] = {
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "VERS"     , 0,0, COMMAND_VER, mem_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "READ"     , 2,2, COMMAND_RDM, mem_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "WRITE"    , 3,3, COMMAND_WRM, mem_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "START"    , 0,0, COMMAND_GOA, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "STOP"     , 0,0, COMMAND_STP, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "RESET"    , 0,0, COMMAND_RST, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "RESET_MCE", 0,0, COMMAND_RCO, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "QT_SET"   , 0,-1, COMMAND_QTS, qt_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "#"        , 0,-1, SPECIAL_COMMENT, anything_opts},
	{ CMDTREE_TERMINATOR, "", 0,0, 0, NULL}
};

typedef struct option_struct {
	int interactive;
	int nonzero_only;
	int no_prefix;
    int dev_num;

	char batch_file[LINE_LEN];
	int  batch_now;

	char cmd_command[LINE_LEN];
	int  cmd_now;

    char *mas_file;
} option_t;

int process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg);

int  process_options(int argc, char **argv, option_t *options);

void uppify(char *s);

int main(int argc, char **argv)
{
	int done = 0;
	int err = 0;
	char errmsg[1024];
	char premsg[1024];

	int line_count = 0;
	FILE *fin = stdin;
	char *line = (char*) malloc(LINE_LEN);

    option_t options = {
        .dev_num = -1,
    };

    if (process_options(argc, argv, &options))
        return 1;

    if (!options.nonzero_only)
        printf("This is %s version %s\n", PROGRAM_NAME, VERSION_STRING);

    /* create a mce context, which will figure out the device URL */
    mce = mcelib_create(options.dev_num, options.mas_file, 0);
    if (mce == NULL) {
        fprintf(stderr, "Unable to create MCE library context.\n");
        return 1;
    }

    if (mcedsp_open(mce)) {
        fprintf(stderr, "Could not open dsp\n");
		exit(1);
	}

	while (!done) {

		size_t n = LINE_LEN;

		if ( options.cmd_now ) {
			strcpy(line, options.cmd_command);
			done = 1;
		} else {

			getline(&line, &n, fin);
			if (n==0 || feof(fin)) break;

			n = strlen(line);
			if (line[n-1]=='\n') line[--n]=0;
			line_count++;
		}

		if (options.no_prefix)
			premsg[0] = 0;
		else
			sprintf(premsg, "Line %3i : ", line_count);
		errmsg[0] = 0;

		cmdtree_token_t args[NARGS];
		args[0].n = 0;
		int err = 0;

		cmdtree_debug = 0;

		err = cmdtree_tokenize(args, line, NARGS);
		if (err) {
			strcpy(errmsg, "could not tokenize");
		}

		if (!err) {
			int count = cmdtree_select( args, root_opts, errmsg);

			if (count < 0) {
				err = -1;
			} else if (count == 0) {
				if (options.interactive || args->n > 0) {
					cmdtree_list(errmsg, root_opts,
                            "dsp_cmd expects argument from [ ", " ", "]");
					err = -1;
        }
			} else {
                err = process_command(root_opts, args, errmsg);
				if (err==0) err = 1;
			}
        }

		if (err > 0) {
			if (*errmsg == 0) {
				if (!options.nonzero_only)
					printf("%sok\n", premsg);
			} else {
				printf("%sok : %s\n", premsg, errmsg);
			}
		} else if (err < 0) {
			printf("%serror : %s\n", premsg, errmsg);
			if (options.interactive)
				continue;
			return 1;
		}
	}

	if (!options.cmd_now)
		printf("EOF after %i lines\n", line_count);

    mcedsp_close(mce);

	return err;
}


int  process_options(int argc, char **argv, option_t *options)
{
	int option;
	char *s;
	while ( (option = getopt(argc, argv, "?hiqpf:d:n:x")) >=0) {

		switch(option) {
		case '?':
		case 'h':
            printf("Usage:\n\t%s [-i] [-q] [-p] [-n devnum] [-m masfile] "
			       "[-f <batch file> | -x <command>]\n",
			       argv[0]);
			return -1;

		case 'i':
			options->interactive = 1;
			break;

		case 'q':
			options->nonzero_only = 1;
			break;

		case 'p':
			options->no_prefix = 1;
			break;

		case 'f':
			strcpy(options->batch_file, optarg);
			options->batch_now = 1;
			break;

        case 'n':
            options->dev_num = atoi(optarg);
            break;

        case 'm':
            if (options->mas_file)
                free(options->mas_file);
            options->mas_file = strdup(optarg);
			break;

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

	return 0;
}

void uppify(char *s) {
	if (s==NULL || *(s--)==0) return;
	while (*(++s)!=0) {
		if (*s<'a' || *s>'z') continue;
		*s += 'A' - 'a';
    }
}


int process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg)
{
	int ret_val = 0;
	int err;

    int is_command = (tokens[0].value >= ENUM_COMMAND_LOW &&
			  tokens[0].value < ENUM_COMMAND_HIGH);

	if (is_command) {

		switch( tokens[0].value ) {

		case COMMAND_VER:
            err = mcedsp_version(mce);
			if (err >= 0) {
				sprintf(errmsg, "version = %#x", err);
				err = 0;
			}
			break;

		case COMMAND_RDM:
            err = mcedsp_read_word(mce, tokens[1].value, tokens[2].value);
			if (err >= 0) {
				sprintf(errmsg, "%s[%#x] = %#x",
                        mcedsp_memtoa(tokens[1].value), tokens[2].value, err);
				err = 0;
			}
			break;

		case COMMAND_WRM:
            err = mcedsp_write_word(mce, tokens[1].value,
						 tokens[2].value, tokens[3].value);
			if (err >= 0) err = 0;
			break;

		case COMMAND_GOA:
            err = mcedsp_start_application(mce, tokens[1].value);
			if (err >= 0) err = 0;
			break;

		case COMMAND_STP:
            err = mcedsp_stop_application(mce);
			if (err >= 0) err = 0;
			break;

		case COMMAND_RST:
            err = mcedsp_reset(mce);
			if (err >= 0) err = 0;
			break;

		case COMMAND_RCO:
            err = mcedsp_reset_mce(mce);
			if (err >= 0) err = 0;
			break;

		case COMMAND_QTS:
            err = mcedsp_qt_set(mce,
					 tokens[1].value,
					 tokens[2].value,
					 tokens[3].value);
			if (err >= 0) err = 0;
			break;
		default:
			sprintf(errmsg, "command not implemented");
			return -1;
		}

		if (err!=0 && errmsg[0] == 0) {
			sprintf(errmsg, "dsp library error -%#08x : %s", -err,
                    mcedsp_error_string(err));
			ret_val = -1;
    }

	} else {

		switch( tokens[0].value ) {

		case SPECIAL_COMMENT:
			break;

		case SPECIAL_SLEEP:
			usleep(tokens[1].value);
			break;

		case SPECIAL_RESETFLAGS:
            ret_val = mcedsp_reset_flags(mce);
			if (ret_val < 0) break;
			break;

		default:
			sprintf(errmsg, "command not implemented");
		}
	}

	return ret_val;
}
