#ifndef _CMDTREE_H_
#define _CMDTREE_H_

extern int cmdtree_debug;

struct cmdtree_opt_struct;

typedef struct cmdtree_opt_struct cmdtree_opt_t;

struct cmdtree_opt_struct {
	int type;
	const char *name;//[32];
	int min_args;
	int max_args;
	unsigned long user_cargo;
	cmdtree_opt_t *sub_opts;
};

enum {
	CMDTREE_TERMINATOR = 0,
	CMDTREE_UNDEF,
	CMDTREE_SELECT,
	CMDTREE_INTEGER,
	CMDTREE_STRING,

/* Insert additional enums between here and "CMDTREE_USERBASE" */

	CMDTREE_USERBASE
};


#define MAX_TOKENS 64
#define WHITESPACE " \t\n\r"



typedef enum {
	CMDTREE_MIN = 1 <<  0,
	CMDTREE_MAX = 1 <<  1,
	CMDTREE_OPT = 1 <<  2,
	CMDTREE_LEN = 1 <<  3,
} cmdtree_flag_t;

typedef struct {
	int start, stop, type, value;
	cmdtree_flag_t viol;
	int n;
	char *line;
} cmdtree_token_t;

int cmdtree_suggest( cmdtree_token_t *t, const cmdtree_opt_t *opt, char *errmsg );
int cmdtree_select( cmdtree_token_t *t, const cmdtree_opt_t *opt, char *errmsg );
int cmdtree_token_word( char *word, cmdtree_token_t *tset );
int cmdtree_tokenize(cmdtree_token_t *tset, char *line, int max_tokens);
int cmdtree_list(char *dest, const cmdtree_opt_t *opts,
		 const char *intro, const char *delim, const char *outro);


#endif
