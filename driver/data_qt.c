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


#define SUBNAME "mce_qti_handler: "

int mce_qti_handler ( dsp_message *msg, unsigned long data )
{
	dsp_qtinform *qti = (dsp_qtinform*)msg;

	PRINT_INFO(SUBNAME
		   "update head to %u with %u drops; active tail is %u\n",
		   qti->dsp_head, qti->dsp_drops,
		   qti->dsp_tail);

	/* Check consistency of buffer_index */

 	data_frame_contribute( qti->dsp_head );
	
	// Schedule a grant update

	return 0;
}

#undef SUBNAME


#define SUBNAME "data_grant_callback: "

int  data_grant_callback( int error, dsp_message *msg )
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
	int err;

	dsp_command cmd = { DSP_QTS, { DSP_QT_TAIL, frames.tail_index, 0 } };

	PRINT_INFO(SUBNAME "trying update to tail=%i\n", frames.tail_index);

	if ( (err=dsp_send_command( &cmd, data_grant_callback )) ) {
		// FIX ME: discriminate between would-block errors and fatals!
		PRINT_INFO(SUBNAME "dsp busy; rescheduling.\n");
		tasklet_schedule(&frames.grant_tasklet);
		return;
	}
	

}

#undef SUBNAME

int data_qt_cmd( dsp_qt_code code, int arg1, int arg2)
{
	dsp_command cmd = { DSP_QTS, {code,arg1,arg2} };
	dsp_message reply;
	return dsp_send_command_wait( &cmd, &reply );
}	


#define SUBNAME "data_qt_enable: "

int data_qt_enable(int on)
{
	int err = data_qt_cmd(DSP_QT_ENABLE, on, 0);
	if (!err)
		frames.data_mode = (on ? DATAMODE_QUIET : DATAMODE_CLASSIC);
	return err;
}

#undef SUBNAME


#define SUBNAME "data_qt_setup: "

int data_qt_configure( int qt_interval )
{
	int err = 0;
	PRINT_INFO(SUBNAME "entry\n");

	if ( data_qt_enable(0) || data_reset() )
		err = -1;

	if (!err)
		err = data_qt_cmd(DSP_QT_DELTA , frames.frame_size, 0);
	
	if (!err)
		err = data_qt_cmd(DSP_QT_NUMBER, frames.max_index, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_INFORM, qt_interval, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_PERIOD, DSP_INFORM_COUNTS, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_SIZE  , frames.data_size, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_TAIL  , frames.tail_index, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_HEAD  , frames.head_index, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_DROPS , 0, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_BASE,
				  ((long)frames.base_busaddr      ) & 0xffff,
				  ((long)frames.base_busaddr >> 16) & 0xffff);
	
	if (!err)
		err = data_qt_enable(1);

	return err;
}

#undef SUBNAME
