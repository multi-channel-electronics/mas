#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <mcedsp.h>

#include <cmdtree.h>

#define LINE_LEN 1024
#define NARGS 64

#define CASE_INSENSITIVE

#define DEFAULT_DEVICE "/dev/mce_dsp0"

int handle;


enum {
	ENUM_COMMAND_LOW,
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
	{ CMDTREE_SELECT, "X", 0,-1, DSP_MEMX, integer_opts},
	{ CMDTREE_SELECT, "Y", 0,-1, DSP_MEMY, integer_opts},
	{ CMDTREE_SELECT, "P", 0,-1, DSP_MEMP, integer_opts},
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL}
};

cmdtree_opt_t qt_opts[] = {
	{ CMDTREE_SELECT, "BASE"  , 1,1, DSP_QT_BASE  , integer_opts},
	{ CMDTREE_SELECT, "DELTA" , 1,1, DSP_QT_DELTA , integer_opts},
	{ CMDTREE_SELECT, "NUMBER", 1,1, DSP_QT_NUMBER, integer_opts},
	{ CMDTREE_SELECT, "INFORM", 1,1, DSP_QT_INFORM, integer_opts},
	{ CMDTREE_SELECT, "SIZE"  , 1,1, DSP_QT_SIZE  , integer_opts},
	{ CMDTREE_SELECT, "TAIL"  , 1,1, DSP_QT_TAIL  , integer_opts},
	{ CMDTREE_SELECT, "HEAD"  , 1,1, DSP_QT_HEAD  , integer_opts},
	{ CMDTREE_SELECT, "DROPS" , 1,1, DSP_QT_DROPS , integer_opts},
	{ CMDTREE_SELECT, "FLUSH" , 1,1, DSP_QT_FLUSH , integer_opts},
	{ CMDTREE_SELECT, "ENABLE", 1,1, DSP_QT_ENABLE, integer_opts},
	{ CMDTREE_SELECT, "TON" ,0,0, 10, NULL},
	{ CMDTREE_SELECT, "TOFF",0,0, 11, NULL},
	{ CMDTREE_TERMINATOR, "", 0,0, 0, NULL}
};

cmdtree_opt_t root_opts[] = {
	{ CMDTREE_SELECT, "READ"     , 2,2, COMMAND_RDM, mem_opts},
	{ CMDTREE_SELECT, "WRITE"    , 3,3, COMMAND_WRM, mem_opts},
	{ CMDTREE_SELECT, "START"    , 0,0, COMMAND_GOA, integer_opts},
	{ CMDTREE_SELECT, "STOP"     , 0,0, COMMAND_STP, integer_opts},
	{ CMDTREE_SELECT, "RESET"    , 0,0, COMMAND_RST, integer_opts},
	{ CMDTREE_SELECT, "RESET_MCE", 0,0, COMMAND_RCO, integer_opts},
	{ CMDTREE_SELECT, "QT_SET"   , 0,-1, COMMAND_QTS, qt_opts},
	{ CMDTREE_SELECT, "#"        , 0,-1, SPECIAL_COMMENT, anything_opts},
	{ CMDTREE_TERMINATOR, "", 0,0, 0, NULL}
};

struct {
	int interactive;
	int nonzero_only;
	int no_prefix;

	char batch_file[LINE_LEN];
	int  batch_now;

	char cmd_command[LINE_LEN];
	int  cmd_now;

	char device_file[LINE_LEN];

} options = {
	0, 0, 0, "", 0, "", 0, DEFAULT_DEVICE
};


int process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg);

int  process_options(int argc, char **argv);

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

	if (process_options(argc, argv)) return 1;

	handle = dsp_open(options.device_file);
	if (handle<0) {
		printf("Could not open device %s\n", options.device_file);
		exit(1);
	}

	

	while (!done) {

		unsigned int n = LINE_LEN;

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
#ifdef CASE_INSENSITIVE
		uppify(line);
#endif

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

	dsp_close(handle);

	return err;
}


int  process_options(int argc, char **argv)
{
	int option;
	char *s;
	while ( (option = getopt(argc, argv, "?hiqpf:d:x")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf("Usage:\n\t%s [-i] [-q] [-p] [-d devfile] "
			       "[-f <batch file> | -x <command>]\n",
			       argv[0]);
			return -1;

		case 'i':
			options.interactive = 1;
			break;

		case 'q':
			options.nonzero_only = 1;
			break;

		case 'p':
			options.no_prefix = 1;
			break;

		case 'f':
			strcpy(options.batch_file, optarg);
			options.batch_now = 1;
			break;

		case 'd':
			strcpy(options.device_file, optarg);
			break;

		case 'x':
			s = options.cmd_command;
			while (optind < argc) {
				s += sprintf(s, "%s ", argv[optind++]);
			}
			options.cmd_now = 1;
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
		
		case COMMAND_RDM:
			err = dsp_read_word(handle, tokens[1].value, tokens[2].value);
			if (err >= 0) {
				sprintf(errmsg, "%s[%#x] = %#x",
					dsp_memtoa(tokens[1].value),
					tokens[2].value, err);
				err = 0;
			}
			break;
    
		case COMMAND_WRM:
			err = dsp_write_word(handle, tokens[1].value,
						 tokens[2].value, tokens[3].value);
			if (err >= 0) err = 0;
			break;

		case COMMAND_GOA:
			err = dsp_start_application(handle, tokens[1].value);
			if (err >= 0) err = 0;
			break;
    
		case COMMAND_STP:
			err = dsp_stop_application(handle);
			if (err >= 0) err = 0;
			break;

		case COMMAND_RST:
			err = dsp_reset(handle);
			if (err >= 0) err = 0;
			break;

		case COMMAND_RCO:
			err = dsp_reset_mce(handle);
			if (err >= 0) err = 0;
			break;
    
		case COMMAND_QTS:
			err = dsp_qt_set(handle,
					 tokens[1].value,
					 tokens[2].value,
					 tokens[3].value);
			if (err >= 0) err = 0;
			break;
		default:
			sprintf(errmsg, "command not implemented");
			return -1;
		}

		if (err==0)
			ret_val = 1;
		else {
			sprintf(errmsg, "dsp library error %#06x", err);
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
			ret_val = dsp_reset_flags(handle);
			if (ret_val < 0) break;
			break;

		default:
			sprintf(errmsg, "command not implemented");
		}
	}

	return ret_val;
}
