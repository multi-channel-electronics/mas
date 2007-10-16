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

#define CASE_INSENSITIVE

#define DEFAULT_DEVICE "/dev/mce_cmd0"
#define DEFAULT_DATA "/dev/mce_data0"
#define DEFAULT_XML "/etc/mce.cfg"


enum {
	ENUM_COMMAND_LOW,	
	COMMAND_RB,
	COMMAND_WB,
	COMMAND_R,
	COMMAND_W,
	COMMAND_GO,
	COMMAND_ST,
	COMMAND_RS,
	ENUM_COMMAND_HIGH,
	ENUM_SPECIAL_LOW,
	SPECIAL_HELP,
	SPECIAL_CLEAR,
	SPECIAL_FAKESTOP,
	SPECIAL_EMPTY,
	SPECIAL_SLEEP,
	SPECIAL_FRAME,
	SPECIAL_DEC,
	SPECIAL_HEX,
	ENUM_SPECIAL_HIGH,
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

cmdtree_opt_t command_placeholder_opts[] = {
	{ CMDTREE_INTEGER   , "", 0, -1, 0, integer_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t root_opts[] = {
	{ CMDTREE_SELECT, "RB"      , 2, 3, COMMAND_RB, command_placeholder_opts},
	{ CMDTREE_SELECT, "WB"      , 3,-1, COMMAND_WB, command_placeholder_opts},
	{ CMDTREE_SELECT, "R"       , 2, 3, COMMAND_RB, command_placeholder_opts},
	{ CMDTREE_SELECT, "W"       , 3,-1, COMMAND_WB, command_placeholder_opts},
	{ CMDTREE_SELECT, "GO"      , 2,-1, COMMAND_GO, command_placeholder_opts},
	{ CMDTREE_SELECT, "STOP"    , 2,-1, COMMAND_ST, command_placeholder_opts},
	{ CMDTREE_SELECT, "RESET"   , 2,-1, COMMAND_RS, command_placeholder_opts},
	{ CMDTREE_SELECT, "HELP"    , 0, 0, SPECIAL_HELP    , NULL},
	{ CMDTREE_SELECT, "FAKESTOP", 0, 0, SPECIAL_FAKESTOP, NULL},
	{ CMDTREE_SELECT, "EMPTY"   , 0, 0, SPECIAL_EMPTY   , NULL},
	{ CMDTREE_SELECT, "SLEEP"   , 1, 1, SPECIAL_SLEEP   , integer_opts},
	{ CMDTREE_SELECT, "FRAME"   , 1, 1, SPECIAL_FRAME   , integer_opts},
	{ CMDTREE_SELECT, "DEC"     , 0, 0, SPECIAL_DEC     , NULL},
	{ CMDTREE_SELECT, "HEX"     , 0, 0, SPECIAL_HEX     , NULL},
	{ CMDTREE_TERMINATOR, "", 0,0,0, NULL},
};
	
struct {
	int interactive;
	int nonzero_only;
	int no_prefix;
	int display;

	char batch_file[LINE_LEN];
	int  batch_now;

	char cmd_command[LINE_LEN];
	int  cmd_now;

	char device_file[LINE_LEN];
	char config_file[LINE_LEN];

} options = {
	0, 0, 0, SPECIAL_HEX, "", 0, "", 0, DEFAULT_DEVICE
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

int process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg);

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
						     "mce_cmd expects argument from [ ", " ", "]");
					err = -1;
				}					
			} else {
 				err = process_command(root_opts, args, errmsg);
				if (err==0) err = 1;
			}
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

	card_opts = calloc(n+2, sizeof(*card_opts));
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
		
		int string_count = strlen(card.name) + 1;

		for (j=0; mceconfig_cardtype_paramset(mce, &ct, j, &ps)==0; j++) {
			for (k=0; mceconfig_paramset_param(mce, &ps, k, &p)==0; k++) {
				string_count += strlen(p.name) + 1;
				n++;
			}
		}

		char *string_table = malloc(string_count);
		par_opts = calloc(n+2, sizeof(*par_opts));

		n = 0;
		for (j=0; mceconfig_cardtype_paramset(mce, &ct, j, &ps)==0; j++) {
			for (k=0; mceconfig_paramset_param(mce, &ps, k, &p)==0; k++) {
				strcpy(string_table, p.name);
				uppify(string_table);
				par_opts[n].name = string_table;
				string_table += strlen(string_table) + 1;
				par_opts[n].type = CMDTREE_SELECT;
				par_opts[n].min_args = 0;
				par_opts[n].max_args = p.count;
				par_opts[n].sub_opts = integer_opts;
				par_opts[n].user_cargo = (unsigned long)p.cfg;
				n++;
			}
		}

		memcpy(&(par_opts[n]), integer_opts, sizeof(integer_opts));
					
		card_opts[i].name = string_table;
 		strcpy(string_table, card.name);
		uppify(string_table);
		card_opts[i].min_args = 1;
		card_opts[i].max_args = -1;
		card_opts[i].type = CMDTREE_SELECT;
		card_opts[i].sub_opts = par_opts;
		card_opts[i].user_cargo = (unsigned long)card.cfg;
	}

	memcpy(&(card_opts[i]), integer_opts, sizeof(integer_opts));

	for (i=0; opts[i].type != CMDTREE_TERMINATOR; i++) {
		if (opts[i].sub_opts == command_placeholder_opts)
			opts[i].sub_opts = card_opts;
	}

	return 0;
}


int process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg)
{
	int ret_val = 0;
	int err;
	int i;
	param_t p;
	card_t c;
	int to_read, to_write;
	u32 buf[NARGS];

	int is_command = (tokens[0].value >= ENUM_COMMAND_LOW && 
			  tokens[0].value < ENUM_COMMAND_HIGH);
	
	if (is_command) {

		// Token[0] is the command (RB, WB, etc.)
		// Token[1] is the card
		// Token[2] is the param
		// Token[3+] are the data

		// Allow integer values for card and para.

		to_read = 0;
		to_write = NARGS;
		int card_id = tokens[1].value;
		int para_id = tokens[2].value;

		if ( tokens[1].type == CMDTREE_SELECT ) {
			mceconfig_cfg_card (&c, (config_setting_t*)tokens[1].value);
			card_id = c.id;
			
			if (tokens[2].type == CMDTREE_SELECT ) {
				mceconfig_cfg_param(&p, (config_setting_t*)tokens[2].value);
				para_id = p.id;
				to_read = p.count;
				to_write = p.count;
			}
		}
	
		if (to_read == 0 && tokens[3].type == CMDTREE_INTEGER)
			to_read = tokens[3].value;

		switch( tokens[0].value ) {
		
		case COMMAND_RS:
			err = mce_reset(handle, card_id, para_id);
			break;

		case COMMAND_GO:
			err = mce_start_application(handle,
						    card_id, para_id);
			break;

		case COMMAND_ST:
			err = mce_stop_application(handle,
						   card_id, para_id);
			break;

		case COMMAND_RB:
			//Check count in bounds; scale if "card" returns multiple data
			if (to_read<0 || to_read>MCE_REP_DATA_MAX) {
				sprintf(errstr, "read length out of bounds!");
				return -1;
			}
			err = mce_read_block(handle, card_id, para_id,
					     to_read, buf, 1);

			if (err==0) {
				for (i=0; i<to_read; i++) {
					if (options.display == SPECIAL_HEX )
						errmsg += sprintf(errmsg, "%#x ", buf[i]);
					else 
						errmsg += sprintf(errmsg, "%i ", buf[i]);
				}
			}
			break;

		case COMMAND_WB:
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

/* 			if (tokens[3].n > to_write) { */
/* 				sprintf(errmsg, "too many arguments  */
			to_write = tokens[3].n;
			for (i=0; i<to_write; i++) {
				buf[i] = tokens[3+i].value;
			}
			
			err = mce_write_block(handle,
					      card_id, para_id, p.count, buf);
			break;
			
		default:
			sprintf(errmsg, "command not implemented");
			return -1;
		}
		
		if (err!=0) {
			sprintf(errmsg, "mce library error %#08x", err);
			ret_val = -1;
		} 
	} else {

		switch(tokens[0].value) {

		case SPECIAL_HELP:
/* 			cmdtree_list(errmsg, root_opts, */
/* 				     "MCE commands: [ ", " ", "]"); */
			break;

		case SPECIAL_CLEAR:
			ret_val = mce_reset(handle, tokens[1].value, tokens[2].value);
			break;

		case SPECIAL_FAKESTOP:
			ret_val = mce_fake_stopframe(data_fd);
			break;

		case SPECIAL_EMPTY:
			ret_val = mce_empty_data(data_fd);
			break;

		case SPECIAL_SLEEP:
			usleep(tokens[1].value);
			break;

		case SPECIAL_FRAME:
			ret_val = mce_set_datasize(data_fd, tokens[1].value);
			if (ret_val != 0) {
				sprintf(errmsg, "mce_library error %i", ret_val);
			}
			break;

		case SPECIAL_DEC:
			options.display = SPECIAL_DEC;
			break;

		case SPECIAL_HEX:
			options.display = SPECIAL_HEX;
			break;

		default:
			sprintf(errmsg, "command not implemented");
			ret_val = -1;
		}
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
