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
#include <string.h>
#include <unistd.h>

#include <mcecmd.h>

#define LINE_LEN 1024

#define WHITE " \t"

#define ERR_CMD  1
#define ERR_OPT  2
#define ERR_MCE  3
#define ERR_MEM  4

#define FMT_HEX 0
#define FMT_DEC 1

struct cmd_str {
	char name[32];
	mce_command_code code;
	int  carded;
	int  paraed;
	int  format;
};

// fine NCOM 7

struct cmd_str commands[] = {
	{"rb",     MCE_RB, 0, 0, FMT_HEX},
	{"rb_hex", MCE_RB, 0, 0, FMT_HEX},
	{"rb_dec", MCE_RB, 0, 0, FMT_DEC},
	{"wb",     MCE_WB, 0, 0, FMT_HEX},
	{"r",      MCE_RB, 0, 0, FMT_HEX},
	{"r_hex",  MCE_RB, 0, 0, FMT_HEX},
	{"r_dec",  MCE_RB, 0, 0, FMT_DEC},
	{"w",      MCE_WB, 0, 0, FMT_HEX},
	{"go",     MCE_GO, 0, 0, FMT_HEX},
	{"stop",   MCE_ST, 0, 0, FMT_HEX},
	{"reset",  MCE_RS, 0, 0, FMT_HEX},
	{"",       0     , 0, 0, FMT_HEX},
};

struct spec_str {
	char name[32];
	int  id;
};

enum { SPECIAL_HELP,
       SPECIAL_CLEAR,
       SPECIAL_FAKESTOP,
       SPECIAL_EMPTY,
       SPECIAL_SLEEP,
       SPECIAL_FRAME
};   

struct spec_str specials[] = {
	{"help", SPECIAL_HELP},
	{"clear_flags", SPECIAL_CLEAR},
	{"frame_size", SPECIAL_FRAME},
	{"fake_stop", SPECIAL_FAKESTOP},
	{"empty", SPECIAL_EMPTY},
	{"sleep", SPECIAL_SLEEP},
	{"", -1}
};


#define DEFAULT_DEVICE "/dev/mce_cmd0"
#define DEFAULT_DATA "/dev/mce_data0"
#define DEFAULT_XML "/etc/mce.cfg"

struct {
	char device_name[LINE_LEN];
	char xml_name[LINE_LEN];
} options;


int handle = -1;
int data_fd = 0;
int  command_now = 0;
int  interactive = 0;
char *line;

char errstr[LINE_LEN];

int process_special(int key);
int process_command(int key);

int command_key(char *name);
int special_key(char *name);

int process_options(int argc, char **argv);
void init_options(void);

char *lowify(char *src);

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

	handle = mce_open(options.device_name);
	if (handle<0) {
		fprintf(ferr, "Could not open mce device '%s'\n",
			options.device_name);
		err = ERR_MCE;
		goto exit_now;
	}

	int line_count = 0;

	data_fd = open(DEFAULT_DATA, 0);
	if (data_fd <= 0) {
		fprintf(ferr, "Could not open mce data '%s'\n",	DEFAULT_DATA);
	}		

	if (mce_load_config(handle, options.xml_name)!=0) {
		fprintf(ferr, "Could not load file '%s', "
			"card/para decoding will be unavailable\n",
			options.xml_name);
	}
/* 	if (mce_load_xml(handle, options.xml_name)!=0) { */
/* 		fprintf(ferr, "Could not load file '%s', " */
/* 			"card/para decoding will be unavailable\n", */
/* 			options.xml_name); */
/* 	} */

	int done = 0;

	while (!done) {

		if (command_now) {
			// This only occurs if command was passed with -x
			done = 1;
		} else {
			int n = LINE_LEN;
			getline(&line, &n, fin);
			line_count++;

			if (n==0 || feof(fin)) {
				done = 1;
			} else {
				n = strlen(line);
				if (line[n-1]=='\n') line[--n] = 0;
				command_now = 1;
			}
		}

		if (command_now) {
			command_now = 0;
			char *cmd = strtok(line, WHITE);
			if (cmd==NULL || *cmd=='#')
				continue;
			
			int key;
			if ( (key = command_key(cmd)) >= 0 ) {
				err = process_command(key);
			} else if ( (key = special_key(cmd)) >= 0) {
				err = process_special(key);
			} else {
				sprintf(errstr, "unknown command");
				err = ERR_CMD;
			}

			if (err==0) {
				printf("Line %i: %s ok: %s\n",
				       line_count, cmd, errstr);
			} else {
				fprintf(ferr, "Line %i: %s failed!: %s\n",
					line_count, cmd, errstr);
				err = ERR_CMD;
				done = !interactive;
			}
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
	strcpy(options.xml_name, DEFAULT_XML);
	strcpy(options.device_name, DEFAULT_DEVICE);
}

int setup_command(int argc, char **argv, int optind)
{
	if (line==NULL) {
		fprintf(stderr, "Memory, tsk tsk.\n");
		return -1;
	}

	char *s = line;
	while (optind < argc) {
		s += sprintf(s, "%s ", argv[optind++]);
	}
	command_now = 1;
	
	return 0;
}

int process_options(int argc, char **argv) {

	int option;
	while ( (option = getopt(argc, argv, "x:f:d:ih")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf("Usage:\n\t%s [-i] [-d devfile] "
			       "[-f <config file>] [-x <command>]\n",
			       argv[0]);
			return -1;

		case 'x':
			setup_command(argc, argv, optind-1);
			break;

		case 'f':
			strcpy(options.xml_name, optarg);
			break;

		case 'd':
			strcpy(options.device_name, optarg);
			break;

		case 'i':
			interactive = 1;
			break;

		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}

	return 0;
}


int command_key(char *name) {
	int i;
	for (i=0; commands[i].name[0]!=0; i++)
		if ( strcmp(commands[i].name, name)==0) return i;
	return -1;
}

int special_key(char *name) {
	int i;
	for (i=0; specials[i].id >= 0; i++)
		if ( strcmp(specials[i].name, name)==0) 
			return specials[i].id;
	return -1;
}


int get_int(char *card, int *card_id) {
	char *end = card;
	if (end==NULL || *end==0) return -1;
	*card_id = strtol(card, &end, 0);
	if (*end!=0) return -1;
}

/* Command word ("w") has already been toked off.  Allowed formats:
 * 
 *     w 0x2 0x99 7 ...
 *     r 0x2 0x96 [1]
 * 
 */
//Return 0 on success

int process_command(int key)
{	
	int i;

	//All commands have card, then param.
	char *card;
	char *para;

	int para_type = -1;
	     
	card = strtok(NULL, WHITE);
	if (card==NULL) {
		sprintf(errstr, "expected card string or number");
		return -1;
	}
	para = strtok(NULL, WHITE);
	if (para==NULL) {
		sprintf(errstr, "expected param string or number");
		return -1;
	}
	
       
	//Attempt to convert card and para to integers, otherwise lookup
	param_properties_t props;
	mce_param_init(&props);

	int raw_mode = ( get_int(card, &props.card_id)==0 &&
			 get_int(para, &props.para_id)==0 );

	if (!raw_mode) {
		//Extract trailing numerals from para as para_index
		int index = 0;
		char *s;
		for ( s=para; *s!=0 && ( *s<'0' || *s>'9'); s++);
		sscanf(s, "%i", &index);
		*s = 0;

		if (mce_lookup(handle, &props,
			       card, para, index) !=0) {
			sprintf(errstr, "Could not locate card:param:index "
				"= %s:%s:%i", card, para, index);
			return -1;
		}
	}


        // Rules for arguments: In raw mode, for all commands except
        // read_block, 0 to MCE_CMD_DATA_MAX arguments may be passed;
        // for read_block only a single argument is accepted and it is
        // the size of the read.

        // In non-raw mode:
        // rb: up to props.count args are accepted, but ignored.
        // wb: exactly props.count args must be given
        // other commands: up to props.count args accepted, but ignored

	int code = commands[key].code;
	char *name = commands[key].name;

	int exact_mode =
		(!raw_mode && (code==MCE_WB)) ||
		( raw_mode && (code==MCE_RB));
	
	if (raw_mode) {
		props.count = (code==MCE_RB) ?
			1 : MCE_CMD_DATA_MAX;
	}

	int data_count = 0;
	int bufr[MCE_REP_DATA_MAX];
	int bufw[MCE_CMD_DATA_MAX];
	char *token;
	while ( (token=strtok(NULL, WHITE)) !=NULL ) {
		if (data_count>=props.count) {
			sprintf(errstr, "too many arguments!");
			return -1;
		}
		if ( get_int(token, bufw+data_count++) !=0) {
			sprintf(errstr, "integer arguments expected!");
			return -1;
		}
	}
	if (exact_mode && data_count<props.count) {
		sprintf(errstr, "%i arguments expected.", props.count);
		return -1;
	}

	int to_read = raw_mode ? bufw[0] : props.count;
	int read_count = to_read * props.card_count;

	// That will likely do it.
	int ret_val = 0;
	errstr[0] = 0;

	switch(code) {

	case MCE_RS:
		ret_val = mce_reset(handle, props.card_id, props.para_id);
		break;

	case MCE_GO:
		ret_val = mce_start_application(handle,
						props.card_id, props.para_id);
		break;

	case MCE_ST:
		ret_val = mce_stop_application(handle,
					       props.card_id, props.para_id);
		break;

	case MCE_RB:
		//Check count in bounds; scale if "card" returns multiple data

		if (read_count<0 || read_count>MCE_REP_DATA_MAX) {
			sprintf(errstr, "read length out of bounds!");
			return -1;
		}
		ret_val = mce_read_block(handle,
					 props.card_id, props.para_id,
					 to_read, bufr, props.card_count);

		if (ret_val==0) {
			char* next = errstr;			
			for (i=0; i<read_count; i++) {
				next += sprintf(next,
						(commands[key].format==FMT_HEX
						 ? " %#x" : " %10i"),
						bufr[i]);
			}
		}
		break;

	case MCE_WB:
		//Check bounds
		for (i=0; i<data_count; i++) {
			if ((props.flags & PARAM_MIN)
			    && bufw[i]<props.minimum) break;
			if ((props.flags & PARAM_MAX)
			    && bufw[i]<props.maximum) break;
		}
		if (i!=data_count) {
			sprintf(errstr,	"command expects values in range "
					"[%#x, %#x] = [%i,%i]\n",
				props.minimum, props.maximum,
				props.minimum, props.maximum);
			return -1;
		}

		ret_val = mce_write_block(handle,
					  props.card_id, props.para_id,
					  data_count, bufw);
		break;

	default:
		sprintf(errstr, "command '%s' not implemented\n",
			name);
		return -1;
	}

	if (ret_val != 0) {
		sprintf(errstr, mce_error_string (ret_val));
	}

	return ret_val;
}


int process_special(int key) {

	int ret_val = 0;
	int a;
	char *nextword = strtok(NULL, WHITE);
	char *s;

	switch(key) {

	case SPECIAL_HELP:
		s = errstr + sprintf(errstr, "Special commands:\n");
		for (a=0; specials[a].name[0]!=0; a++)
			s += sprintf(s, "%s\n", specials[a].name);
		ret_val = 0;
		break;

/* 	case SPECIAL_CLEAR: */
/* 		ret_val = mce_reset(handle, card_id, para_id); */
/* 		break; */

	case SPECIAL_FAKESTOP:
		ret_val = mce_fake_stopframe(data_fd);
		break;

	case SPECIAL_EMPTY:
		ret_val = mce_empty_data(data_fd);
		break;

	case SPECIAL_SLEEP:
		if (get_int(nextword, &a)==0) {
			usleep(a);
		} else
			sleep(1);
		ret_val = 0;
		break;

	case SPECIAL_FRAME:
		if (get_int(nextword, &a)==0) {
			ret_val = mce_set_datasize(data_fd, a);
		} else {
			sprintf(errstr, "'%s' expects integer argument",
				specials[key].name);
			ret_val = -1;
		}
		break;

	default:
		sprintf(errstr, "special command '%s' not implemented\n",
			specials[key].name);
		return -1;
	}

	return ret_val;
}

char *lowify(char *src)
{
	char *s = src;
	while (*s != 0)
		*(s++) = toupper(*s);
	return src;
}
