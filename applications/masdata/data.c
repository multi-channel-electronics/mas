#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <libmaslog.h>

#include <mcecmd.h>
#include "masdata.h"
#include "data.h"
#include "mcecomms.h"

#define BUFSIZE 8192

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

int   data_thread_launcher(params_t *p);
char* data_reformat( void );
void  data_prepare(params_t *p, char *dest, char *src);


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

int data_fakestop(mce_comms_t *mce)
{
	return mce_fake_stopframe(mce->mcedata_fd);
}
	
int  data_stop(params_t *p, char *errstr)
{
	data_setflag(&p->data_data, FLAG_STOP);

	return command_stop(&p->mce_comms, &p->frame_setup);
}

int data_kill(params_t *p)
{
	// Send stop, maybe
	// data_stop( &p->data_data, &p->mce_comms, &p->frame_setup);

	// Sleep for data thread killing
	data_setflag( &p->data_data, FLAG_STOP);
	
	sleep(1);
	if (! data_checkflags (&p->data_data, FLAG_DONE) )
		return -1;

	return 0;
}

int data_reset(params_t *p)
{
	data_kill(p);
	data_clearflag( &p->data_data, FLAG_ALL );
	return 0;
}



#define SUBNAME "data_go: "

int data_go(params_t *p, char *errstr)
{
	int err = -1;

	mce_comms_t *mce = &p->mce_comms;
	frame_setup_t *frames = &p->frame_setup;
	data_thread_t *data = &p->data_data;

	if (data_checkflags(data, FLAG_GO | FLAG_DONE)) {
		sprintf(errstr, "GO in progress.");
		goto out;
	}

	if ( (err = data_refile_start(&p->datafile, 0))!=0 ) {
		sprintf(errstr, "Could not initialize output file '%s'.",
			p->datafile.filename);
		goto out;
	}

	printf("You made it.\n");

	if ( (err = mce_empty_data(mce->mcedata_fd)) !=0) {
		sprintf(errstr, "Failed to clear data driver [%i].", err);
		goto out;
	}

	if ( (err = mce_set_datasize(mce->mcedata_fd,
				     frames->data_size)) != 0) {
		sprintf(errstr, "Failed to set frame size [%i].", err);
		goto out;
	}

	if ( (err = command_sequence(mce, frames->seq_first,
				     frames->seq_last)) != 0 ) {
		sprintf(errstr, "Failed to set frame sequence [%i].", err);
		goto out;
	}

	// Flags up
	data_setflag(data, FLAG_GO);

	// Thread now
	if ( (err = data_thread_launcher(p)) != 0 ) {
		sprintf(errstr, "Failed to launch data thread.");
		data_clearflag(data, FLAG_GO);
		goto out;
	}

	//Send go
	if ( (err = command_go(mce, frames)) != 0) {
		sprintf(errstr, "GO command failed [%i].",
			err);
		data_setflag(data, FLAG_STOP);
		goto out;
	}

out:
	if (err) {
		logger_print(&p->logger, errstr);
		fprintf(stderr, errstr);
	} else {
		sprintf(errstr,
			"Acquiring: size=%i, first=%i, last=%i, file=%s",
			frames->frame_size, frames->seq_first,
			frames->seq_last, p->datafile.filename);
		logger_print(&p->logger, errstr);
	}

	return err;
}


/* DATA THREAD:
 *
 * data_thread is launched just prior to the initiation of the GO.
 */

inline int stop_bit(char *packet) {
	//Stop flag is bit 0 of first word
	return *( (u32*)packet + 0) & 1;
}

#define EXIT_LAST      1
#define EXIT_READ      2
#define EXIT_WRITE     3
#define EXIT_STOP      4
#define EXIT_EOF       5
 
void *data_thread(void *p_void)
{
	char errstr[1024];

	params_t *p = (params_t*) p_void;
	data_thread_t *d = &p->data_data;

	int fd = p->mce_comms.mcedata_fd;
	int in_size  = p->frame_setup.data_size;
	int out_size = p->frame_setup.frame_size;

	char packet_raw[BUFSIZE];
	char packet_extra[BUFSIZE];

	printf("data_thread: entry\n");
	logger_print(&p->logger, "Data thread starting\n");

	// Allow reformatter to prepare
	data_prepare(p, packet_extra, packet_raw);
	
	d->count = 0;
	d->chksum = 0;
	d->drop = 0;

        int done = data_checkflags(d, FLAG_STOP) ? EXIT_STOP : 0;
	int err = 0;

        while ( !done ) {

                //Read

                int index = 0;
                int target = in_size;
                while (!done && index<target) {
                        err = read(fd,
				   (void*)packet_raw + index,
				   target-index);
			if (err<0) {
				if (errno==EAGAIN) {
					usleep(1000);
				} else {
					// Error: clear rest of frame and quit
					fprintf(stderr,
						"read failed with code %i\n",
						err);
					memset((void*)packet_raw + index, 0,
					       target-index);
					done = EXIT_READ;
					break;
				}
			} else if (err==0) {
				done = EXIT_EOF;
			} else
				index += err;
			if (data_checkflags(d, FLAG_STOP)) done = EXIT_STOP;
                }

		// Switch files if it is time

		if (data_refile_criterion(&p->datafile, d->count)) {
			if (data_refile_now(&p->datafile, d->count)!=0) {
				sprintf(errstr,
					"Could not switch file to '%s'",
					p->datafile.filename);
				logger_print(&p->logger, errstr);
			} else {
				sprintf(errstr,
					"Changed output file to '%s'.",
					p->datafile.filename);
				logger_print(&p->logger, errstr);
			}
		}

		// Pre-process flags and things

		if (stop_bit(packet_raw)) {
			done = EXIT_LAST;
		}

                // Reformat

		char *packet_cooked = data_reformat();

                //Write whole frames only.
		err = fwrite((void*)packet_cooked,
			     out_size, 1, p->datafile.out);
		if (err<=0) {
                        fprintf(stderr,"fwrite failed (%i), packet %i\n",
				err, d->count);
                        done = EXIT_WRITE;
                        break;
                } else
			d->count++;

		if (!done && data_checkflags(d, FLAG_STOP))
			done = EXIT_STOP;
	}

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
/* 	fflush(p->datafile.out); */
/* 	if (data_closefile(&p->datafile, errstr)) { */
/* 		printf("data_thread: %s\n", errstr); */
/* 		logger_print(&p->logger, errstr); */
/* 	}	 */

	data_clearflag(d, FLAG_STOP);
	data_setflag(d, FLAG_DONE);
	d->done = 1;
	d->error = err;

	return (void*)p;
}

int data_thread_launcher(params_t *p)
{
	p->data_data.done = 0;
	p->data_data.error = 0;
	p->data_data.errstr[0] = 0;

	int err = pthread_create(&p->data_data.thread, NULL,
				 data_thread, (void*)p);
	
	return err;
}


/* FRAME DATA PROCESSING:
 *
 * At the start of acqusition, a call is made to data_prepare, which
 * stores key parameters about the input and output frame formats to
 * make per-frame processing as simple as possible.  When frames
 * arrive, they are processed by calls to data_reformat.
 */

inline void  transpose(u32 *src, u32 *dest, int src_rows, int src_cols) {
	int i,j;
	for (i=0; i<src_cols; i++)
		for (j=0; j<src_rows; j++)
			dest[j*src_rows+i] = src[i*src_cols + j];
}

inline void widen(u32 *dest, u32 *src, int dest_cols, int src_cols, int rows)
{
	int i;
	for (i=0; i<rows; i++)
		memcpy(dest+i*dest_cols, src+i*src_cols, src_cols*sizeof(*src));
}

struct data_params_struct {
	int format;

	u32 *src;
	u32 *dest;

	int card_n;
	int card_i;
	int card_cols;
	int dest_cols;
	int rows;

	int  head_count;
	int  src_tail_ofs;
	int  dest_tail_ofs;
	int  tail_count;
} dp;

/* data_prepare:
 *     src     points to the raw data buffer
 *     extra   points to an extra buffer which may be used for cooked data
*/

void data_prepare(params_t *p, char *extra, char *src)
{
	frame_setup_t *f = &p->frame_setup;
	column_config_t *c = &f->colcfgs[f->colcfg_index];

	dp.format = f->format;

	switch(f->format) {

	case FORMAT_RAW:
		dp.src = (void*)src;
		return;

	case FORMAT_LOGICAL:
		dp.card_n = c->cards;
		dp.card_i = c->card_idx;
		dp.rows  = f->rows;
		dp.card_cols = f->columns_card;
		dp.dest_cols = f->columns;

		dp.src   = (u32*)src;
		dp.dest  = (u32*)extra;

		dp.head_count = f->header;

		dp.src_tail_ofs  = dp.head_count + 
			dp.rows * dp.card_cols * dp.card_n;
		dp.dest_tail_ofs = dp.head_count + dp.rows * dp.dest_cols;
		dp.tail_count = f->extra;
		
		// Clear entire data section.
		memset(dp.dest + dp.head_count, 0, (dp.dest_tail_ofs - dp.head_count)*sizeof(u32));

		return;
	}
}

char* data_reformat()
{
	switch(dp.format) {

	case FORMAT_RAW:
		return (char*)dp.src;

	case FORMAT_LOGICAL:

		// Copy header and chksum
		memcpy(dp.dest, dp.src, dp.head_count*sizeof(u32));
		memcpy(dp.dest + dp.dest_tail_ofs, dp.src + dp.src_tail_ofs,
		       dp.tail_count*sizeof(u32));

		// Remap data
		int i;
		for (i=0; i<dp.card_n; i++) {
			widen(dp.dest + dp.head_count + dp.card_cols*(i+dp.card_i),
			      dp.src  + dp.head_count + dp.card_cols*i*dp.rows,
			      dp.dest_cols, dp.card_cols, dp.rows);
		}
		
		return (char*)dp.dest;

	}
	return 0;
}
