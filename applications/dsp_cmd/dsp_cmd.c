/*

  User interfaces are only fun to write when the user is also a machine

*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <mcedsp.h>

#define CASE_INSENSITIVE

#define LINE_LEN 1024

struct cmd_str {
	char name[32];
	dsp_command_code code;
	int  min_args;
	int  mem_arg1;
};

#define NCOM 6

struct cmd_str commands[NCOM] = {
	{"READ",      DSP_RDM, 2, 1},
	{"WRITE",     DSP_WRM, 3, 1},
	{"START",     DSP_GOA, 0, 0},
	{"STOP",      DSP_STP, 0, 0},
	{"RESET",     DSP_RST, 0, 0},
	{"RESET_MCE", DSP_RCO, 0, 0},
};

struct spec_str {
	char name[32];
	int  id;
};

#define SPECIAL_CLEAR          0

#define NSPEC 1

struct spec_str specials[NSPEC] = {
	{"CLEAR_FLAGS", SPECIAL_CLEAR},
};


#define DEFAULT_DEVICE "/dev/mce_dsp0"


int handle;
char errstr[LINE_LEN];

void uppify(char *s);

int process_command(int key);
int process_special(int key);

int command_key(char *name);
int special_key(char *name);

int main(int argc, char **argv) {
	int key;
	int err = 0;
	//ignore args for now

	int line_count = 0;
	FILE *fin = stdin;
	char *line = (char*) malloc(LINE_LEN);

	handle = dsp_open(DEFAULT_DEVICE);
	if (handle<0) {
		printf("Could not open device %s\n", DEFAULT_DEVICE);
		exit(1);
	}

	while (1) {
		int n = LINE_LEN;
		getline(&line, &n, fin);

		if (n==0 || feof(fin)) break;

#ifdef CASE_INSENSITIVE
		uppify(line);
#endif
		n = strlen(line);
		if (line[n-1]=='\n') line[--n]=0;
		line_count++;
		
		char name[LINE_LEN];
		char *tmp = strtok(line, " ");
		if (tmp!=NULL) strcpy(name, tmp);
		
		if ( (key=command_key(name)) >=0 ) {
			if (process_command(key)==0) {
				printf("Command %s ok: %s\n",
				       name, errstr);
			} else {
				printf("Command %s failed!: %s\n",
				       name, errstr);
			}
		} else if ( (key=special_key(name)) >=0 ) {
			if (process_special(key)==0) {
				printf("Special command %s ok: %s\n",
				       name, errstr);
			} else {
				printf("Special command %s failed!: %s\n",
				       name, errstr);
			}
		} else {
			printf("Unknown command '%s'\n", name);
			err = 1;
			break;
		}
	}

	printf("Processed %i lines\n", line_count);

	dsp_close(handle);

	return err;
}


void uppify(char *s) {
	if (s--==NULL) return;
	while (*(++s)!=0) {
		if (*s<'a' || *s>'z') continue;
		*s += 'A' - 'a';
	}	
}

int command_key(char *name) {
	int i;
	for (i=0; i<NCOM; i++)
		if ( strcmp(commands[i].name, name)==0) return i;
	return -1;
}

int special_key(char *name) {
	int i;
	for (i=0; i<NSPEC; i++)
		if ( strcmp(specials[i].name, name)==0) return i;
	return -1;
}

int get_int(char *card, int *card_id) {
	errno = 0;
	if (card==NULL) return -1;
	*card_id = strtol(card, NULL, 0);
	if (errno!=0) return -1;
}

		

/* Command word ("read") has already been toked off.  Allowed formats:
 * 
 *     read y 0x2
 *     write x 453 786
 *
 *     reads y 0x2 3
 *     write y 453 786 23 19
 */

#define ARGS 3
int process_command(int key)
{
	
	int argi[ARGS] = {0,0,0};
	char *token;
	int narg = 0;

	*errstr = 0;

	token = strtok(NULL, " ");

	//Special handling for arg 0?
	if (commands[key].mem_arg1 ) {
		argi[narg] = dsp_atomem(token);
		if (argi[narg]<0) {
			sprintf(errstr,
				"invalid memory type expression '%s'",
				token);
			return -1;
		}
		narg++;
		token = strtok(NULL, " ");
	}
	
	while (narg<ARGS) {
		if (get_int(token, argi+narg) != 0) {
			if (narg < commands[key].min_args) {
				strcpy(errstr, "not enough arguments");
				return -1;
			}
			break;
		}
		token = strtok(NULL, " ");
		narg++;
	}

	int ret_val = 0;

	switch(commands[key].code) {

	case DSP_RDM:
		ret_val = dsp_read_word(handle, argi[0], argi[1]);
		if (ret_val < 0) break;
		sprintf(errstr, "%s[%#x] = %#x",
			dsp_memtoa(argi[0]), argi[1], ret_val);
		break;
    
	case DSP_WRM:
		ret_val = dsp_write_word(handle, argi[0], argi[1], argi[2]);
		break;

	case DSP_GOA:
		ret_val = dsp_start_application(handle, argi[0]);
		break;
    
	case DSP_STP:
		ret_val = dsp_stop_application(handle);
		break;

	case DSP_RST:
		ret_val = dsp_reset(handle);
		break;

	case DSP_RCO:
		ret_val = dsp_reset_mce(handle);
		break;
    
	default:
		sprintf(errstr, "command not implemented");
		return -1;
	}


	if (ret_val < 0) {
		sprintf(errstr, "command returned error %i", ret_val);
		return -1;
	}

	return 0;
}


int process_special(int key)
{
	char *token;
	*errstr = 0;

	int ret_val = 0;

	switch(specials[key].id) {

	case SPECIAL_CLEAR:
		ret_val = dsp_reset_flags(handle);
		if (ret_val < 0) break;
		break;
    
	default:
		sprintf(errstr, "command not implemented");
		return -1;
	}


	if (ret_val < 0) {
		sprintf(errstr, "command returned error %#x", ret_val);
		return -1;
	}

	return 0;
}

