/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "mce/cmdtree.h"

typedef cmdtree_token_t token_t;

int cmdtree_debug = 0;

static int get_int(char *card, long *card_id);

static const char **add_lm(const char **lm, int *lm_size, int *nlm,
        size_t *lm_len, const char *item)
{
    const char **e = lm;
    if (*nlm == *lm_size) {
        *lm_size += 10;
        e = realloc(lm, *lm_size * sizeof(char *));
        if (e == NULL)
            return lm;
    }

    size_t l = strlen(item);
    if (l > *lm_len)
        *lm_len = l;

    e[(*nlm)++] = item;
    
    return e;
}

static int lmcmp(const void *a, const void *b)
{
    return strcmp(*(const char**)a, *(const char**)b);
}

#define LINELEN 80
int cmdtree_list(char *dest, const cmdtree_opt_t *opts, int pretty,
		 const char *intro, const char *delim, const char *outro)
{
    fprintf(stderr, "pretty = %i\n", pretty);
    const char **lm = NULL;
    int lm_size = 0;
    int nlm = 0;
    size_t lm_len = 0;

	int done = 0;
	int i;
	char *t, *d = dest;
	d += sprintf(d, "%s", intro);
    if (pretty)
        d += sprintf(d, "\n\n  ");
    t = d - 2;

	for (i=0; !done; i++) {
		int type = opts[i].flags & CMDTREE_TYPE_MASK;
		
		switch(type) {
		case CMDTREE_TERMINATOR:
			done = 1;
			break;

		case CMDTREE_SELECT:
            lm = add_lm(lm, &lm_size, &nlm, &lm_len, opts[i].name);
			break;
			
		case CMDTREE_INTEGER:
            lm = add_lm(lm, &lm_size, &nlm, &lm_len, "(int)");
			break;
			
		case CMDTREE_STRING:
            lm = add_lm(lm, &lm_size, &nlm, &lm_len, "(string)");
			break;
				
		default:
            lm = add_lm(lm, &lm_size, &nlm, &lm_len, "(other)");
		}
	}

    if (pretty)
        qsort(lm, nlm, sizeof(char *), lmcmp);

    for (i = 0; i < nlm; ++i)
        if (pretty) {
            if (d - t + lm_len > LINELEN) {
                d += sprintf(d, "\n  ");
                t = d - 2;
            }
            d += sprintf(d, "%*s%s", -lm_len, lm[i], delim);
        } else
            d += sprintf(d, "%s%s", lm[i], delim);

    free(lm);

    if (pretty)
        d += sprintf(d, "\n\n");
	d += sprintf(d, "%s", outro);
	return d-dest;
}

static int list_args(char *dest, cmdtree_opt_t *opts)
{
	int index = 0;
	while (opts != NULL) {
		index += sprintf(dest+index, "%s", opts->name);
		opts = opts->sub_opts;
		if (opts != NULL) {
			index += sprintf(dest+index, ", ");
		}
	}
	return index;
}


static int nocase_cmp(const char *s1, const char *s2)
{
	while (*s1 != 0) {
		if (*s2==0) return 1;
		if (toupper(*(s1++)) != toupper(*(s2++))) return -1;
	}

	if (*s2 != 0) return -1;
	return 0;
}

static int climb( token_t *t, const cmdtree_opt_t *opt, char *errmsg,
        int suggest, int pretty)
{
	int i;
	char arg_s[1024];
	long arg_i;
	const cmdtree_opt_t *my_opt = opt;
	int arg = 0;

	t->type = CMDTREE_UNDEF;

	cmdtree_token_word(arg_s, t);
	if (cmdtree_debug) printf("select %s\n", arg_s);
	
	for (i = 0; t->type==CMDTREE_UNDEF; i++) {

		my_opt = opt+i;
		int type = my_opt->flags & CMDTREE_TYPE_MASK;
		arg = my_opt->flags & CMDTREE_ARGS;

		if (cmdtree_debug) printf(" compare %i %s\n", type, my_opt->name);
		switch (type) {

		case CMDTREE_TERMINATOR:
			t->type = CMDTREE_TERMINATOR;
			break;
			
		case CMDTREE_SELECT:
			if ( ((my_opt->flags & CMDTREE_NOCASE) &&
			      nocase_cmp(arg_s, my_opt->name)==0) ||
			     strcmp(arg_s, my_opt->name)==0) {
				t->value = (int)my_opt->user_cargo;
				t->type = CMDTREE_SELECT;
			}
			break;
			
		case CMDTREE_INTEGER:
			if (get_int(arg_s, &arg_i)==0) {
				t->value = arg_i;
				t->type = CMDTREE_INTEGER;
			} else if (arg) {
				sprintf(errmsg, "Integer argument '%s' expected.", my_opt->name);
				t->type = CMDTREE_TERMINATOR;
			}
			break;

		case CMDTREE_STRING:
			t->value = 0;
			t->type = CMDTREE_STRING;
			break;

		default:
			sprintf(errmsg, "Unknown menu option type!");
			t->type = CMDTREE_TERMINATOR;
		}
	}

	if (t->type == CMDTREE_TERMINATOR)
		return 0;

	// All selections should return the user cargo.
	t->data = my_opt->user_cargo;

	// Get additional args
	int count = 0;
	if (my_opt==NULL) {
		if (t->n > 1) {
			sprintf(errmsg, "orphaned arguments!");
			return -1;
		}
		return 0;
	}
	
	if (t->n > 1) {
		count = climb( t+1, my_opt->sub_opts, errmsg, suggest, pretty);
	} 

	if (count < 0) return count - 1;

	if (count == 0 && t->n > 1) {
		errmsg += sprintf(errmsg, "%s expects argument from ", arg_s);
		errmsg += cmdtree_list(errmsg, my_opt->sub_opts, pretty, "[ ",
				       " ", "]");
		return -2; // Biased so that missing arg position is well-represented.
	}

	// Arg counts checking...
	if (count < my_opt->min_args) {
		if (my_opt->sub_opts == NULL ||
		    (my_opt->sub_opts->flags & CMDTREE_ARGS)==0 ) {
			sprintf(errmsg, "%s expects at least %i argument%s",
				arg_s, my_opt->min_args, (my_opt->min_args == 1) ? "" : "s");
		} else {
			errmsg += sprintf(errmsg, "%s takes arguments [ ", arg_s);
			errmsg += list_args(errmsg, my_opt->sub_opts);
			errmsg += sprintf(errmsg, " ]");
		}
		return -1;
	}
	if (my_opt->max_args >=0 && count > my_opt->max_args) {
		sprintf(errmsg, "%s expects at most %i argument%s",
			arg_s, my_opt->max_args, (my_opt->max_args == 1) ? "" : "s");
		return -1;
	}

	return count + 1;
	

}


/* These aren't different. Hm. */

int cmdtree_suggest( token_t *t, const cmdtree_opt_t *opt, char *errmsg,
        int pretty)
{
	return climb(t, opt, errmsg, 1, pretty);
}

int cmdtree_select( token_t *t, const cmdtree_opt_t *opt, char *errmsg,
        int pretty)
{
	return climb(t, opt, errmsg, 0, pretty);
}


static int get_int(char *card, long *card_id) {
	char *last;
	errno = 0;
	if (card==NULL) return -1;
	*card_id = strtol(card, &last, 0);
	if (errno!=0 || *last != 0) return -1;
	return 0;
}


int cmdtree_token_word( char *word, token_t *tset )
{
	strncpy( word, tset->line + tset->start,
		 tset->stop - tset->start );
	word[tset->stop-tset->start] = 0;

	return 0;
}

int cmdtree_tokenize(token_t *t, char *line, int max_tokens) 
{
	if (t==NULL) return -1;
	if (line==NULL) return -2;
	
	int idx = 0;
	int n = 0;
	int overflow = 0;

	while (line[idx] != 0 && n < max_tokens) {
		while ( index(WHITESPACE, line[idx]) != NULL &&
			line[idx] != 0) idx++;

		if (line[idx] == 0) break;
		
		t[n].start = idx;
		
		while ( index(WHITESPACE, line[idx]) == NULL && 
			line[idx] != 0 ) idx++;
		
		t[n++].stop = idx;
	}
	if (line[idx] != 0) overflow = 1;

	for (idx=0; idx<n; idx++ ) {
		t[idx].n = n-idx;
		t[idx].line = line;
	}

	if (cmdtree_debug) {
		printf("%i %s\n", t->n, t->line);
		char word[128];
		int i;
		for (i=0; i<t->n; i++) {
			cmdtree_token_word(word, t+i);
			printf("%i %i %s\n", t->start, t->stop, word);
		}
	}

	if (overflow) return -2;

	return 0;		
}

int match_menu( cmdtree_token_t *t, const cmdtree_opt_t *menu )
{
	int index;
	char arg[1024];
	long argi;

	if (t== NULL || t->line == NULL)
		return -1;

	cmdtree_token_word(arg, t);

	for (index = 0; !(menu[index].flags & CMDTREE_TERMINATOR); index++) {

		const cmdtree_opt_t* opt = menu+index;
		
		switch (opt->flags & CMDTREE_TYPE_MASK) {

		case CMDTREE_SELECT:
			if ( ((opt->flags & CMDTREE_NOCASE) &&
			      nocase_cmp(arg, opt->name)==0) ||
			     strcmp(arg, opt->name)==0) {
				t->value = (int)opt->user_cargo;
				t->data = opt->user_cargo;
				t->type = CMDTREE_SELECT;
				return index;
			}
			break;

		case CMDTREE_INTEGER:
			if (get_int(arg, &argi)==0) {
				t->value = argi;
				t->data = opt->user_cargo;
				t->type = CMDTREE_INTEGER;
				return index;
			}
		}
	}
	return 0;
}
