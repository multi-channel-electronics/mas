/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "context.h"
#include "data_thread.h"
#include "frame_manip.h"


/*
 * data_go
 *
 *  - lock down
 *  - flush data device
 *  - start data collector
 *  - inform driver of frame size
 *  - set sequence numbers
 *  - send go to selected column(s)
 */
/*
void data_setflag(data_thread_t *d, int flag)
{
	d->flags |= flag;
}

void data_clearflag(data_thread_t *d, int flag)
{
	d->flags &= ~flag;
}

int data_checkflags(data_thread_t *d, int flags)
{
	return d->flags & flags;
}

int  data_stop(params_t *p, char *errstr)
{
	data_setflag(&p->data_data, FLAG_STOP);

	return command_stop(&p->mce_comms, &p->frame_setup);
}

int data_reset(params_t *p)
{
	data_kill(p);
	data_clearflag( &p->data_data, FLAG_ALL );
	return 0;
}
*/


#define SUBNAME "data_go: "



/* DATA THREAD:
 *
 * data_thread is launched just prior to the initiation of the GO.
 */

inline int stop_bit(char *packet) {
	//Stop flag is bit 0 of first word
	return *( (u32*)packet + 0) & 1;
}

 
void *data_thread(void *p_void)
{
	int ret_val;
	data_thread_t *d =(data_thread_t*) p_void;
	int size = d->acq->frame_size*sizeof(u32);
	mcedata_storage_t *acts = d->acq->storage;
	u32 *data = malloc(size);
	int done = 0;

	if (data==NULL) {
		sprintf(d->errstr, "Could not allocate data buffer memory\n");
		d->state = MCETHREAD_ERROR;
		return (void*)d;
	}		

	printf("data_thread: entry\n");
	//logger_print(&p->logger, "Data thread starting\n");

	d->count = 0;
	d->chksum = 0;
	d->drop = 0;

	int index = 0;
	int count = 0;
	while (!done) {

		if (acts->pre_frame != NULL && acts->pre_frame(d->acq)) {
				fprintf(stderr, "pre_frame action failed\n");
		}
	
        ret_val = mcedata_read(d->acq->context, (void*)data + index,
                size - index);

		if (ret_val<0) {
			if (errno==EAGAIN) {
				usleep(1000);
			} else {
				// Error: clear rest of frame and quit
				fprintf(stderr,
					"read failed with code %i\n", ret_val);
				memset((void*)data + index, 0, size - index);
				done = EXIT_READ;
				break;
			}
		} else if (ret_val==0) {
			done = EXIT_EOF;
		} else
			index += ret_val;

		if (d->state == MCETHREAD_STOP)
			done = EXIT_STOP;
		
		// Only dump complete frames to disk
		if (index < size)
			continue;

		// Logical formatting
		sort_columns( d->acq, data );

		if ( (acts->post_frame != NULL) && acts->post_frame( d->acq, count, data ) ) {
			fprintf(stderr, "post_frame action failed\n");
		}

		index = 0;
		if (++count >= d->acq->n_frames)
			done = EXIT_COUNT;
	}

	d->state = MCETHREAD_IDLE;
/*
	sprintf(errstr, "Data thread exiting with %i of %i expected frames; ",
		d->count,
		p->frame_setup.seq_last - p->frame_setup.seq_first + 1);

	switch (done) {
		
	case EXIT_STOP:
		strcat(errstr, "user STOP received.\n");
		break;

	case EXIT_READ:
		strcat(errstr, "device read error.\n");
		break;

	case EXIT_WRITE:
		strcat(errstr, "file write error.\n");
		break;

	case EXIT_LAST:
		strcat(errstr, "last frame detected.\n");
		break;

	case EXIT_EOF:
		strcat(errstr, "device signaled EOF.\n");
		break;

	default:
		strcat(errstr, "unexpected loop termination.\n");
	}

	printf("data_thread: %s\n", errstr);
	logger_print(&p->logger, errstr);

	if (data_refile_stop(&p->datafile)) {
		sprintf(errstr, "error closing data file '%s'.\n",
			p->datafile.filename);
		printf("data_thread: %s\n", errstr);
		logger_print(&p->logger, errstr);
	}	

	data_clearflag(d, FLAG_STOP);
	data_setflag(d, FLAG_DONE);
	d->done = 1;
	d->error = err;
*/
	return (void*)d;
}

int data_thread_launcher(data_thread_t *d)
{
	d->state = MCETHREAD_LAUNCH;
	int err = pthread_create(&d->thread, NULL, data_thread, (void*)d);
	
	return err;
}

