#ifndef _CMDTREE_H_
#define _CMDTREE_H_

struct cmdtree_opt_struct;

typedef struct cmdtree_opt_struct cmdtree_opt_t;

struct cmdtree_opt_struct {
	char name[32];
	int type;
	int id;
	int min_args;
	int max_args;
	cmdtree_opt_t *sub_opts;
	void *cargo;
};

enum {
	CMDTREE_TERMINATOR = 0,
	CMDTREE_STRINGS,
	CMDTREE_INTEGERS,

/* Insert additional enums between here and "QT_USERBASE" */

	CMDTREE_USERBASE
};


int cmdtree_list(char *dest, cmdtree_opt_t *opts,
		 const char *intro, const char *delim, const char *outro);

int cmdtree_decode(char *line, int *args, int max_args, cmdtree_opt_t *opts,
		   char *errmsg);

int cmdtree_translate(int *dest, const int *src, int nargs,
		      cmdtree_opt_t *src_opts);


#define MAX_TOKENS 64
#define WHITESPACE " \t\n\r"


enum {
	CMDTREE_UNDEF,
	CMDTREE_SELECT,
	CMDTREE_INTEGER,
	CMDTREE_STRING
};

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
} token_t;

typedef struct {
	int start[MAX_TOKENS];
	int stop[MAX_TOKENS];
	int n;
	char *line;
	int type[MAX_TOKENS];
	int value[MAX_TOKENS];
	cmdtree_flag_t viol[MAX_TOKENS];
} token_set_t;


int tokenize(token_set_t *tset, char *line);

#endif
