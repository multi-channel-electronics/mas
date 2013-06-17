#ifndef __DATA_THREAD_H__
#define __DATA_THREAD_H__

#include <pthread.h>

#include <mce_library.h>

#define EXIT_LAST      1
#define EXIT_READ      2
#define EXIT_WRITE     3
#define EXIT_STOP      4
#define EXIT_EOF       5
#define EXIT_COUNT     6 
#define EXIT_TIMEOUT   7
#define EXIT_KILL      8


typedef struct data_thread_struct {
	
	pthread_t thread;

	mce_acq_t *acq;
	unsigned state;

	int count;
	int chksum;
	int drop;
	
	char errstr[MCE_LONG];

} data_thread_t;


enum { MCETHREAD_IDLE = 0,
       MCETHREAD_LAUNCH,
       MCETHREAD_GO,
       MCETHREAD_STOP,
       MCETHREAD_PING,
       MCETHREAD_ERROR
};

int  data_thread_launcher();
int  data_thread_stop();
int  data_thread_go();

#endif
