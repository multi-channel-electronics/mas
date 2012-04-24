/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _MCE_CMDTREE_H_
#define _MCE_CMDTREE_H_

extern int mascmdtree_debug;

struct mascmdtree_opt_struct;

typedef struct mascmdtree_opt_struct mascmdtree_opt_t;

struct mascmdtree_opt_struct {
	int flags;
	const char *name;//[32];
	int min_args;
	int max_args;
	unsigned long user_cargo;
    mascmdtree_opt_t *sub_opts;
};

#define MASCMDTREE_TYPE_MASK  0x000000ff

#define MASCMDTREE_UNDEF      0
#define MASCMDTREE_TERMINATOR 1
#define MASCMDTREE_SELECT     2
#define MASCMDTREE_INTEGER    3
#define MASCMDTREE_STRING     4
#define MASCMDTREE_USERSTART  5

#define MASCMDTREE_NOCASE     0x00000100
#define MASCMDTREE_ARGS       0x00000200

#define MASCMDTREE_MAX_TOKENS 64
#define MASCMDTREE_WHITESPACE " \t\n\r"



typedef enum {
    MASCMDTREE_MIN = 1 <<  0,
    MASCMDTREE_MAX = 1 <<  1,
    MASCMDTREE_OPT = 1 <<  2,
    MASCMDTREE_LEN = 1 <<  3,
} mascmdtree_flag_t;

typedef struct {
	int start, stop, type, value;
	unsigned long data;
    mascmdtree_flag_t viol;
	int n;
	char *line;
} mascmdtree_token_t;

int mascmdtree_suggest(mascmdtree_token_t *t, const mascmdtree_opt_t *opt,
        char *errmsg);
int mascmdtree_select(mascmdtree_token_t *t, const mascmdtree_opt_t *opt,
        char *errmsg);
int mascmdtree_token_word(char *word, mascmdtree_token_t *tset);
int mascmdtree_tokenize(mascmdtree_token_t *tset, char *line, int max_tokens);
int mascmdtree_list(char *dest, const mascmdtree_opt_t *opts,
        const char *intro, const char *delim, const char *outro);


#endif
