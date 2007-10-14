/*

  User interfaces are only fun to write when the user is also a machine

*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <mcedsp.h>

#include <cmdtree.h>

#define CASE_INSENSITIVE

#define LINE_LEN 1024


#define DEFAULT_DEVICE "/dev/mce_dsp0"

int handle;


/* Extend the cmdtree enum for option type */

enum {
	COMMAND = CMDTREE_USERBASE,
	SPECIAL,
	DATA
};


/* Our custom commands ("SPECIAL") get their own enum */

enum {
	COMMENT,
	SLEEP,
	RESET_FLAGS
};


cmdtree_opt_t mem_opts[] = {
	{"X", DATA, DSP_MEMX, 0, -1, NULL},
	{"Y", DATA, DSP_MEMY, 0, -1, NULL},
	{"P", DATA, DSP_MEMP, 0, -1, NULL},
	{"", CMDTREE_TERMINATOR, 0, 0, 0, NULL}
};

cmdtree_opt_t qt_opts[] = {
	{"BASE"  , DATA, DSP_QT_BASE  , 2, 2, NULL},
	{"DELTA" , DATA, DSP_QT_DELTA , 1, 1, NULL},
	{"NUMBER", DATA, DSP_QT_NUMBER, 1, 1, NULL},
	{"INFORM", DATA, DSP_QT_INFORM, 1, 1, NULL},
	{"SIZE"  , DATA, DSP_QT_SIZE  , 1, 1, NULL},
	{"TAIL"  , DATA, DSP_QT_TAIL  , 1, 1, NULL},
	{"FLUSH" , DATA, DSP_QT_FLUSH , 1, 1, NULL},
	{"ENABLE", DATA, DSP_QT_ENABLE, 1, 1, NULL},
	{"", CMDTREE_TERMINATOR, 0, 0, 0, NULL}
};

cmdtree_opt_t root_opts[] = {
	{"READ"     , COMMAND, DSP_RDM, 2, 2, mem_opts},
	{"WRITE"    , COMMAND, DSP_WRM, 3, 3, mem_opts},
	{"START"    , COMMAND, DSP_GOA, 0, 0, NULL},
	{"STOP"     , COMMAND, DSP_STP, 0, 0, NULL},
	{"RESET"    , COMMAND, DSP_RST, 0, 0, NULL},
	{"RESET_MCE", COMMAND, DSP_RCO, 0, 0, NULL},
	{"QT_SET"   , COMMAND, DSP_RDM, 1,-1, qt_opts},
	{"#"        , SPECIAL, COMMENT, 0, 0, NULL},
	{""         , CMDTREE_TERMINATOR, 0, 0, 0, NULL}
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


int  process_command(cmdtree_opt_t *src_opts, int *args, int nargs,
			    char *errmsg);

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

		int args[25];
		int err = 0;
		int count = cmdtree_decode(line, args, 25, root_opts, errmsg);
		if (count == 0) {
			if (options.interactive) {
				cmdtree_list(errmsg + strlen(errmsg),
					     root_opts, "DSP_CMD expects "
					     "command from [ ", " ", "]");
				err = 1;
			} else {
				err = 0;
			}
		} else if (count < 0) {
			err = -1;
		} else {
			err = process_command(root_opts, args, count, errmsg);
		}


		if (err > 0) {
			if (*errmsg == 0 && !options.nonzero_only)
				printf("%sok\n", premsg);
			else 
				printf("%sok : %s\n", premsg, errmsg);
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


int process_command(cmdtree_opt_t *src_opts, int *arg_opt, int nargs, char *errmsg)
{
	int ret_val = 0;
	int err;

	int arg_id[25] = {0};
	cmdtree_translate(arg_id, arg_opt, nargs, src_opts);
	
	switch (src_opts[arg_opt[0]].type) {
	case COMMAND:

		switch( arg_id[0] ) {
		
		case DSP_RDM:
			err = dsp_read_word(handle, arg_id[1], arg_id[2]);
			if (err >= 0) {
				sprintf(errmsg, "%s[%#x] = %#x",
					src_opts[arg_opt[0]].sub_opts[arg_opt[1]].name,
					arg_opt[2], err);
				err = 0;
			}
			break;
    
		case DSP_WRM:
			err = dsp_write_word(handle, arg_id[1],
						 arg_id[2], arg_id[3]);
			break;

		case DSP_GOA:
			err = dsp_start_application(handle, arg_id[1]);
			break;
    
		case DSP_STP:
			err = dsp_stop_application(handle);
			break;

		case DSP_RST:
			err = dsp_reset(handle);
			break;

		case DSP_RCO:
			err = dsp_reset_mce(handle);
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

		break;


	case SPECIAL:

		switch( arg_id[0] ) {

		case COMMENT:
			break;

		case SLEEP:
			usleep(arg_opt[1]);
			break;

		case RESET_FLAGS:
			ret_val = dsp_reset_flags(handle);
			if (ret_val < 0) break;
			break;

		default:
			sprintf(errmsg, "command not implemented");
		}

		break;

	default:
		sprintf(errmsg, "Unknown root command type!");
	}

	return ret_val;
}
