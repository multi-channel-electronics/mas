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
};

enum {
	CMDTREE_TERMINATOR = 0,
/* Insert additional enums between here and "QT_USERBASE" */

	CMDTREE_USERBASE
};


int cmdtree_list(char *dest, cmdtree_opt_t *opts,
		 const char *intro, const char *delim, const char *outro);

int cmdtree_decode(char *line, int *args, int max_args, cmdtree_opt_t *opts,
		   char *errmsg);

int cmdtree_translate(int *dest, const int *src, int nargs,
		      cmdtree_opt_t *src_opts);



#endif
