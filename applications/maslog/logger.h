#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>
#include <mce/socks.h>
#include "../../defaults/config.h"

#define MAX_CLIENTS 16
#define MEDLEN 1024

#define MAX_MSG 1024

struct params_struct;

/* client_t - holds information about clients connected to logger */

typedef struct {

	char name[MEDLEN];

} log_client_t;


/* params_t - holds active logger configuration */

typedef struct params_struct {

	char serve_address[MEDLEN];
	int  sock;
	int  daemon;
	int  level;

	char filename[MEDLEN];
	FILE *out;
	int  flags;
#define    FLAG_FLUSH 0x0001
#define    FLAG_QUIT  0x8000
#define    FLAG_ALL   0xffff

	log_client_t clients[MAX_CLIENTS];
	listener_t listener;

} params_t;

void log_init(params_t *p);
int  log_openfile(params_t *p);
int  log_text(params_t *p, char* client_name, char *str);
int  log_flush(params_t *p);
int  log_closefile(params_t *p);
void log_setflag(params_t *p, int flag);
void log_clearflag(params_t *p, int flag);
int  log_quit(params_t *p);
int  log_client_process(params_t *p, int client_idx);

#endif
