#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cmdtree.h"

static int get_int(char *card, int *card_id);

static int string_index(char *word, cmdtree_opt_t *list);

int cmdtree_translate(int *dest, const int *src, int nargs,
		      cmdtree_opt_t *src_opts)
{
	int i;
	for (i=0; i<nargs && src_opts != NULL; i++) {
/* 		printf("tx-s %i\n", src[i]); fflush(stdout); */
		dest[i] = src_opts[src[i]].id;
		src_opts = src_opts[src[i]].sub_opts;
	}
	for ( ; i<nargs; i++) {
/* 		printf("tx-# %i\n", src[i]); fflush(stdout); */
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

/* 	printf(" decode %s\n", word); */
	
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

		int final_word = cmdtree_decode(NULL, args, 0, NULL, errmsg);
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
/* 		printf("CMP %s %s\n", word, list[idx].name); */
		if (strcmp(word, list[idx].name)==0) return idx;
	}

	return idx;
}


int get_word( char *word, token_set_t *tset, int index )
{
	if (index <0 || index >= tset->n) {
		*word = 0;
		return 0;
	}
	
	strncpy( word, tset->line + tset->start[index],
		 tset->stop[index] - tset->start[index] );
	word[tset->stop[index]-tset->start[index]] = 0;

	return 0;
}

/* int select(token_set_t *t, cmdtree_opt_t *opt) */
/* { */
/* 	int i; */

/* 	for (i=0; opt[i]. */


/* } */


int tokenize(token_set_t *tset, char *line) 
{
	if (tset==NULL) return -1;
	if (line==NULL) return -2;
	
	int idx = 0;
	tset->n = 0;
	tset->line = line;

	while (line[idx] != 0) {
		while ( index(WHITESPACE, line[idx]) != NULL &&
			line[idx] != 0) idx++;

		if (line[idx] == 0) break;
		
		tset->start[tset->n] = idx;
		
		while ( index(WHITESPACE, line[idx]) == NULL && 
			line[idx] != 0 ) idx++;
		
		tset->stop[tset->n++] = idx;
	}

/* 	printf("%i %s\n", tset->n, tset->line); */
/* 	char word[128]; */
/* 	int i; */
/* 	for (i=0; i<tset->n; i++) { */
/* /\* 		strncpy(word, line+tset->start[i], *\/ */
/* /\* 			tset->stop[i]-tset->start[i]); *\/ */
/* /\* 		word[tset->stop[i]-tset->start[i]] = 0; *\/ */
/* 		get_word(word, tset, i); */
/* 		printf("%i %i %s\n", tset->start[i], tset->stop[i], word); */
/* 	} */

	return 0;		
	
}
