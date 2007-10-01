#ifndef _MAIN_H_
#define _MAIN_H_

#include "string.h"

#define MAX_CLIENTS  16
#define MEDLEN       1024
#define SHORTLEN     32

#define MAX_MSG      256

struct string_pair {
	int  key;
	char name[SHORTLEN];
};

struct params_struct;
typedef struct params_struct params_t;

int lookup_key(struct string_pair *pairs, int n_pairs, char *name);

char* key_list(struct string_pair *pairs, int n_pairs, char *separator);

char* key_list_marker(struct string_pair *pairs, int n_pairs, char *separator,
		      int marker_key, char *marker);

#endif
