/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef __DATA_THREAD_H__
#define __DATA_THREAD_H__

#include <pthread.h>

#include <mce_library.h>

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

int  mcedata_thread_launcher();

#endif
