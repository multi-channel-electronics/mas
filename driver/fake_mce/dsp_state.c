#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <mce_options.h>

#include "mce_fake.h"
#include "dsp_state.h"
#include "../dsp_driver.h"

dsp_state_t dsp_state;

/* Prototypes */

void msg_now(unsigned long data);

/* Initialization */

int dsp_state_init()
{
	dsp_state.mce_con = NULL;

	PRINT_ERR("INIT!\n");

	init_timer(&dsp_state.timer);
	dsp_state.timer.function = dsp_timer_function;
	dsp_state.timer.data = (unsigned long)&dsp_state;
	
	tasklet_init(&dsp_state.msg_tasklet,
		     msg_now, (unsigned long)&dsp_state);

	return 0;
}

int dsp_state_set_handler(fake_int_handler_t fake_int_handler)
{
	dsp_state.fake_int_handler = fake_int_handler;
	return 0;
}

int dsp_state_cleanup( void )
{
	del_timer_sync(&dsp_state.timer);
	tasklet_kill(&dsp_state.msg_tasklet);

	return 0;
}


/* DSP message handling.
 *
 * Messages are passed to the driver by msg_now.  They are added to a
 * message cue by the function dsp_state_function, which accepts a
 * pointer to a dsp_message (whose data is copied into a driver
 * buffer) and a 'schedule' argument.  If schedule is 0, the dsp
 * message is passed to dsp_driver immediately; this call is suitable
 * for messages arriving in interrupt context (i.e. the NFY from
 * fake_mce).  If schedule is non-zero; a tasklet is scheduled, which
 * will call msg_now so that it looks more like an interrupt.
 */

void trigger_message_task( void )
{
	tasklet_schedule(&dsp_state.msg_tasklet);
}


#define SUBNAME "fake_dsp: msg_now: "

void msg_now(unsigned long data)
{
	dsp_state_t* d = (dsp_state_t*)data;

	PRINT_INFO(SUBNAME "entry\n");

	if (d==NULL) {
		PRINT_ERR(SUBNAME "null data!\n");
		return;
	}

	if (d->msg_r == d->msg_w) {
		PRINT_ERR(SUBNAME "no replies in queue\n");
		return;
	}
	
	d->fake_int_handler( d->msg_q + d->msg_r );

	d->msg_r = (d->msg_r + 1) % CUE;

	//Send mce_command after DSP message interrupt

	if (d->mce_con != NULL) {
		PRINT_INFO("mce_fake_command now %lx\n",
			  (unsigned long)d->mce_con);
		mce_fake_command(d->mce_con);
		d->mce_con = NULL;
	}
	
	//Reschedule if there are more messages...
	if ( d->msg_r != d->msg_w) {
		trigger_message_task();
	}
}

#undef SUBNAME


#define SUBNAME "dsp_state_message: "

int dsp_state_message(dsp_message *msg, int schedule)
{
	PRINT_INFO(SUBNAME "add message to queue\n");

	if (dsp_state.trigger_timeout) {
		dsp_state.trigger_timeout = 0;
		return 0;
	}

	if (msg==NULL) {
		PRINT_ERR(SUBNAME "NULL message!\n");
		return -1;
	}

	if ((dsp_state.msg_w - dsp_state.msg_r + 1) % CUE == 0) {
		PRINT_ERR(SUBNAME "queue full!\n");
		return -1;
	}

	memcpy(dsp_state.msg_q + dsp_state.msg_w, msg, sizeof(*msg));
	dsp_state.msg_w = (dsp_state.msg_w + 1) % CUE;	

	if (schedule)
		trigger_message_task();
	else
		msg_now((unsigned long) &dsp_state);

	return 0;
}

#undef SUBNAME

/* DSP COMMAND HANDLERS */

int dsp_writemem(dsp_command *cmd, dsp_message *msg)
{
	int addr = cmd->args[1];
	int val = cmd->args[2] & DSP_DATAMASK;

	switch (cmd->args[0]) {
	case DSP_MEMP:
		if (addr<0 || addr>=PSIZE) msg->reply = DSP_ERR;
		else dsp_state.p[addr] = val;
		break;

	case DSP_MEMX:
		if (addr<0 || addr>=XSIZE) msg->reply = DSP_ERR;
		else dsp_state.x[addr] = val;
		break;

	case DSP_MEMY:
		if (addr<0 || addr>=YSIZE) msg->reply = DSP_ERR;
		else dsp_state.y[addr] = val;
		break;
	default:
		msg->reply = DSP_ERR;
	}

	return (msg->reply==DSP_ACK) ? 0 : 1;
}


int dsp_readmem(dsp_command *cmd, dsp_message *msg)
{
	int addr = cmd->args[1];
	int val = 0;

	switch (cmd->args[0]) {
	case DSP_MEMP:
		if (addr<0 || addr>=PSIZE) msg->reply = DSP_ERR;
		else val = dsp_state.p[addr];
		break;

	case DSP_MEMX:
		if (addr<0 || addr>=XSIZE) msg->reply = DSP_ERR;
		else val = dsp_state.x[addr];
		break;

	case DSP_MEMY:
		// Special handling!  Read from Y[YSIZE] will time-out.
		if (addr==YSIZE)
			dsp_state.trigger_timeout = 1;

		if (addr<0 || addr>=YSIZE) msg->reply = DSP_ERR;
		else val = dsp_state.y[addr];
		break;
	default:
		msg->reply = DSP_ERR;
	}

	msg->data = val;
	return (msg->reply==DSP_ACK) ? 0 : 1;
}


int dsp_qt_command(dsp_command *cmd, dsp_message *msg)
{
	int arg1 = cmd->args[1];
	int val = 0;

	switch (cmd->args[0]) {
	case DSP_QT_ENABLE:
		dsp_state.qt_data.enabled = arg1;
		break;

	case DSP_QT_DELTA:
		dsp_state.qt_data.delta = arg1;
		break;

	case DSP_QT_NUMBER:
		dsp_state.qt_data.number = arg1;
		break;

	case DSP_QT_INFORM:
		dsp_state.qt_data.inform = arg1;
		break;

	case DSP_QT_PERIOD:
		dsp_state.qt_data.period = arg1;
		break;

	case DSP_QT_SIZE:
		dsp_state.qt_data.size = arg1;
		break;

	case DSP_QT_TAIL:
		dsp_state.qt_data.tail = arg1;
		break;

	case DSP_QT_HEAD:
		dsp_state.qt_data.head = arg1;
		break;

	case DSP_QT_DROPS:
		dsp_state.qt_data.drops = arg1;
		break;

	case DSP_QT_BASE:
		dsp_state.qt_data.base =
			bus_to_virt(arg1 | (cmd->args[2] << 16));
		break;

	default:
		msg->reply = DSP_ERR;
	}

	msg->data = val;
	return (msg->reply==DSP_ACK) ? 0 : 1;
}


int dsp_fake_version(dsp_command *cmd, dsp_message *msg)
{
	// Old firmware replies with error.
	if (FAKE_DSP_VERSION < 0x550103) {
		msg->reply = DSP_ERR;
		return 1;
	}

	msg->data = FAKE_DSP_VERSION;
	return 0;
}


int dsp_con(dsp_command *cmd, dsp_message *msg)
{
	int busaddr = (cmd->args[0] << 16) | cmd->args[1];
	void *addr = bus_to_virt(busaddr);

	PRINT_INFO("dsp_fake_con: bus=%lx virt=%lx\n", (long unsigned) busaddr,
		  (unsigned long)addr);

	dsp_state.mce_con = addr;

	return 0;
}


int dsp_hst(dsp_command *cmd, dsp_message *msg)
{
	int busaddr = (cmd->args[0] << 16) | cmd->args[1];
	void *addr = bus_to_virt(busaddr);
	PRINT_INFO("dsp_hst: HOST to virt %lx\n",
		  (unsigned long) addr);
	mce_fake_host(addr);
	return 0;
}


int dsp_rst(dsp_command *cmd, dsp_message *msg)
{
	dsp_state.x[0] = 0x100;
	return 0;
}


int dsp_rco(dsp_command *cmd, dsp_message *msg)
{
	return mce_fake_reset();
}


int dsp_state_command(dsp_command *cmd)
{
	dsp_message msg = {
		.type = DSP_REP,
		.command = cmd->command,
		.reply = DSP_ACK,
		.data = 0,
	};

	PRINT_INFO("dsp_state_command [%06x].\n", cmd->command);

	switch(cmd->command) {

	case DSP_RDM:
		dsp_readmem(cmd, &msg);
		break;

	case DSP_WRM:
		dsp_writemem(cmd, &msg);
		break;

	case DSP_CON:
		dsp_con(cmd, &msg);
		break;

	case DSP_HST:
		dsp_hst(cmd, &msg);
		break;

	case DSP_RST:
		dsp_rst(cmd, &msg);
		break;

	case DSP_RCO:
		dsp_rco(cmd, &msg);
		break;

	case DSP_VER:
		dsp_fake_version(cmd, &msg);
		break;

	case DSP_QTS:
		dsp_qt_command(cmd, &msg);
		break;

	default:
		msg.reply = DSP_ERR;
	}

	return dsp_state_message(&msg, 1);
}

/* dsp_state_nfy_* : called by mce_fake to queue a DSP interrupt for
 * data or reply packet */

int dsp_state_nfy_da(int size)
{
	dsp_notification msg = {
		.type = DSP_NFY,
		.code = DSP_DA,
		.size_lo =  size        & 0xffff,
		.size_hi = (size >> 16) & 0xffff,
	};
	
	dsp_state_message((dsp_message*) &msg, 1);

	PRINT_INFO("nfy rp: size=%i\n", size);
	return 0;
}

int dsp_state_nfy_rp(int size)
{
	dsp_notification msg = {
		.type = DSP_NFY,
		.code = DSP_RP,
		.size_lo =  size        & 0xffff,
		.size_hi = (size >> 16) & 0xffff,
	};
	
	dsp_state_message((dsp_message*) &msg, 1);

	PRINT_INFO("nfy rp: size=%i\n", size);

	return 0;
}


/* QT handling - mce_fake calls us back when a go is received and
   tells us the dirt on what the interrupt rate will be.  We then
   generate the correct rate of interrupts, calling mce back for the
   relevant number of frames at each step. */

int dsp_retdat_callback(int frame_size, int ticks, int nframes)
{
	PRINT_ERR("dsp_retdat_callback: ting-a-ling! %i %i %i\n", frame_size, ticks, nframes);

	// If this is a stop, kill the timer
	if (frame_size < 0) {
		del_timer_sync(&dsp_state.timer);
		dsp_state.n_frames = 0;
	} else {
		dsp_state.n_frames = nframes;
		dsp_state.delta_frames = dsp_state.qt_data.period / ticks;
		dsp_state.delta_jiffies = dsp_state.qt_data.period * HZ / MCE_CLOCK;
		if (dsp_state.delta_jiffies <= 0)
			dsp_state.delta_jiffies = 1;
		PRINT_ERR("dj = %i df = %i\n", dsp_state.delta_jiffies, dsp_state.delta_frames);
		dsp_state.timer.expires = jiffies + dsp_state.delta_jiffies;
		add_timer(&dsp_state.timer);
	}
	
	return 0;
}


void dsp_timer_function(unsigned long arg)
{
	dsp_state_t *state = (dsp_state_t*)arg;
	dsp_qtinform qti;
	int new_frames = state->delta_frames;
	if (new_frames > state->n_frames) new_frames = state->n_frames;

	state->n_frames -= new_frames;
	PRINT_ERR("blip! %i\n", state->n_frames);

	PRINT_ERR("qt: %i %i %i\n", dsp_state.qt_data.size,
		  dsp_state.qt_data.inform,  dsp_state.qt_data.size);

	for (; new_frames > 0; new_frames--) {
		if ((state->qt_data.head + 1) % state->qt_data.number == state->qt_data.tail) {
			state->qt_data.drops++;
			fake_data_fill(NULL);			
		} else {
			fake_data_fill((u32*)(state->qt_data.base + 
					      state->qt_data.head*state->qt_data.delta));
			state->qt_data.head = (state->qt_data.head + 1) % 
				state->qt_data.number;
		}
	}
	
	qti.type = DSP_QTI;
	qti.dsp_head = state->qt_data.head;
	qti.dsp_tail = state->qt_data.tail;
	qti.dsp_drops = state->qt_data.drops;
	
	dsp_int_handler((dsp_message*)&qti);

	if (state->n_frames > 0) {
		int delta = state->delta_jiffies;
		if (state->n_frames < state->qt_data.inform) {
			delta = delta * state->qt_data.inform / state->n_frames;
		}
		if (delta <= 0) delta = 1;
		state->timer.expires = jiffies + delta;
		add_timer(&state->timer);
	}
}
