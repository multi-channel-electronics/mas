#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cmdtree.h"

static int get_int(char *card, int *card_id);

static int string_index(char *word, cmdtree_opt_t *list);

int cmdtree_translate(int *dest, const int *src, int nargs, cmdtree_opt_t *src_opts)
{
	int i;
	for (i=0; i<nargs && src_opts != NULL; i++) {
		dest[i] = src_opts[src[i]].id;
		src_opts = src_opts[i].sub_opts;
	}
	for ( ; i<nargs; i++) {
		dest[i] = src[i];
	}
	return 0;
}

int cmdtree_list(char *dest, cmdtree_opt_t *opts,
		 const char *intro, const char *delim, const char *outro)
{
	int i;
	dest += sprintf(dest, "%s", intro);
	for (i=0; opts[i].type != CMDTREE_TERMINATOR; i++) {
		dest += sprintf(dest, "%s%s", opts[i].name, delim);
	}
	dest += sprintf(dest, "%s", outro);
	return i;		
}


int cmdtree_decode(char *line, int *args, int max_args, cmdtree_opt_t *opts, char *errmsg)
{
	char *word = strtok(line, " ");
	if (word == NULL) return 0;
	
	if (max_args <= 0) {
		sprintf(errmsg, "I can't parse that many arguments");
		return -1;
	}

	if (opts == NULL) {
		// Fill last args with ints from command line
		int arg_idx = 0;
		while ( word != NULL && (arg_idx < max_args) &&
			(get_int(word, args+arg_idx) == 0) ) {
			arg_idx ++;
			word = strtok(NULL, " ");
		}

		int final_word = cmdtree_decode(word, args, 0, NULL, errmsg);
		return (final_word >= 0 ? arg_idx + final_word : final_word);
	}

	int idx = string_index(word, opts);

	// Pass un-found parameters up to previous level
	if (opts[idx].type == CMDTREE_TERMINATOR)
		return 0;

	*args = idx;
	int sub_args = cmdtree_decode(NULL, args+1, max_args-1,
				      opts[idx].sub_opts, errmsg);
	if (sub_args < 0) return sub_args;
	if (sub_args < opts[idx].min_args) {
		if (sub_args==0 && opts[idx].sub_opts != NULL) {
			sprintf(errmsg, "%s expects argument from ",
				opts[idx].name);
			cmdtree_list(errmsg + strlen(errmsg),
				     opts[idx].sub_opts,
				     "[ ", " ", "]");
		} else {
			sprintf(errmsg, "%s expects at least %i arguments",
				opts[idx].name, opts[idx].min_args);
		}
		return -1;
	} else if ( opts[idx].max_args >= 0 && sub_args > opts[idx].max_args) {
		sprintf(errmsg, "%s expects at most %i arguments",
			opts[idx].name, opts[idx].max_args);
		return -1;
	}
	return sub_args + 1;
}


int get_int(char *card, int *card_id) {
	errno = 0;
	if (card==NULL) return -1;
	*card_id = strtol(card, NULL, 0);
	if (errno!=0) return -1;
	return 0;
}

		
int string_index(char *word, cmdtree_opt_t *list)
{
	int idx;
	for (idx = 0; list[idx].type != CMDTREE_TERMINATOR; idx++) {
		if (strcmp(word, list[idx].name)==0) return idx;
	}

	return idx;
}

