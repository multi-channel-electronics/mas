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

#include "driver.h"

int mce_qti_handler (dsp_message *msg, unsigned long data)
{
	frame_buffer_t *dframes = (frame_buffer_t *)data;
	int card = dframes - data_frames;
	dsp_qtinform *qti = (dsp_qtinform*)msg;

	PRINT_INFO("update head to %u with %u drops; active tail is %u\n",
		   qti->dsp_head, qti->dsp_drops,
		   qti->dsp_tail);

	/* Check consistency of buffer_index */

 	data_frame_contribute(qti->dsp_head, card);
	
	// Schedule a grant update

	return 0;
}


int  data_grant_callback( int error, dsp_message *msg, int card)
{
	if (error != 0 || msg==NULL) {
		PRINT_ERR("error or NULL message.\n");
	}
	return 0;		
}


void data_grant_task(unsigned long data)
{
	frame_buffer_t *dframes = (frame_buffer_t *)data;
	int card = dframes - data_frames;
	int err;

	dsp_command cmd = { DSP_QTS, { DSP_QT_TAIL, dframes->tail_index, 0 } };

	PRINT_INFO("trying update to tail=%i\n", dframes->tail_index);

	if ( (err=dsp_send_command( &cmd, data_grant_callback, card)) ) {
		// FIX ME: discriminate between would-block errors and fatals!
		PRINT_ERR("dsp busy; rescheduling.\n");
		tasklet_schedule(&dframes->grant_tasklet);
		return;
	}
}


int data_qt_enable(int on, int card) 
{
	frame_buffer_t *dframes = data_frames + card;
	//FIX ME: should call mce_qt_command
	int err = dframes->mce->qt_command(DSP_QT_ENABLE, on, 0, card);
	if (!err)
		dframes->data_mode = (on ? DATAMODE_QUIET : DATAMODE_CLASSIC);
	return err;
}


//FIX ME: requires mce_qt_command
int data_qt_configure(int qt_interval, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	int err = 0;
	PRINT_INFO("entry, card: %d\n", card);

	if ( data_qt_enable(0, card) || data_reset(card) )
		err = -1;

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_DELTA , dframes->frame_size, 0, card);
	
	if (!err)
		err = dframes->mce->qt_command(DSP_QT_NUMBER, dframes->max_index, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_INFORM, qt_interval, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_PERIOD, DSP_INFORM_COUNTS, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_SIZE  , dframes->data_size, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_TAIL  , dframes->tail_index, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_HEAD  , dframes->head_index, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_DROPS , 0, 0, card);

	if (!err)
		err = dframes->mce->qt_command(DSP_QT_BASE,
		      ((long)dframes->mem->bus_addr      ) & 0xffff,
		      ((long)dframes->mem->bus_addr >> 16) & 0xffff,
		      card);
	
	if (!err)
		err = data_qt_enable(1, card);

	return err;
}
