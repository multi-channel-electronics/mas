#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <libmaslog.h>

#include "data_ioctl.h"
#include "mcedata.h"

/* #define LOG_LEVEL_CMD     LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_OK  LOGGER_DETAIL */
/* #define LOG_LEVEL_REP_ER  LOGGER_INFO */


#define EXIT_LAST      1
#define EXIT_READ      2
#define EXIT_WRITE     3
#define EXIT_STOP      4
#define EXIT_EOF       5
#define EXIT_COUNT     6 



/* Data connection */

int mcedata_init(mcedata_t *mcedata, int mcecmd_handle, const char *dev_name)
{
	if (mcedata==NULL) return -1;
	mcedata->mcecmd_handle = mcecmd_handle;

	strcpy(mcedata->dev_name, dev_name);

	mcedata->fd = open(mcedata->dev_name, 0);
	if (mcedata->fd <= 0) {
		fprintf(stderr, "Could not open mce data '%s'\n",
			mcedata->dev_name);
		return -1;
	}		
	
	return 0;
}


int mcedata_acq_reset(mce_acq_t *acq, mcedata_t *mcedata)
{
	memset(acq, 0, sizeof(*acq));
	acq->mcedata = mcedata;
	return 0;
}
	

int mcedata_acq_setup(mce_acq_t *acq, int options, int cards, int frame_size,
		      const char *filename)
{
	int ret_val = 0;

	acq->frame_size = frame_size;
	acq->cards = cards;
	acq->options = options;
	strcpy(acq->filename, filename);

	if (acq->fout != NULL) fclose(acq->fout);
	acq->fout = fopen64(acq->filename, "a");
	if (acq->fout == NULL) {
		fprintf(stderr, "Could not open output file '%s'\n",
			acq->filename);
		acq->filename[0] = 0;
		return -1;
	}

	// Set frame size in driver.
	ret_val = mce_set_datasize(acq->mcedata->fd, acq->frame_size);
	if (ret_val != 0) {
 		fprintf(stdout, "Could not set data size [%i]\n", ret_val);
		return -1;
	}

	// Configure quiet transfer mode



	return 0;
}


/* int mcedata_acq_now(mce_acq_t *acq, int n_frames, int cards); */

static int copy_frames(mce_acq_t *acq);

int mcedata_acq_go(mce_acq_t *acq, int n_frames)
{
	int ret_val = 0;
	int card_id;
	int para_id;

	u32 args[2];
	if (acq->fout == NULL) {
		fprintf(stderr, "Acq output file not initialized.\n");
		return -1;
	}

	// Configure information interval


	// Set number of acq_frames (HOTWIRED)
	card_id = 0x02; //CC
	para_id = 0x53; //ret_dat_s
	args[0] = 0;
	args[1] = n_frames - 1;
	ret_val = mce_write_block(acq->mcedata->mcecmd_handle,
				  card_id, para_id, 2, args);
	if (ret_val != 0) {
		fprintf(stderr, "Could not set ret_dat_s! [%#x]\n", ret_val);
		return -1;
	}

	// Start acquisition
	para_id = 0x16; //ret_dat
	switch (acq->cards) {
	case MCEDATA_RC1:
		card_id = 3;
		break;
	case MCEDATA_RC2:
		card_id = 4;
		break;
	case MCEDATA_RC3:
		card_id = 5;
		break;
	case MCEDATA_RC4:
		card_id = 6;
		break;
	case MCEDATA_RC1 | MCEDATA_RC2 | MCEDATA_RC3 | MCEDATA_RC4:
		card_id = 0xb;
		break;
	default:
		fprintf(stderr, "Invalid card set selection [%#x]\n",
			acq->cards);
		return -1;
	}

	ret_val = mce_start_application(acq->mcedata->mcecmd_handle,
					card_id, para_id);
	if (ret_val != 0) {
		fprintf(stderr, "Could not set ret_dat_s! [%#x]\n", ret_val);
		return -1;
	}

	acq->n_frames = n_frames;
	ret_val = copy_frames(acq);
	if (ret_val != n_frames) {
		fprintf(stderr, "Data write wanted %i frames and got %i\n",
			acq->n_frames, ret_val);
	}
	return 0;
}

static int count_bits( int bits )
{
	int c = 0;
	while (bits!=0) {
		c = (bits & 1);
		bits = bits >> 1;
	}
	return c;
}

static int sort_columns( mce_acq_t *acq, u32 *data )
{
	u32 temp[MCEDATA_PACKET_MAX];

	int header_size = 43;
	int footer_size = 1;

	int columns = 8;
	int cards_in = count_bits(acq->cards);
	int cards_out = cards_in;
	int rows = (acq->frame_size - header_size - footer_size) /
		columns / cards_in;

	int data_size_out = cards_out*rows*columns;
	int data_size_in = cards_out*rows*columns;

	int c, r, c_in = 0;

	memcpy(temp, data, header_size*sizeof(*temp));
	memset(temp + header_size, 0, data_size_out*sizeof(*temp));
	memcpy(temp + header_size + data_size_out,
	       data + header_size + data_size_in,
	       footer_size*sizeof(*temp));
	       
	for (c=0; c<cards_out; c++) {
		if ( (acq->cards & (1 << c)) == 0 ) 
			continue;
		
		for (r=0; r<rows; r++) {
			memcpy(temp+header_size + (r*cards_out + c)*columns,
			       data+header_size + (rows*c_in + r)*columns,
			       columns*sizeof(*temp));
		}
		c_in++;				
	}
	
	memcpy(data, temp,
	       (header_size+footer_size+data_size_out)*sizeof(*data));

	return 0;	       
}

static int copy_frames(mce_acq_t *acq)
{
	int ret_val = 0;
	int done = 0;
	int count = 0;
	int index = 0;
	u32 *data = malloc(acq->frame_size);
	
	if (data==NULL) {
		fprintf(stderr, "Could not allocate frame buffer of size %i\n",
			acq->frame_size);
		return -1;
	}

	while (!done) {
		ret_val = read(acq->mcedata->fd, 
				   (void*)data + index,
				   acq->frame_size - index);
		if (ret_val<0) {
			if (errno==EAGAIN) {
				usleep(1000);
			} else {
				// Error: clear rest of frame and quit
				fprintf(stderr,
					"read failed with code %i\n", ret_val);
				memset((void*)data + index, 0, acq->frame_size - index);
				done = EXIT_READ;
				break;
			}
		} else if (ret_val==0) {
			done = EXIT_EOF;
		} else
			index += ret_val;

		// Only dump complete frames to disk
		if (index >= acq->frame_size) {

			sort_columns( acq, data );

			fwrite(data, acq->frame_size, 1, acq->fout);
			index = 0;
			if (++count >= acq->n_frames)
				done = EXIT_COUNT;
		}
		
	}

	return count;
}


/* /\* REPATRIATED FROM MCECMD *\/ */

/* /\* Ioctl related - note argument is fd, not handle (fixme) *\/ */

/* int mcedata_ioctl(mcedata_t *mcedata, int key, unsigned long arg); */
/* int mcedata_framesize(mcedata_t *mcedata, int datasize); */
/* int mcedata_clear(mcedata_t *mcedata); */
/* int mcedata_fakestop(mcedata_t *mcedata); */

/* int mcedata_qt_setup(mcedata_t *mcedata, int frame_index); */
/* int mcedata_qt_enable(mcedata_t *mcedata, int on); */
