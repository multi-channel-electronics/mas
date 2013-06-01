/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
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

//#include "data.h"
//#include "data_qt.h"
#include "dsp_driver.h"
//#include "mce_driver.h"

#if 0
int mce_qti_handler (dsp_message *msg, unsigned long data)
{
	frame_buffer_t *dframes = (frame_buffer_t *)data;
	int card = dframes - data_frames;
	dsp_qtinform *qti = (dsp_qtinform*)msg;

        PRINT_INFO(card,
		   "update head to %u with %u drops; active tail is %u (%u)\n",
		   qti->dsp_head, qti->dsp_drops,
                  qti->dsp_tail, dframes->tail_index);

        /* Are we dropping frames? */

        if (dframes->dropped != qti->dsp_drops) {
                PRINT_ERR(card, "DSP has dropped %i frames to date.\n", qti->dsp_drops);
                dframes->dropped = qti->dsp_drops;
        }

	/* Check consistency of buffer_index */

 	data_frame_contribute(qti->dsp_head, card);
	
	return 0;
}

int data_qt_enable(int on, int card) 
{
	frame_buffer_t *dframes = data_frames + card;
	//FIX ME: should call mce_qt_command
	int err = mce_qt_command(DSP_QT_ENABLE, on, 0, card);
	if (!err)
		dframes->data_mode = (on ? DATAMODE_QUIET : DATAMODE_CLASSIC);
	return err;
}

//FIX ME: requires mce_qt_command
int data_qt_configure(int qt_interval, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	int err = 0;
        PRINT_INFO(card, "entry\n");

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
#endif

#include <mce/new_dsp.h>

int get_qt_command(frame_buffer_t *dframes, int enable, int qt_interval,
                   struct dsp_command *cmd)
{
        cmd->cmd = DSP_CMD_SET_DATA_BUF;
        if (enable)
                cmd->data[0] = dframes->base_busaddr;
        else
                cmd->data[0] = 0;
        cmd->data[1] = dframes->n_frames;
        cmd->data[2] = dframes->frame_size;
        cmd->data[3] = dframes->data_size;
        cmd->data[4] = dframes->update_interval;
        cmd->data[5] = DSP_INFORM_COUNTS; // Timer expiry
        cmd->data[6] = dframes->head_index;
        cmd->data[7] = dframes->tail_index;
        cmd->data[8] = 0; /* drops */
        cmd->data_size = 9;

        cmd->size = cmd->data_size + 1;
        cmd->flags = DSP_EXPECT_DSP_REPLY;
        return 0;
}
