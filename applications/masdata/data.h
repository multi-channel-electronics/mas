#ifndef _DATA_H_
#define _DATA_H_

#include <stdio.h>
#include <pthread.h>

#include "main.h"

#include "files.h"
#include "frames.h"
#include "mcecomms.h"


typedef struct data_thread_struct {

	int done;
	int error;
	char errstr[MEDLEN];
	struct params_struct *parent;
	pthread_t thread;

	int count;
	int chksum;
	int drop;

	int  flags;
#define    FLAG_FLUSH 0x0001
#define    FLAG_GO    0x0002
#define    FLAG_DONE  0x0004
#define    FLAG_STOP  0x0008
#define    FLAG_QUIT  0x8000
#define    FLAG_ALL   0xffff

} data_thread_t;


void data_init(params_t *p);
int  data_go(params_t *p, char *errstr);
int  data_stop(params_t *p, char *errstr);
int  data_reset(params_t *p);

int  data_fakestop(mce_comms_t *mce);
void data_setflag(data_thread_t *p, int flag);
void data_clearflag(data_thread_t *p, int flag);
int  data_checkflags(data_thread_t *d, int flags);

int  data_kill(params_t *p);

int  data_client_process(params_t *p, int client_idx);
int  data_client_reply(params_t *p, int client_idx, char *message);

#endif
