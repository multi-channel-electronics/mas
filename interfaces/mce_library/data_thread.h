#ifndef __DATA_THREAD_H__
#define __DATA_THREAD_H__

#include <mcecmd.h>
#include <mcedata.h>

#include <pthread.h>

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
