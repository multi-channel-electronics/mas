#ifndef _SERVER_H_
#define _SERVER_H_

#include <socks.h>
#include "main.h"

typedef struct {

	char name[MEDLEN];
	int  blocking;

} data_client_t;


typedef struct server_struct {

	char serve_address[MEDLEN];
	int  daemon;

	listener_t listener;
	data_client_t clients[MAX_CLIENTS];

	char work_dir[MEDLEN];

} server_t;


int  command(params_t *p, int client_idx, char *str);


#endif
