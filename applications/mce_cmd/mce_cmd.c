/*! \file mce_cmd.c
 *
 *  \brief Program to send commands to the MCE.
 *
 *  This program uses mce_library to send commands and receive
 *  responses from the MCE.  The program accepts instructions from
 *  standard input or from the command line using the -x option.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <mcecmd.h>
#include <mceconfig.h>

#include <cmdtree.h>

#define LINE_LEN 1024
#define NARGS 64

#define WHITE " \t"

#define CASE_INSENSITIVE

#define DEFAULT_DEVICE "/dev/mce_cmd0"
#define DEFAULT_DATA "/dev/mce_data0"
#define DEFAULT_XML "/etc/mce.cfg"


enum {
	COMMAND=CMDTREE_USERBASE,
	SPECIAL
};

enum { SPECIAL_HELP,
       SPECIAL_CLEAR,
       SPECIAL_FAKESTOP,
       SPECIAL_EMPTY,
       SPECIAL_SLEEP,
       SPECIAL_FRAME
};   

cmdtree_opt_t root_opts[] = {
	{"RB"   , COMMAND, MCE_RB, 2, 2, NULL, NULL},
	{"WB"   , COMMAND, MCE_WB, 3,-1, NULL, NULL},
	{"R"    , COMMAND, MCE_RB, 2, 2, NULL, NULL},
	{"W"    , COMMAND, MCE_WB, 3,-1, NULL, NULL},
	{"GO"   , COMMAND, MCE_GO, 2,-1, NULL, NULL},
	{"STOP" , COMMAND, MCE_ST, 2,-1, NULL, NULL},
	{"RESET", COMMAND, MCE_RS, 2,-1, NULL, NULL},
	{"HELP"    , SPECIAL, SPECIAL_HELP    , 0,0, NULL, NULL},
	{"FAKESTOP", SPECIAL, SPECIAL_FAKESTOP, 0,0, NULL, NULL},
	{"EMPTY"   , SPECIAL, SPECIAL_EMPTY   , 0,0, NULL, NULL},
	{"SLEEP"   , SPECIAL, SPECIAL_SLEEP   , 1,1, NULL, NULL},
	{"FRAME"   , SPECIAL, SPECIAL_FRAME   , 0,0, NULL, NULL},
	{"", CMDTREE_TERMINATOR, 0,0,0,NULL, NULL},
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
	char config_file[LINE_LEN];

} options = {
	0, 0, 0, "", 0, "", 0, DEFAULT_DEVICE
};


enum { ERR_MEM=1,
       ERR_OPT=2,
       ERR_MCE=3 };

int handle = -1;
int data_fd = 0;
int  command_now = 0;
int  interactive = 0;
char *line;

char errstr[LINE_LEN];


int  load_mceconfig( mce_data_t *mce, cmdtree_opt_t *opts);

int  process_command(cmdtree_opt_t *src_opts, int *args, int nargs,
		     char *errmsg);

int  process_options(int argc, char **argv);

void init_options(void);

void uppify(char *s);


int main(int argc, char **argv)
{
	FILE *ferr = stderr;
	FILE *fin  = stdin;

	int err = 0;
	//ignore args for now

	line = (char*) malloc(LINE_LEN);
	if (line==NULL) {
		fprintf(ferr, "memory error!\n");
		err = ERR_MEM;
		goto exit_now;
	}

	init_options();
	if (process_options(argc, argv)) {
		err = ERR_OPT;
		goto exit_now;
	}

	handle = mce_open(options.device_file);
	if (handle<0) {
		fprintf(ferr, "Could not open mce device '%s'\n",
			options.device_file);
		err = ERR_MCE;
		goto exit_now;
	}

	int line_count = 0;

	data_fd = open(DEFAULT_DATA, 0);
	if (data_fd <= 0) {
		fprintf(ferr, "Could not open mce data '%s'\n",	DEFAULT_DATA);
	}		

	if (mce_load_config(handle, options.config_file)!=0) {
		fprintf(ferr, "Could not load file '%s', "
			"card/para decoding will be unavailable\n",
			options.config_file);
	}

	mce_data_t *mce = NULL;

	if (mceconfig_load(options.config_file, &mce)!=0) {
		fprintf(ferr, "Could not load MCE config file '%s'.\n",
			options.config_file);
		err = ERR_MCE;
		goto exit_now;
	}
	load_mceconfig(mce, root_opts);
	
	char errmsg[1024] = "";
	char premsg[1024] = "";
	int done = 0;

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

		int args[NARGS];
		int err = 0;
		int count = cmdtree_decode(line, args, NARGS, root_opts, errmsg);
/* 		printf("count=%i\n", count); */
		if (count == 0) {
			if (options.interactive) {
				cmdtree_list(errmsg + strlen(errmsg),
					     root_opts, "MCE_CMD expects "
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

	printf("Processed %i lines, exiting.\n", line_count);

exit_now:
	if (line!=NULL) free(line);
	if (handle>=0)  mce_close(handle);

	return err;
}

void init_options()
{
	strcpy(options.config_file, DEFAULT_XML);
	strcpy(options.device_file, DEFAULT_DEVICE);
}

void uppify(char *s)
{
	if (s==NULL || *(s--)==0) return;
	while (*(++s)!=0) {
		if (*s<'a' || *s>'z') continue;
		*s += 'A' - 'a';
	}	
}

int load_mceconfig( mce_data_t *mce, cmdtree_opt_t *opts)
{
	cmdtree_opt_t *card_opts;
	card_t card;
	int i;
	int n;
	for (n=0; mceconfig_get_card(mce, &card, n) == 0; n++);

	card_opts = calloc(n+1, sizeof(*card_opts));
	if (card_opts==NULL) {
		printf("Holy, memory.\n");
		return 1;
	}
	for (i=0; mceconfig_get_card(mce, &card, i) == 0; i++) {

		cmdtree_opt_t *par_opts;
		int j,k,n=0;
		cardtype_t ct;
		paramset_t ps;
		param_t p;
		mceconfig_card_cardtype(mce, &card, &ct);
		
		for (j=0; mceconfig_cardtype_paramset(mce, &ct, j, &ps)==0; j++) {
			for (k=0; mceconfig_paramset_param(mce, &ps, k, &p)==0; k++) {
				n++;
			}
		}

		par_opts = calloc(n+1, sizeof(*par_opts));
		n = 0;
		for (j=0; mceconfig_cardtype_paramset(mce, &ct, j, &ps)==0; j++) {
			for (k=0; mceconfig_paramset_param(mce, &ps, k, &p)==0; k++) {
				strcpy(par_opts[n].name, p.name);
				uppify(par_opts[n].name);
				par_opts[n].id = p.id;
				par_opts[n].type = 12;
				par_opts[n].max_args = p.count;
				par_opts[n].sub_opts = NULL;
				par_opts[n].cargo = p.cfg;
				n++;
			}
		}
		par_opts[n].type = CMDTREE_TERMINATOR;
					
		strcpy(card_opts[i].name, card.name);
		uppify(card_opts[i].name);
		card_opts[i].id = card.id;
		card_opts[i].min_args = 1;
		card_opts[i].max_args = -1;
		card_opts[i].type = 11;
		card_opts[i].sub_opts = par_opts;
		card_opts[i].cargo = card.cfg;
	}
	card_opts[i].type = CMDTREE_TERMINATOR;

	for (i=0; opts[i].type != CMDTREE_TERMINATOR; i++) {
		if (opts[i].type == COMMAND)
			opts[i].sub_opts = card_opts;
	}

	return 0;
}


int process_command(cmdtree_opt_t *src_opts, int *arg_opt, int nargs, char *errmsg)
{
	int ret_val = 0;
	int err;
	int i;
	int arg_id[NARGS] = {0};
	param_t p;
	card_t c;
	int to_read;
	u32 bufr[100];

	err = cmdtree_translate(arg_id, arg_opt, nargs, src_opts);
	
	switch (src_opts[arg_opt[0]].type) {
	case COMMAND:

		mceconfig_cfg_card(&c, src_opts[arg_opt[0]].sub_opts[arg_opt[1]].cargo);
		mceconfig_cfg_param(&p, src_opts[arg_opt[0]].sub_opts[arg_opt[1]].sub_opts[arg_opt[2]].cargo);
		to_read = p.count;

		switch( arg_id[0] ) {
		    
		case MCE_RS:
			err = mce_reset(handle, arg_id[1], arg_id[2]);
			break;

		case MCE_GO:
			err = mce_start_application(handle,
						    arg_id[1], arg_id[2]);
			break;

		case MCE_ST:
			err = mce_stop_application(handle,
						   arg_id[1], arg_id[2]);
			break;

		case MCE_RB:
			//Check count in bounds; scale if "card" returns multiple data
			if (p.count<0 || p.count>MCE_REP_DATA_MAX) {
				sprintf(errstr, "read length out of bounds!");
				return -1;
			}
			err = mce_read_block(handle, arg_id[1], arg_id[2],
					     to_read, bufr, 1);

			if (err==0) {
				for (i=0; i<p.count; i++) {
					errmsg += sprintf(errmsg, "%#x", bufr[i]);
				}
			}
			break;

		case MCE_WB:
			//Check bounds
/* 			for (i=0; i<p.count; i++) { */
/* 				if ((props.flags & PARAM_MIN) */
/* 				    && bufw[i]<props.minimum) break; */
/* 				if ((props.flags & PARAM_MAX) */
/* 				    && bufw[i]<props.maximum) break; */
/* 			} */
/* 			if (i!=p.count) { */
/* 				sprintf(errstr,	"command expects values in range " */
/* 					"[%#x, %#x] = [%i,%i]\n", */
/* 					props.minimum, props.maximum, */
/* 					props.minimum, props.maximum); */
/* 				return -1; */
/* 			} */

			err = mce_write_block(handle,
						  arg_id[1], arg_id[2],
						  p.count, (unsigned*)arg_id+3);
			break;

		default:
			sprintf(errmsg, "command not implemented");
			return -1;
		}

		if (err==0)
			ret_val = 1;
		else {
			sprintf(errmsg, "mce library error %#08x", err);
			ret_val = -1;
		} 

		break;


	case SPECIAL:

		switch( arg_id[0] ) {

		case SPECIAL_HELP:
			cmdtree_list(errmsg, root_opts,
				     "MCE commands: [ ", " ", "]");
			break;

		case SPECIAL_CLEAR:
			ret_val = mce_reset(handle, arg_id[1], arg_id[2]);
			break;

		case SPECIAL_FAKESTOP:
			ret_val = mce_fake_stopframe(data_fd);
			break;

		case SPECIAL_EMPTY:
			ret_val = mce_empty_data(data_fd);
			break;

		case SPECIAL_SLEEP:
			//printf("SLEEP! %i\n", arg_id[1]); fflush(stdout);
			usleep(arg_id[1]);
			ret_val = 0;
			break;

		case SPECIAL_FRAME:
			ret_val = mce_set_datasize(data_fd, arg_id[1]);
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

int process_options(int argc, char **argv)
{
	char *s;
	int option;
	while ( (option = getopt(argc, argv, "?hiqpf:c:d:x")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf("Usage:\n\t%s [-i] [-q] [-p] [-d devfile] "
			       "[-c <config file> ] "
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

		case 'c':
			strcpy(options.config_file, optarg);
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


int get_int(char *card, int *card_id)
{
	char *end = card;
	if (end==NULL || *end==0) return -1;
	*card_id = strtol(card, &end, 0);
	if (*end!=0) return -1;
	return 0;
}
