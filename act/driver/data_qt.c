/*
  data_qt.c

  Quiet Transfer Mode support

  Information interrupts from DSP are directed to
  data_frame_contribute, which checks for consistency of the informed
  value and then updates tail_index in the driver.

  Following information, a tasklet is scheduled to update the head
  index on the DSP.

  QT configuration is achieved with calls to data_qt_setup and
  data_qt_enable.

*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#include "data.h"
#include "data_qt.h"
#include "dsp_driver.h"
#include "mce_driver.h"

#define SUBNAME "mce_qti_handler: "
int mce_qti_handler (dsp_message *msg, unsigned long data)
{
	frame_buffer_t *dframes = (frame_buffer_t *)data;
	int card = dframes - data_frames;
	dsp_qtinform *qti = (dsp_qtinform*)msg;

	PRINT_INFO(SUBNAME
		   "update head to %u with %u drops; active tail is %u\n",
		   qti->dsp_head, qti->dsp_drops,
		   qti->dsp_tail);

	/* Check consistency of buffer_index */

 	data_frame_contribute(qti->dsp_head, card);
	
	// Schedule a grant update

	return 0;
}
#undef SUBNAME

#define SUBNAME "data_grant_callback: "
int  data_grant_callback( int error, dsp_message *msg, int card)
{
	if (error != 0 || msg==NULL) {
		PRINT_ERR(SUBNAME "error or NULL message.\n");
	}
	return 0;		
}
#undef SUBNAME

#define SUBNAME "data_grant_task: "
void data_grant_task(unsigned long data)
{
	frame_buffer_t *dframes = (frame_buffer_t *)data;
	int card = dframes - data_frames;
	int err;

	dsp_command cmd = { DSP_QTS, { DSP_QT_TAIL, dframes->tail_index, 0 } };

	PRINT_INFO(SUBNAME "trying update to tail=%i\n", dframes->tail_index);

	if ( (err=dsp_send_command( &cmd, data_grant_callback, card)) ) {
		// FIX ME: discriminate between would-block errors and fatals!
		PRINT_INFO(SUBNAME "dsp busy; rescheduling.\n");
		tasklet_schedule(&dframes->grant_tasklet);
		return;
	}
}
#undef SUBNAME

//FIX ME: mce_driver.c needs this function instead
/* int data_qt_cmd( dsp_qt_code code, int arg1, int arg2, int card) */
/* { */
/* 	dsp_command cmd = { DSP_QTS, {code,arg1,arg2} }; */
/* 	dsp_message reply; */
/* 	return dsp_send_command_wait( &cmd, &reply, card); */
/* }	 */


#define SUBNAME "data_qt_enable: "
int data_qt_enable(int on, int card) 
{
	frame_buffer_t *dframes = data_frames + card;
	//FIX ME: should call mce_qt_command
	int err = mce_qt_command(DSP_QT_ENABLE, on, 0, card);
	if (!err)
		dframes->data_mode = (on ? DATAMODE_QUIET : DATAMODE_CLASSIC);
	return err;
}
#undef SUBNAME

//FIX ME: requires mce_qt_command
#define SUBNAME "data_qt_configure: "
int data_qt_configure(int qt_interval, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	int err = 0;
	PRINT_INFO(SUBNAME "entry, card: %d\n", card);

	if ( data_qt_enable(0, card) || data_reset(card) )
		err = -1;

	if (!err)
		err = mce_qt_command(DSP_QT_DELTA , dframes->frame_size, 0, card);
	
	if (!err)
		err = mce_qt_command(DSP_QT_NUMBER, dframes->max_index, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_INFORM, qt_interval, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_PERIOD, DSP_INFORM_COUNTS, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_SIZE  , dframes->data_size, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_TAIL  , dframes->tail_index, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_HEAD  , dframes->head_index, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_DROPS , 0, 0, card);

	if (!err)
		err = mce_qt_command(DSP_QT_BASE,
				  ((long)dframes->base_busaddr      ) & 0xffff,
				  ((long)dframes->base_busaddr >> 16) & 0xffff,
				  card);
	
	if (!err)
		err = data_qt_enable(1, card);

	return err;
}
#undef SUBNAME
