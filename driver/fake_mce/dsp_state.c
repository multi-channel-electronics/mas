#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <mce_options.h>

#include "mce_fake.h"
#include "dsp_state.h"


dsp_state_t dsp_state;

/* Prototypes */

void msg_now(unsigned long data);

/* Initialization */

int dsp_state_init()
{
	dsp_state.mce_con = NULL;

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
		if (addr<0 || addr>=YSIZE) msg->reply = DSP_ERR;
		else val = dsp_state.y[addr];
		break;
	default:
		msg->reply = DSP_ERR;
	}

	msg->data = val;
	return (msg->reply==DSP_ACK) ? 0 : 1;
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
