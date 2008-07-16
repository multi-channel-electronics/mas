#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>

#include "kversion.h"
#include "mce_options.h"
#include "memory.h"

#include "mce/dsp.h"
#include "mce/mce_errors.h"

#include "mce_driver.h"
#include "mce_ops.h"
#include "data.h"
#include "data_ops.h"
#include "dsp_driver.h"

#define MAX_FERR 100


typedef struct {

	mce_command *command;
	mce_reply   *reply;
	u32          reply_size;
	void* command_busaddr;
	void* reply_busaddr;

	int dma_size;

} mce_comm_buffer;

typedef enum {
	MDAT_IDLE = 0,
	MDAT_CON,
	MDAT_CONOK,
	MDAT_NFY,
	MDAT_HST,
	MDAT_ERR,
} mce_state_t;

struct mce_local {

	struct semaphore sem;
	wait_queue_head_t queue;
	mce_reply *rep;
	int flags;
#define   LOCAL_CMD 0x01
#define   LOCAL_REP 0x02
#define   LOCAL_ERR 0x08

}; //was local_rep

struct mce_control {
	
	struct mce_local local;
	struct semaphore sem;
	struct timer_list timer;
 	struct tasklet_struct hst_tasklet;

	int initialized;

	mce_state_t state;

	int ferror_count;

	int data_flags;
#define   MDAT_DGO   0x001
#define   MDAT_DHST  0x002

	mce_callback callback;

	dsp_message hst_msg;

	mce_comm_buffer buff;

} mce_dat[MAX_CARDS];


int mce_error_register(int card)
{
 	struct mce_control *mdat = mce_dat + card;

	if (mdat->ferror_count == MAX_FERR)
		PRINT_ERR("no further frame errors will be logged.\n");
						 
	return (mdat->ferror_count++ < MAX_FERR);
}

void mce_error_reset( int card )
{
 	struct mce_control *mdat = mce_dat + card;

	mdat->ferror_count = 0;
}


/*
 *   MCE command routines.  Holy, elaborate.
 */

int  mce_CON_dsp_callback( int error, dsp_message *msg, int card);
int  mce_NFY_RP_handler( int error, dsp_message *msg, int card);
void mce_do_HST_or_schedule( unsigned long data );
int  mce_HST_dsp_callback( int error, dsp_message *msg, int card);


/* First set: interrupt context, no blocking and no sems! */

/* Generic error handler; reports error to caller and goes to IDLE state */
#define SUBNAME "mce_command_do_callback: "
int mce_command_do_callback( int error, mce_reply *rep, int card)
{
 	struct mce_control *mdat = mce_dat + card;

	mdat->state = MDAT_IDLE;

	if ( mdat->callback != NULL ) {
		mdat->callback(error, rep, card);
	} else {
		PRINT_INFO(SUBNAME "no callback specified\n");
	} 
	
	// Clear the buffer for the next reply
	memset(mdat->buff.reply, 0, sizeof(*mdat->buff.reply));
	mdat->callback = NULL;

	return 0;
}
#undef SUBNAME


#define SUBNAME "mce_CON_dsp_callback: "
int mce_CON_dsp_callback(int error, dsp_message *msg, int card)
{
 	struct mce_control *mdat = mce_dat + card;

	PRINT_INFO(SUBNAME "entry\n");

	if (mdat->state != MDAT_CON) {
		PRINT_ERR(SUBNAME "unexpected callback! (state=%i)\n",
			  mdat->state);
		return -1;
	}

	if (error<0 || msg==NULL) {
		PRINT_ERR(SUBNAME "called with error %i\n", error);
		if (error == DSP_ERR_TIMEOUT) {
			mce_command_do_callback(-MCE_ERR_INT_TIMEOUT, NULL, card);
		} else {
			mce_command_do_callback(-MCE_ERR_INT_UNKNOWN, NULL, card);
		}
		return 0;
	}

	if (msg->command != DSP_CON) {
		PRINT_ERR(SUBNAME "dsp command was not CON!\n");
		mce_command_do_callback(-MCE_ERR_INT_PROTO, NULL, card);
		return 0;
	}

	if (msg->reply != DSP_ACK) {
		PRINT_ERR(SUBNAME "dsp reply was not ACK!\n");
		mce_command_do_callback(-MCE_ERR_INT_FAILURE, NULL, card);
		return 0;
	}

	mdat->state = MDAT_CONOK;
	PRINT_INFO(SUBNAME "state<-CONOK\n");
	
	return 0;
}
#undef SUBNAME


#define SUBNAME "mce_NFY_RP_handler: "
int mce_NFY_RP_handler( int error, dsp_message *msg, int card)
{
 	struct mce_control *mdat = mce_dat + card;

	// We'll just trust the NFY for now, assuming no error.
	if ( error || (msg==NULL) ) {
		PRINT_ERR(SUBNAME "called error=%i, msg=%lx\n",
			  error, (unsigned long)msg);
		mce_command_do_callback(-MCE_ERR_INT_SURPRISE, NULL, card);
		return 0;
	}

	if (mdat->state != MDAT_CONOK) {
		PRINT_ERR(SUBNAME "unexpected state=%i\n", mdat->state);
		return -1;
	}

	mdat->state = MDAT_NFY;
	mce_do_HST_or_schedule( (unsigned long)mdat );

	return 0;
}
#undef SUBNAME


#define HST_FILL(cmd, bus) cmd.command = DSP_HST; \
                           cmd.args[0] = (bus >> 16) & 0xffff; \
                           cmd.args[1] = bus & 0xffff


#define SUBNAME "mce_do_HST_or_schedule: "
void mce_do_HST_or_schedule(unsigned long data)
{
 	struct mce_control *mdat = (struct mce_control *)data;
	int err; // MATT: why is err not returned or used?
	int card = mdat - mce_dat;
	dsp_command cmd;
	HST_FILL(cmd, (u32)mdat->buff.reply_busaddr);

	if (mdat->state != MDAT_NFY) {
		PRINT_ERR(SUBNAME "unexpected state=%i\n", mdat->state);
		return;
	}

	mdat->state = MDAT_HST;;
	if ( (err=dsp_send_command(&cmd, mce_HST_dsp_callback, card)) ) {
		// FIX ME: discriminate between would-block errors and fatals!
		PRINT_ERR(SUBNAME "dsp busy; rescheduling.\n");
		mdat->state = MDAT_NFY;
		tasklet_schedule(&mdat->hst_tasklet);
		return;
	}
	
	return;
}
#undef SUBNAME


#define SUBNAME "mce_HST_dsp_callback: "
int mce_HST_dsp_callback(int error, dsp_message *msg, int card)
{
 	struct mce_control *mdat = mce_dat + card;

	if (mdat->state != MDAT_HST) {
		PRINT_ERR(SUBNAME "unexpected callback! (state=%i)\n",
			  mdat->state);
		return -1;
	}

	if (error<0 || msg==NULL) {
		PRINT_ERR(SUBNAME "called with error %i\n", error);
		if (error == DSP_ERR_TIMEOUT) {
			mce_command_do_callback(-MCE_ERR_INT_TIMEOUT, NULL, card);
		} else {
			mce_command_do_callback(-MCE_ERR_INT_UNKNOWN, NULL, card);
		}
		return 0;
	}

	if (msg->command != DSP_HST) {
		PRINT_ERR(SUBNAME "dsp command was not HST!\n");
		mce_command_do_callback(-MCE_ERR_INT_PROTO, NULL, card);
		return 0;
	}

	if (msg->reply != DSP_ACK) {
		PRINT_ERR(SUBNAME "dsp reply was not ACK!\n");
		mce_command_do_callback(-MCE_ERR_INT_FAILURE, NULL, card);
		return 0;
	}

	mce_command_do_callback(0, mdat->buff.reply, card);
	return 0;
}
#undef SUBNAME

//Command must already be in mdat->buff.command
#define SUBNAME "mce_send_command_now: "
int mce_send_command_now (int card)
{
 	struct mce_control *mdat = mce_dat + card;
	int err = 0;
	u32 baddr = (u32)mdat->buff.command_busaddr;
	
	dsp_command cmd = {
		DSP_CON,
		{ (baddr >> 16) & 0xffff, baddr & 0xffff, 0 }
	};

	PRINT_INFO(SUBNAME "Sending CON [%#08x %#04x %#04x]\n",
		   mdat->buff.command->command,
		   (int)mdat->buff.command->para_id,
		   (int)mdat->buff.command->card_id);
	
	if ( (err=dsp_send_command( &cmd, mce_CON_dsp_callback, DEFAULT_CARD))) {
		PRINT_INFO(SUBNAME "dsp_send_command failed (%#x)\n",
			  err);
		switch(-err) {
		case EAGAIN:
			return -MCE_ERR_INT_BUSY;
		//case EIO:
		//case ERESTARTSYS:
		default:
			return -MCE_ERR_INT_UNKNOWN;
		}
	}

	return 0;
}
#undef SUBNAME

#define SUBNAME "mce_send_command_timer: "
void mce_send_command_timer(unsigned long data)
{
	struct mce_control *mdat = (struct mce_control *)data;
	int card = mdat - mce_dat;

	if (mdat->state == MDAT_IDLE) {
		PRINT_INFO(SUBNAME "timer ignored\n");
		return;
	}

	PRINT_ERR(SUBNAME "mce reply timed out!\n");
	mce_command_do_callback( -MCE_ERR_TIMEOUT, NULL, card);
}
#undef SUBNAME


#define SUBNAME "mce_send_command: "
int mce_send_command(mce_command *cmd, mce_callback callback, int non_block, int card)
{
 	struct mce_control *mdat = mce_dat + card;
	int ret_val = 0;
	
	if (non_block) {
		if (down_trylock(&mdat->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&mdat->sem))
			return -ERESTARTSYS;
	}
	
	if (mdat->state != MDAT_IDLE) {
		PRINT_INFO(SUBNAME "transaction in progress (state=%i)\n",
			   mdat->state);
		ret_val = -MCE_ERR_ACTIVE;
		goto up_and_out;
	}
	
	// Register callback, advance state, enable timer.
	memcpy(mdat->buff.command, cmd, sizeof(*cmd));
	mdat->callback = callback;
	mdat->state = MDAT_CON;
	mod_timer(&mdat->timer, jiffies + MCE_DEFAULT_TIMEOUT);

	// Try command
	if ( (ret_val = mce_send_command_now(card)) ) {
		PRINT_INFO(SUBNAME "send now failed [%i]!\n", ret_val);
		mdat->state = MDAT_IDLE;
		mdat->callback = NULL;
	}

 up_and_out:
	up(&mdat->sem);
	return ret_val;
}
#undef SUBNAME

//mce_send_command_user lived here once upon a time


/******************************************************************/

/*
  Upon receipt of NFY, dsp_int_handler calls mce_int_handler.  The
  mce_int_handler determines if packet is DA or RP, and calls the
  associated handler, either mce_da_hst or mce_rp_hst.  The *_hst
  functions are responsible for immediately 
*/

#define SUBNAME "mce_da_hst_callback: "
int mce_da_hst_callback(int error, dsp_message *msg, int card)
{
	//FIXME: "error" case should be natural and handled smoothly.
	// What will happen to this "data"?
 	struct mce_control *mdat = mce_dat + card;

	if (error || msg==NULL) {

		if (!mce_error_register(card)) return -1;

		PRINT_ERR(SUBNAME "called with error = %i\n", error);
		if (msg==NULL) {
			PRINT_ERR(SUBNAME "dsp_message is NULL\n");
		} else {
			PRINT_INFO(SUBNAME
				   "dsp_message=(%06x, %06x, %06x, %06x)\n",
				   msg->type, msg->command,
				   msg->reply, msg->data);
		}
		mdat->data_flags &= ~MDAT_HST;
		return -1;
	}

	if (mdat->data_flags != (MDAT_DHST)) {
		if (mce_error_register(card))
			PRINT_ERR(SUBNAME "unexpected flags state %#x\n",
				  mdat->data_flags);
		mdat->data_flags = 0;
		return -1;
	}

	if (data_frame_increment(card) && mce_error_register(card)) {
		PRINT_ERR(SUBNAME "frame_increment error; packet lost\n");
	}

	//Only action is to increment tail pointer or whatever

	mdat->data_flags &= ~MDAT_DHST;
	return 0;
}
#undef SUBNAME

#define SUBNAME "mce_da_hst_now: "
int mce_da_hst_now(int card)
{
 	struct mce_control *mdat = mce_dat + card;
	int err = 0;
	u32 baddr;
	dsp_command cmd;

	PRINT_INFO(SUBNAME "NFY-DA accepted, sending HST\n");

	if ( (mdat->data_flags & MDAT_DHST) && mce_error_register(card) ) {
		PRINT_ERR(SUBNAME
			  "NFY-DA interrupts outstanding HST!\n");
	}

	data_frame_address(&baddr, card);
	HST_FILL(cmd, baddr);

	if ((err = dsp_send_command(&cmd, mce_da_hst_callback, DEFAULT_CARD))) {
		PRINT_INFO(SUBNAME "dsp_send_command failed!\n");
		if (mce_error_register(card)) {
			PRINT_ERR(SUBNAME "dsp_send_command error %i; "
				   "packet dropped\n",
				   err);
		}
		return -1;
	}

	mdat->data_flags |= MDAT_DHST;

	return 0;
}
#undef SUBNAME


#define SUBNAME "mce_int_handler: "
int mce_int_handler( dsp_message *msg, unsigned long data )
{
	struct mce_control *mdat = (struct mce_control *)data;
	frame_buffer_t *dframes = data_frames;
	dsp_notification *note = (dsp_notification*) msg;
	int packet_size = (note->size_lo | (note->size_hi << 16)) * 4;
	int card = mdat - mce_dat;

       	if (note->type != DSP_NFY) {
		PRINT_ERR(SUBNAME "message is not NFY!\n");
		return -1;
	}

	switch(note->code) {

	case DSP_RP:

		PRINT_INFO(SUBNAME "NFY RP identified\n");
		mce_NFY_RP_handler(0, msg, card);

		break;

	case DSP_DA:
		if (packet_size != dframes->data_size) {
			if (mce_error_register(card))
				PRINT_ERR(SUBNAME
					  "unexpected DA packet size"
					  "%i bytes; dropping.\n",
					  packet_size);
			return -1;
		} else
			mce_da_hst_now(card);
		break;

	default:
		PRINT_ERR(SUBNAME "unknown packet type, ignoring\n");
	}

	return 0;
}
#undef SUBNAME

//mce_send_command_wait (_callback) lived here once upon a time...

int mce_buffer_allocate(mce_comm_buffer *buffer)
{
	// Create DMA-able area.  Use only one call since the two
	// buffers are so small.

	int offset = ( sizeof(mce_command) + (DMA_ADDR_ALIGN-1) ) &
		DMA_ADDR_MASK;

	int size = offset + sizeof(mce_reply);

	buffer->command = (mce_command*) dsp_allocate_dma(
		size, (unsigned int*)&buffer->command_busaddr);
	if (buffer->command==NULL)
		return -ENOMEM;

	buffer->reply = (mce_reply*) ((char*)buffer->command + offset);
	buffer->reply_busaddr = buffer->command_busaddr + offset;
	buffer->dma_size = size;
	
	PRINT_INFO("cmd/rep[virt->bus]: [%lx->%lx]/[%lx->%lx]\n",
		   (long unsigned int)buffer->command,
		   virt_to_bus(buffer->command),
		   (long unsigned int)buffer->reply,
		   virt_to_bus(buffer->reply));
	
	return 0;
}

int mce_buffer_free(mce_comm_buffer *buffer)
{
	if (buffer->command!=NULL) {
		dsp_free_dma(buffer->command, buffer->dma_size,
			     (int)buffer->command_busaddr);
	}

	buffer->command = NULL;
	buffer->reply   = NULL;
	
	return 0;
}


int mce_proc(char *buf, int count)
{
 	struct mce_control *mdat = mce_dat;
	int len = 0;
	if (len < count) {
		len += sprintf(buf+len, "    state:    ");
		switch (mdat->state) {
		case MDAT_IDLE:
			len += sprintf(buf+len, "idle\n");
			break;
		case MDAT_CON:
			len += sprintf(buf+len, "command initiated\n");
			break;
		case MDAT_CONOK:
			len += sprintf(buf+len, "command sent\n");
			break;
		case MDAT_NFY:
			len += sprintf(buf+len, "reply notified\n");
			break;
		case MDAT_HST:
			len += sprintf(buf+len, "reply queried\n");
			break;
		case MDAT_ERR:
			len += sprintf(buf+len, "error\n");
			break;
		}
	}

	return len;
}


/* Special DSP functionality */

int mce_hardware_reset(int card)
{
	dsp_command cmd = { DSP_RCO, {0,0,0} };
	dsp_message msg;
	return dsp_send_command_wait(&cmd, &msg, card);
}

int mce_interface_reset(int card)
{
	dsp_command cmd = { DSP_RST, {0,0,0} };
	dsp_message msg;
	return dsp_send_command_wait(&cmd, &msg, card);
}


#define SUBNAME "mce_init: "
int mce_init()
{
	int err = 0;
	PRINT_INFO(SUBNAME "entry\n");
	
	err = mce_ops_init();
	if(err != 0) goto out;

	//FIX ME:: add error checking for data_ops
	data_ops_init();
	
	PRINT_INFO(SUBNAME "ok\n");
	return 0;
 out:
	PRINT_ERR(SUBNAME "exiting with error\n");
	return err;
}
#undef SUBNAME

#define SUBNAME "mce_probe: "
int mce_probe(int dsp_version, int card)
{
 	struct mce_control *mdat = mce_dat + card;
	int err = 0;

	PRINT_INFO(SUBNAME "entry\n");
	mdat->initialized = 1;

	//Init data module
	//	err = data_init(FRAME_BUFFER_SIZE, DEFAULT_DATA_SIZE);
	//	if (err) {
	//		PRINT_ERR(SUBNAME "mce data module init failure\n");
	//		goto out;
	//	}

	err = data_probe(dsp_version, card, FRAME_BUFFER_SIZE, DEFAULT_DATA_SIZE);
	if (err !=0 ) goto out;

	err = mce_buffer_allocate(&mdat->buff);
	if (err != 0) goto out;

	err = mce_ops_probe(card);
	if (err != 0) goto out;

	init_MUTEX(&mdat->sem);

	init_MUTEX(&mdat->local.sem);
	init_waitqueue_head(&mdat->local.queue);

   	tasklet_init(&mdat->hst_tasklet,
		     mce_do_HST_or_schedule, (unsigned long)mdat);

	init_timer(&mdat->timer);
	mdat->timer.function = mce_send_command_timer;
	mdat->timer.data = (unsigned long)mdat;

	mdat->state = MDAT_IDLE;
	mdat->data_flags = 0;

	// Set up command and quiet transfer handlers
	dsp_set_handler(DSP_QTI, mce_qti_handler, (unsigned long)mdat, card);
	dsp_set_handler(DSP_NFY, mce_int_handler, (unsigned long)mdat, card);

	PRINT_INFO(SUBNAME "init ok.\n");

	return 0;

 out:
	PRINT_ERR(SUBNAME "init error!\n");

	mce_remove(card);
	return err;
}
#undef SUBNAME

#define SUBNAME "mce_cleanup: "
int mce_cleanup()
{
	PRINT_INFO(SUBNAME "entry\n");
	
	mce_ops_cleanup();
	data_ops_cleanup();
	
	PRINT_INFO(SUBNAME "ok\n");
	return 0;
}
#undef SUBNAME

#define SUBNAME "mce_remove: "
int mce_remove(int card)
{
 	struct mce_control *mdat = mce_dat + card;

	PRINT_INFO(SUBNAME "entry\n");

	if (!mdat->initialized) return 0;

	del_timer_sync(&mdat->timer);
	tasklet_kill(&mdat->hst_tasklet);

  	mce_buffer_free(&mdat->buff);
	
	data_remove(card);

	PRINT_INFO(SUBNAME "ok\n");
	return 0;
}
#undef SUBNAME
