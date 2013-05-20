/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
/*
  dsp_driver.c

  Contains all PCI related code, including register definitions and
  lowest level i/o routines.
*/

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/sched.h>

#include "mce_options.h"
#include "kversion.h"
#include "version.h"

#ifdef REALTIME
#include <rtai.h>
#endif

#ifndef OLD_KERNEL
#  include <linux/dma-mapping.h>
/* Newer 2.6 kernels use IRQF_SHARED instead of SA_SHIRQ */
#  ifdef IRQF_SHARED
#    define IRQ_FLAGS IRQF_SHARED
#  else
#    define IRQ_FLAGS SA_SHIRQ
#  endif
#else
#  define IRQ_FLAGS 0
#endif

#include "dsp_driver.h"
#include "mce/dsp_ioctl.h"

/*
 *  dsp_driver.c includes
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include "mce_driver.h"
#include "dsp_ops.h"
#include "proc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Matthew Hasselfield"); 


/*
  Each command is mapped to a particular DSP host control vector
  address.  Those can be firmware specific, but aren't so far.
*/

enum dsp_vector_type {
	VECTOR_STANDARD, // 4 words into htxr
	VECTOR_QUICK,    // no htxr, no reply expected
};

struct dsp_vector {
	u32 key;
	u32 vector;
	enum dsp_vector_type type;
};

#define NUM_DSP_CMD 17

static struct dsp_vector dsp_vector_set[NUM_DSP_CMD] = {
	{DSP_WRM, 0x0078, VECTOR_STANDARD},
	{DSP_RDM, 0x007A, VECTOR_STANDARD},
	{DSP_VER, 0x007A, VECTOR_STANDARD},
	{DSP_GOA, 0x007C, VECTOR_STANDARD},
	{DSP_STP, 0x007E, VECTOR_STANDARD},
	{DSP_RST, 0x0080, VECTOR_STANDARD},
	{DSP_CON, 0x0082, VECTOR_STANDARD},
	{DSP_HST, 0x0084, VECTOR_STANDARD},
	{DSP_RCO, 0x0086, VECTOR_STANDARD},
	{DSP_QTS, 0x0088, VECTOR_STANDARD},
	{DSP_INT_RST, HCVR_INT_RST, VECTOR_QUICK},
	{DSP_INT_DON, HCVR_INT_DON, VECTOR_QUICK},
	{DSP_INT_RPC, HCVR_INT_RPC, VECTOR_QUICK},
	{DSP_SYS_ERR, HCVR_SYS_ERR, VECTOR_QUICK},
	{DSP_SYS_RST, HCVR_SYS_RST, VECTOR_QUICK},
};


typedef struct {

	u32 code;
	dsp_handler handler;
	unsigned long data;

} dsp_handler_entry;

#define MAX_HANDLERS 16

/* Here's the deal with the states.  When a DSP command is issued,
 * IDLE -> CMD.  At this time the commander may request that RESERVE
 * be set.  The CMD bit is cleared once the DSP reply is received (or
 * times out).  The RESERVE bit must be cleared by whomever set it (or
 * someone else).  When either of CMD or RESERVE are set, no DSP
 * commands can be issued and send_command will return -EAGAIN.
 *
 * High priority tasks include the clearing of the quiet-RP buffer
 * (RPC) and the sending of frame buffer updates to the DSP (INFORM).
 * When these flags (PRIORITY) are set, the driver sets a tasklet to
 * accomplish them and in the meantime blocks any other DSP commands.
 *
 */

typedef enum {
	/* These are bit flags.  Except IDLE.  It's nothin'. */
	DDAT_IDLE     = 0x00,
	DDAT_CMD      = 0x01, /* Command in progress; awaiting reply */
	DDAT_RESERVE  = 0x02, /* Commander is reserved by some agent */
	DDAT_INFORM   = 0x10, /* QT system wants to write grant to DSP */
	DDAT_RPC      = 0x20, /* RP flag needs to be cleared ASAP */
	DDAT_DSP_INT  = 0x40, /* Oustanding internal DSP command, e.g. QTS. */
        /* Masks */
	DDAT_BUSY     = 0x0F, /* Commander unavailable, for anything. */
	DDAT_PRIORITY = 0xF0, /* Outstanding priority tasks */
} dsp_state_t;


/* This structure helps provide a blocking commander that can service
   other driver levels. */

struct dsp_local {
	wait_queue_head_t queue;
	dsp_command *cmd;
	dsp_message *msg;
	int flags;
#define   LOCAL_CMD 0x01
#define   LOCAL_REP 0x02
#define   LOCAL_ERR 0x08
};


/* Mode bits in DSP firmware (U0105+) - in particular, we want to set
 * up NOIRQ and HANDSHAKE before issuing any DSP commands that will
 * interrupt and reply.  */

#define DSP_MODE_APP          0x0001
#define DSP_MODE_MCE          0x0002
#define DSP_MODE_QUIETDA      0x0004
#define DSP_MODE_QUIETRP      0x0008

struct dsp_dev_t {

	int enabled;
        int card;

	struct pci_dev *pci;
	dsp_reg_t *dsp;
	int hcvr_bits;

	int comm_mode;
	irq_handler_t int_handler;

 	struct tasklet_struct handshake_tasklet;
	struct timer_list tim_poll;

	struct dsp_local local;

	struct timer_list tim_dsp;

	spinlock_t lock;
	int cmd_count;
	int rep_count;

        volatile dsp_state_t state;
	int version;
	char version_string[32];

	int n_handlers;
	dsp_handler_entry handlers[MAX_HANDLERS];

	dsp_command last_command;
	dsp_callback callback;

	/* Priority stuff */
	int inform_tail;
 	struct tasklet_struct priority_tasklet;

} dsp_dev[MAX_CARDS];


/* Internal prototypes */

void  dsp_driver_remove(struct pci_dev *pci);

int   dsp_driver_probe(struct pci_dev *pci, const struct pci_device_id *id);

static int dsp_quick_command(struct dsp_dev_t *dev, u32 vector);

/* Data for PCI enumeration */

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(DSP_VENDORID, DSP_DEVICEID) },
	{ 0 },
};

static struct pci_driver pci_driver = {
	.name = DEVICE_NAME,
	.id_table = pci_ids,
	.probe = dsp_driver_probe,
	.remove = dsp_driver_remove,
};


/* DSP register wrappers */

static inline volatile int dsp_read_hrxs(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->htxr_hrxs));
}

static inline int dsp_read_hstr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hstr));
}

static inline int dsp_read_hcvr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hcvr));
}

static inline int dsp_read_hctr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hctr));
}

static inline void dsp_write_htxr(dsp_reg_t *dsp, u32 value) {
	iowrite32(value, (void*)&(dsp->htxr_hrxs));
}

static inline void dsp_write_hcvr(dsp_reg_t *dsp, u32 value) {
	iowrite32(value, (void*)&(dsp->hcvr));
}

static inline void dsp_write_hctr(dsp_reg_t *dsp, u32 value) {
	iowrite32(value, (void*)&(dsp->hctr));
}


#define DDAT_LOCK    spin_lock_irqsave(&dev->lock, irqflags)
#define DDAT_UNLOCK  spin_unlock_irqrestore(&dev->lock, irqflags)

void dsp_ack_int_or_schedule(unsigned long data)
{
	struct dsp_dev_t *dev = (struct dsp_dev_t*)data;
	/* Check that DSP has dropped HF3 */
	if (dsp_read_hstr(dev->dsp) & HSTR_HC3) {
                PRINT_ERR(dev->card, "Rescheduling int ack.");
		tasklet_schedule(&dev->handshake_tasklet);
	} else {
		dsp_write_hctr(dev->dsp, dev->comm_mode);
	}
}


irqreturn_t pci_int_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Note that the regs argument is deprecated in newer kernels,
	   do not use it.  It is left here for compatibility with
	   -2.6.18                                                    */
        return IRQ_NONE;

}


/* 
 * This will handle REP interrupts from the DSP, which correspond to
 * replies to DSP commands.
 */
int dsp_reply_handler(dsp_message *msg, unsigned long data)
{
	unsigned long irqflags;
  	struct dsp_dev_t *dev = (struct dsp_dev_t *)data;
	dsp_callback callback = NULL;
	int complain = 0;

	DDAT_LOCK;
	if (dev->state & DDAT_CMD) {
                PRINT_INFO(dev->card, "REP received, calling back.\n");
		// Store a copy of the callback address before going to IDLE
		callback = dev->callback;
	} else {
                PRINT_ERR(dev->card,
                                "unexpected REP received [state=%i, %i %i].\n",
                                dev->state, dev->cmd_count, dev->rep_count);
	}
	DDAT_UNLOCK;

	/* This is probably the best place to check packet consistency. */
	if (msg->command != dev->last_command.command) {
                PRINT_ERR(dev->card, "reply does not match command.\n");
		complain = 1;
	}
	if (msg->reply != DSP_ACK) {
                PRINT_ERR(dev->card, "reply was not ACK.\n");
		complain = 1;
	}
	if (complain) {
		dsp_command *cmd = &dev->last_command;
                PRINT_ERR(dev->card, "command %#06x %#06x %#06x %#06x\n",
			   cmd->command, cmd->args[0], cmd->args[1], cmd->args[2]);
                PRINT_ERR(dev->card, "reply   %#06x %#06x %#06x %#06x\n",
			   msg->type, msg->command, msg->reply, msg->data);
	}
		
	if (callback != NULL) {
		int card = dev - dsp_dev;
		callback(0, msg, card);
	}

	DDAT_LOCK;
	dev->state &= ~DDAT_CMD;
	dev->rep_count++;
	DDAT_UNLOCK;
	return 0;
}

/* This will handle HEY interrupts from the DSP, which are generic
 * communications from the DSP typically used for debugging.
 */
int dsp_hey_handler(dsp_message *msg, unsigned long data)
{
        PRINT_ERR(((struct dsp_dev_t *)data)->card,
                  "dsp HEY received: %06x %06x %06x\n",
		  msg->command, msg->reply, msg->data);
	return 0;
}


void dsp_timeout(unsigned long data)
{
	unsigned long irqflags;
	struct dsp_dev_t *dev = (struct dsp_dev_t*)data;

	DDAT_LOCK;
	if (dev->state & DDAT_CMD) {
		dev->state &= ~DDAT_CMD;
		DDAT_UNLOCK;

                PRINT_ERR(dev->card, "dsp reply timed out!\n");
		if (dev->callback != NULL) {
			int card = dev - dsp_dev;
			dev->callback(-DSP_ERR_TIMEOUT, NULL, card);
		}
	} else {
		DDAT_UNLOCK;
                PRINT_INFO(dev->card, "timer ignored\n");
	}
}


/*
  DSP command sending framework

  Modules issue dsp commands and register a callback function with

  dsp_send_command_now

  This looks up the vector address by calling dsp_lookup_vector,
  registers the callback in the device structure, and issues the
  command to the correct vector by calling
  dsp_send_command_now_vector.
*/
static int dsp_quick_command(struct dsp_dev_t *dev, u32 vector)
{
        PRINT_INFO(dev->card, "sending vector %#x\n", vector);
	if (dsp_read_hcvr(dev->dsp) & HCVR_HC) {
                PRINT_ERR(dev->card, "HCVR blocking\n");
		return -EAGAIN;
	}
	dsp_write_hcvr(dev->dsp, vector | dev->hcvr_bits | HCVR_HC);
	return 0;
}


struct dsp_vector *dsp_lookup_vector(dsp_command *cmd, int card)
{
	int i;
	for (i = 0; i < NUM_DSP_CMD; i++)
		if (dsp_vector_set[i].key == cmd->command)
			return dsp_vector_set+i;
	
        PRINT_ERR(card, "could not identify command %#x\n", cmd->command);
	return NULL;
}


int dsp_send_command_now_vector(struct dsp_dev_t *dev, u32 vector,
                dsp_command *cmd)
{
	int i = 0;
	int n = sizeof(dsp_command) / sizeof(u32);
        int s = 0;

	// DSP may block while HCVR interrupts in some cases.
	if (dsp_read_hcvr(dev->dsp) & HCVR_HC) {
                PRINT_ERR(dev->card, "HCVR blocking\n");
		return -EAGAIN;
	}

	// HSTR must be ready to receive
	if ( !((s=dsp_read_hstr(dev->dsp)) & HSTR_TRDY) ) {
                /* DSP56301 errata ED46: such a read may sometimes return
                 * the value of HCVR instead of HSTR... hopefully this will
                 * help us catch that. */
                PRINT_ERR(dev->card, "HSTR not ready to transmit (%#x)!\n", s);
		return -EIO;
	}

	//Write bytes and interrupt
	while ( i<n && ((s=dsp_read_hstr(dev->dsp)) & HSTR_HTRQ)!=0 )
		dsp_write_htxr(dev->dsp, ((u32*)cmd)[i++]);

	if (i<n) {
                PRINT_ERR(dev->card, "HTXR filled up during write! HSTR=%#x\n", s);
		return -EIO;
	}

	dsp_write_hcvr(dev->dsp, vector | dev->hcvr_bits | HCVR_HC);
	return 0;
}

int dsp_send_command_now(struct dsp_dev_t *dev, dsp_command *cmd) 
{
        struct dsp_vector *vect = dsp_lookup_vector(cmd, dev->card);

        PRINT_INFO(dev->card, "cmd=%06x\n", cmd->command);

	if (vect==NULL) return -ERESTARTSYS;

	switch (vect->type) {
	case VECTOR_STANDARD:
		memcpy(&dev->last_command, cmd, sizeof(*cmd));
		return dsp_send_command_now_vector(dev, vect->vector, cmd);
	case VECTOR_QUICK:
		// FIXME: these don't reply so they'll always time out.
		return dsp_quick_command(dev, vect->vector);
	}

        PRINT_ERR(dev->card,
		  "unimplemented vector command type %06x for command %06x\n",
		  vect->type, cmd->command);
	return -ERESTARTSYS;
}


/*
  DSP command sending framework

  - The most straight-forward approach to sending DSP commands is to
    call dsp_command with pointers to the full dsp_command structure
    and an empty dsp_message structure.

  - A non-blocking version of the above that allows the caller to
    specify a callback routine that will be run upon receipt of the
    DSP ack/err interrupt message is exposed as dsp_command_nonblock.
    The calling module must notify the dsp module that the message has
    been processed by calling dsp_clear_commflags; this cannot be done
    from within the callback routine!

  - The raw, no-semaphore-obtaining, no-flag-checking-or-setting,
    non-invalid-command-rejecting command issuer is dsp_command_now.
    Don't use it.  Only I'm allowed to use it.

*/


/* For better or worse, this routine returns linux error codes.
   
   The callback error codes are 0 (success, msg will be non-null) or
   DSP_ERR_TIMEOUT (failure, msg will be null).

   The "reserve" option can be a combination of:
      DSP_REQ_RESERVE - execute the command (if possible) and also
                        reserve the DSP (while awaiting an MCE reply,
                        for example).
      DSP_REQ_PRIORITY - execute as high-priority (should only be used
                         by the priority tasklet).
*/

int dsp_send_command(dsp_command *cmd, dsp_callback callback, int card,
		     dsp_request_t reserve)
{
	struct dsp_dev_t *dev = dsp_dev + card;
	unsigned long irqflags;
	int err = 0;

	DDAT_LOCK;
	
	// Locked due to command, reservation, or pending priority command?
	if ((dev->state & DDAT_BUSY) ||
	    ((dev->state & DDAT_PRIORITY) && (reserve != DSP_REQ_PRIORITY))
		) {
		DDAT_UNLOCK;
		return -EAGAIN;
	}
	
        PRINT_INFO(card, "send %i\n", dev->cmd_count+1);
	if ((err = dsp_send_command_now(dev, cmd)) == 0) {
		dev->state |= DDAT_CMD;
		// Commander reserves DSP?
		if (reserve == DSP_REQ_RESERVE)
			dev->state |= DDAT_RESERVE;
		dev->cmd_count++;
		dev->callback = callback;
		mod_timer(&dev->tim_dsp, jiffies + DSP_DEFAULT_TIMEOUT);
	}

        PRINT_INFO(card, "returning [%i]\n", err);
	DDAT_UNLOCK;
	return err;
}


void dsp_unreserve(int card)
{
	unsigned long irqflags;
	struct dsp_dev_t *dev = dsp_dev + card;
	DDAT_LOCK;
	dev->state &= ~DDAT_RESERVE;
	DDAT_UNLOCK;
}


int dsp_send_command_wait_callback(int error, dsp_message *msg, int card)
{
	unsigned long irqflags;
	struct dsp_dev_t *dev = dsp_dev + card;

	if (dev->local.flags != LOCAL_CMD) {
                PRINT_ERR(card, "unexpected flags, cmd=%x rep=%x err=%x\n",
			  dev->local.flags & LOCAL_CMD,
			  dev->local.flags & LOCAL_REP,
			  dev->local.flags & LOCAL_ERR);
		return -1;
	}
	memcpy(dev->local.msg, msg, sizeof(*dev->local.msg));
	DDAT_LOCK;
	dev->local.flags |= LOCAL_REP;
	wake_up_interruptible(&dev->local.queue);
	DDAT_UNLOCK;

	return 0;
}


int dsp_send_command_wait(dsp_command *cmd,
			  dsp_message *msg, int card)
{
	unsigned long irqflags;
	struct dsp_dev_t *dev = dsp_dev + card;
	int err = 0;

        PRINT_INFO(card, "entry\n");

	DDAT_LOCK;
	// Loop until we can write in our message
	while (dev->local.flags != 0) {
		DDAT_UNLOCK;
		if (wait_event_interruptible(dev->local.queue,
					     dev->local.flags == 0)) {
			return -ERESTARTSYS;
		}
		DDAT_LOCK;
	}
	if (!(dev->state & DDAT_PRIORITY))
		tasklet_schedule(&dev->priority_tasklet);
	dev->state |= DDAT_DSP_INT;
	dev->local.cmd = cmd;
	dev->local.msg = msg;
	dev->local.flags = LOCAL_CMD;
	DDAT_UNLOCK;

        PRINT_INFO(card, "commanded, waiting\n");
	if (wait_event_interruptible(dev->local.queue,
				     dev->local.flags != LOCAL_CMD)) {
		dev->local.flags = 0;
		return -ERESTARTSYS;
	}
	
	// Get spin lock, clear flags, wake up sleepers again.
	DDAT_LOCK;
	err = (dev->local.flags & LOCAL_ERR) ? -EIO : 0;
	dev->local.flags = 0;
	wake_up_interruptible(&dev->local.queue);
	DDAT_UNLOCK;

        PRINT_INFO(card, "returning %x\n", err);
	return err;
}

int dsp_grant_now_callback( int error, dsp_message *msg, int card)
{
	if (error != 0 || msg==NULL) {
                PRINT_ERR(card, "error or NULL message.\n");
	}
	return 0;		
}

int dsp_clear_RP_now(struct dsp_dev_t *dev)
{
	unsigned long irqflags;
 	dsp_command cmd = { DSP_INT_RPC, {0, 0, 0} };
	int err;
	DDAT_LOCK;
	if  (dev->state & DDAT_CMD)
		err = -EAGAIN;
	else
		err = dsp_send_command_now(dev, &cmd);
	DDAT_UNLOCK;
	return err;
}


/* This function runs from a tasklet and is responsible for running
 * top-priority DSP actions, such as clearing the RPQ buffer and
 * writing QT inform packets to the DSP.  As long as priority events
 * are outstanding, no other commands can be processed.  */

void dsp_priority_task(unsigned long data)
{
	int card = (int)data;
	struct dsp_dev_t *dev = dsp_dev+card;
	unsigned long irqflags;
	int err = 0;
	int success_mask = ~0;
	if (dev->state & DDAT_RPC) {
		err = dsp_clear_RP_now(dev);
		success_mask = ~DDAT_RPC;
	} else if (dev->state & DDAT_DSP_INT) {
		err = dsp_send_command(dev->local.cmd, &dsp_send_command_wait_callback,
				       card, DSP_REQ_PRIORITY);
		success_mask = ~DDAT_DSP_INT;
	} else if (dev->state & DDAT_INFORM) {
		dsp_command cmd = { DSP_QTS, { DSP_QT_TAIL, dev->inform_tail, 0 } };
		err = dsp_send_command(&cmd, dsp_grant_now_callback, card, DSP_REQ_PRIORITY);
		success_mask = ~DDAT_INFORM;
	} else {
                PRINT_ERR(card, "extra schedule.\n");
		return;
	}

	DDAT_LOCK;
	// Only retry on -EAGAIN, which is not considered to be an error.
	if (err == -EAGAIN) {
		err = 0;
	} else {
		dev->state &= success_mask;
		err = 0;
	}
	// Reschedule if there are any priority tasks
	if (dev->state & DDAT_PRIORITY)
		tasklet_schedule(&dev->priority_tasklet);
	DDAT_UNLOCK;

	if (err != 0)
                PRINT_ERR(card, "weird error calling dsp_grant_now (task %i)\n",
                          ~success_mask);
}


int dsp_request_grant(int card, int new_tail)
{
	struct dsp_dev_t *dev = dsp_dev + card;
	unsigned long irqflags;

	DDAT_LOCK;
	// Only reanimate the tasklet if it's not active.
	if (!(dev->state & DDAT_PRIORITY))
		tasklet_schedule(&dev->priority_tasklet);
	dev->state |= DDAT_INFORM;
	dev->inform_tail = new_tail;
	DDAT_UNLOCK;

	return 0;
}

/* dsp_clear_RP will set the priority flag so that RP is cleared at
 * the earliest opportunity.
 */

void dsp_request_clear_RP(int card)
{
	unsigned long irqflags;
	struct dsp_dev_t *dev = dsp_dev + card;
	DDAT_LOCK;
	if (!(dev->state & DDAT_PRIORITY))
		tasklet_schedule(&dev->priority_tasklet);
	dev->state |= DDAT_RPC;
	DDAT_UNLOCK;
}

/*
 *  Initialization and clean-up
 */


int dsp_query_version(int card)
{
  	struct dsp_dev_t *dev = dsp_dev + card;
	int err = 0;
	dsp_command cmd = { DSP_VER, {0,0,0} };
	dsp_message msg;
	strcpy(dev->version_string,  "<=U0103");
	
	dev->version = 0;
	if ( (err=dsp_send_command_wait(&cmd, &msg, card)) != 0 )
		return err;

	dev->version = DSP_U0103;

	if (msg.reply == DSP_ACK) {
	
		dev->version_string[0] = msg.data >> 16;
		sprintf(dev->version_string+1, "%02i%02i",
			(msg.data >> 8) & 0xff, msg.data & 0xff);

		dev->version = msg.data;
	}
		
        PRINT_ERR(card, " discovered PCI card DSP code version %s\n",
		  dev->version_string);
	return 0;
}

void dsp_timer_function(unsigned long data)
{
	struct dsp_dev_t *dev = (struct dsp_dev_t *)data;
        PRINT_INFO(dev->card, "entry\n");
	pci_int_handler(0, dev, NULL);
	mod_timer(&dev->tim_poll, jiffies + DSP_POLL_JIFFIES);
}


int dsp_set_latency(int card, int value)
{
	/* If argument 'value' is positive, that value is loaded into
	   the DSP as the size of PCI bursts.  If 'value' is 0 or
	   negative, the burst size is obtained from the PCI
	   configuration registers (that's the right way). */
	   
	struct dsp_dev_t *dev = dsp_dev + card;
        dsp_command cmd0 = { DSP_QTS, { DSP_QT_BURST, 0, 0 }};
	dsp_command cmd1 = { DSP_WRM, { DSP_MEMX, 0, 0 }};
	dsp_command cmd2 = { DSP_WRM, { DSP_MEMP, 0, 0 }};
	dsp_message rep;
	char c;

	// Get latency value or use value passed in
	if (((int)value) <= 0) {
		// User wants us to use the bus value -- most likely.
		c = 0;
		pci_read_config_byte(dev->pci, PCI_LATENCY_TIMER, &c);
                PRINT_INFO(card, "PCI latency is %#x\n", (int)c);
	} else {
		c = (char)value;
                PRINT_INFO(card, "obtained user latency %#x\n", (int)c);
	}
	if (c <= 0) {
                PRINT_ERR(card, "bad latency value %#x\n", (int)c);
		return -1;
	}

        /* As of U0106, this is easy */
        if (dev->version >= DSP_U0106) {
                cmd0.args[1] = (int)(c-4);
                dsp_send_command_wait(&cmd0, &rep, card);
                return (rep.reply == DSP_ACK) ? 0 : -1;
        }
			
        /* Hacks for older versions; modify temporary and
           semi-permanent storage of PCI_BURST_SIZE */
	cmd1.args[2] = (int)c;
	cmd2.args[2] = (int)c;
	switch (dev->version) {
	case DSP_U0104:
		cmd1.args[1] = 0x5b;
		cmd2.args[1] = 0x6ad;
		break;
	case DSP_U0105:
		cmd1.args[1] = 0x29;
		cmd2.args[1] = 0x6c7;
		break;
	default:
                PRINT_ERR(card, "can't set latency for DSP version %#06x\n",
			  dev->version);
		return -1;
	}
	dsp_send_command_wait(&cmd1, &rep, card);
	if (rep.reply == DSP_ACK)
		dsp_send_command_wait(&cmd2, &rep, card);
	return (rep.reply == DSP_ACK) ? 0 : -1;
}


/*
  Memory allocation

  This is lame, but DMA-able memory likes a pci device in old kernels.
*/


void* dsp_allocate_dma(ssize_t size, unsigned long* bus_addr_p)
{
#ifdef OLD_KERNEL
//FIX_ME: mce will call this with out card info currently
	return pci_alloc_consistent(dev->pci, size, bus_addr_p);
#else
	return dma_alloc_coherent(NULL, size, (dma_addr_t*)bus_addr_p,
				  GFP_KERNEL);
#endif
}

void  dsp_free_dma(void* buffer, int size, unsigned long bus_addr)
{
#ifdef OLD_KERNEL
//FIX_ME: mce will call this with out card info currently
 	  pci_free_consistent (dev->pci, size, buffer, bus_addr);
#else
	  dma_free_coherent(NULL, size, buffer, bus_addr);
#endif
}

int dsp_pci_flush()
{
	//FIX ME: no current called, needs card info
	struct dsp_dev_t *dev = dsp_dev;
	dsp_reg_t *dsp = dev->dsp;

	int count = 0, tmp;

	while ((dsp_read_hstr(dsp) & HSTR_HRRQ) && (count++ < PCI_MAX_FLUSH))
	{
		tmp = dsp_read_hrxs(dsp);
                if (count<4)
                        PRINT_INFO(dev->card, " %x", tmp);
                else if (count==4)
                        PRINT_INFO(dev->card, " ...");
	}

        PRINT_INFO(dev->card, "\n");

	if (dsp_read_hstr(dsp) & HSTR_HRRQ) {
                PRINT_ERR(dev->card, "could not empty HRXS!\n");
		return -1;
	}

	return 0;
}


int dsp_pci_remove_handler(struct dsp_dev_t *dev)
{
	struct pci_dev *pci = dev->pci;

	if (dev->int_handler==NULL) {
                PRINT_INFO(dev->card, "no handler installed\n");
		return 0;
	}

#ifdef REALTIME
	rt_disable_irq(pci->irq);
	rt_free_global_irq(pci->irq);
#else
	free_irq(pci->irq, dev);
#endif
	dev->int_handler = NULL;

	return 0;
}


int dsp_pci_set_handler(int card, irq_handler_t handler,
			char *dev_name)
{
	struct dsp_dev_t *dev = dsp_dev + card;
	struct pci_dev *pci = dev->pci;	
	int err = 0;
	int cfg_irq = 0;

	if (pci==NULL || dev==NULL) {
                PRINT_ERR(card, "Null pointers! pci=%lx dev=%lx\n",
			  (long unsigned)pci, (long unsigned)dev);
		return -ERESTARTSYS;
	}
	
	pci_read_config_byte(pci, PCI_INTERRUPT_LINE, (char*)&cfg_irq);
        PRINT_INFO(card, "pci has irq %i and config space has irq %i\n",
		   pci->irq, cfg_irq);

	// Free existing handler
	if (dev->int_handler!=NULL)
		dsp_pci_remove_handler(dev);
	
#ifdef REALTIME
        PRINT_ERR(card, "request REALTIME irq %#x\n", pci->irq);
	rt_disable_irq(pci->irq);
	err = rt_request_global_irq(pci->irq, (void*)handler);
#else
        PRINT_INFO(card, "requesting irq %#x\n", pci->irq);
	err = request_irq(pci->irq, handler, IRQ_FLAGS, dev_name, dev);
#endif

	if (err!=0) {
                PRINT_ERR(card, "irq request failed with error code %#x\n",
			  -err);
		return err;
	}

#ifdef REALTIME
	rt_startup_irq(pci->irq);
	rt_enable_irq(pci->irq);
#endif

	dev->int_handler = handler;
	return 0;
}


//Can be called in the future to clear a handler, not in use atm
int dsp_clear_handler(u32 code, int card)
{
  	struct dsp_dev_t *dev = dsp_dev + card;
	int i = 0;

        PRINT_INFO(card, "entry\n");

	for (i=0; i<dev->n_handlers; i++) {
		if (dev->handlers[i].code == code)
			break;
	}
	
	if (i>=dev->n_handlers)
		return -1;
	
	dev->n_handlers--;

	// Move entries i+1 to i.
	for ( ; i<dev->n_handlers; i++) {
		memcpy(dev->handlers + i, dev->handlers + i + 1,
		       sizeof(*dev->handlers));
	}

	// Clear the removed entry
	memset(dev->handlers + dev->n_handlers, 0, sizeof(*dev->handlers));

        PRINT_INFO(card, "ok\n");
	return 0;
}


int dsp_set_msg_handler(u32 code, dsp_handler handler, unsigned long data,
                int card)
{
	struct dsp_dev_t *dev = dsp_dev + card;
	int i;

        PRINT_INFO(card, "entry\n");

	// Replace handler if it exists
	for (i=0; i<dev->n_handlers; i++) {
		if (dev->handlers[i].code == code) {
			dev->handlers[i].handler = handler;
			dev->handlers[i].data = data;
			return 0;
		}
	}

	// Add to end of list
	if (i < MAX_HANDLERS) {
		dev->handlers[i].code = code;
		dev->handlers[i].handler = handler;
		dev->handlers[i].data = data;
	        dev->n_handlers++;
                PRINT_INFO(card, "ok\n");
		return 0;
	}

        PRINT_ERR(card, "no available handler slots\n");
	return -1;
}


/*
 *  IOCTL, for what it's worth...
 */

int dsp_driver_ioctl(unsigned int iocmd, unsigned long arg, int card)
{
  	struct dsp_dev_t *dev = dsp_dev + card;

	switch(iocmd) {

	case DSPDEV_IOCT_SPEAK:
                PRINT_IOCT(card, "state=%#x\n", dev->state);
		break;

	case DSPDEV_IOCT_CORE:
		if (dev->pci == NULL || dev->dsp == NULL) {
                        PRINT_IOCT(card, "dev->pci=%p dev->dsp=%p\n",
				   dev->pci, dev->dsp);
			return 0;
		}
                PRINT_IOCT(card, "hstr=%#06x hctr=%#06x\n",
			   dsp_read_hstr(dev->dsp), dsp_read_hctr(dev->dsp));		
		break;

	case DSPDEV_IOCT_LATENCY:
		return dsp_set_latency(card, (int)arg);

	default:
                PRINT_ERR(card, "I don't handle iocmd=%ui\n", iocmd);
		return -1;
	}

	return 0;

}


int dsp_proc(char *buf, int count, int card)
{
	struct dsp_dev_t *dev = dsp_dev + card;
	int len = 0;

        PRINT_INFO(card, "card = %d\n", card);
	if (dev->pci == NULL) 
		return len;
	if (len < count) {
		len += sprintf(buf+len, "    %-15s %25s\n",
			       "bus address:", pci_name(dev->pci));
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-15s %25s\n",
			       "interrupt:",
			       (dev->comm_mode & DSP_PCI_MODE_NOIRQ) ? "polling" : "enabled");
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-32s %#08x\n    %-32s %#08x\n"
			       "    %-32s %#08x\n",
			       "hstr:", dsp_read_hstr(dev->dsp),
			       "hctr:", dsp_read_hctr(dev->dsp),
			       "hcvr:", dsp_read_hcvr(dev->dsp));
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-20s %20s\n",
			       "firmware version:", dev->version_string);
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-30s ", "state:");
		if (dev->state & DDAT_CMD)
			len += sprintf(buf+len, "commanded");
		else
			len += sprintf(buf+len, "     idle");
		if (dev->state & DDAT_RESERVE)
			len += sprintf(buf+len, " (reserved)\n");
		else
			len += sprintf(buf+len, "\n");
	}

	return len;
}


int dsp_configure(struct pci_dev *pci)
{
	int err = 0;
	int card;
	struct dsp_dev_t *dev;

        PRINT_INFO(NOCARD, "(%p) entry\n", pci);

	// Find a free slot in dsp_dev array; this defines the card id
	if (pci==NULL) {
                PRINT_ERR(NOCARD, "Called with NULL pci_dev!\n");
		return -EPERM;
	}
	for (card=0; card<MAX_CARDS; card++) {
		dev = dsp_dev + card;
                if(dev->pci == NULL)
                        break;
	} 	
	if (dev->pci != NULL) {
                PRINT_ERR(NOCARD, "too many cards, dsp_dev[] is full.\n");
		return -EPERM;
	}
        PRINT_INFO(card, "%s: card = %i\n", __FUNCTION__, card);

        // Initialize device structure
	memset(dev, 0, sizeof(*dev));
	dev->pci = pci;
        dev->card = card;

	tasklet_init(&dev->handshake_tasklet,
		     dsp_ack_int_or_schedule, (unsigned long)dev);
	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->local.queue);

	init_timer(&dev->tim_dsp);
	dev->tim_dsp.function = dsp_timeout;
	dev->tim_dsp.data = (unsigned long)dev;
	dev->state = DDAT_IDLE;

	// Data granting task
	tasklet_init(&dev->priority_tasklet, dsp_priority_task, (unsigned long)card);

	// PCI paperwork
	err = pci_enable_device(dev->pci);
	if (err) goto fail;
	if (pci_request_regions(dev->pci, DEVICE_NAME)!=0) {
                PRINT_ERR(card, "pci_request_regions failed.\n");
		err = -1;
		goto fail;
	}
	dev->dsp = (dsp_reg_t *)ioremap_nocache(pci_resource_start(dev->pci, 0) & 
						PCI_BASE_ADDRESS_MEM_MASK,
						sizeof(*dev->dsp));
	if (dev->dsp==NULL) {
                PRINT_ERR(card, "Could not map PCI registers!\n");
		pci_release_regions(dev->pci);			
		err = -EIO;
		goto fail;
	}
	pci_set_master(dev->pci);

	/* Card configuration - now we're done with the kernel and
	 * talk to the card */

	// Until we check the card version, make all commands unmaskable
	dev->hcvr_bits = HCVR_HNMI;

	// Clear any outstanding interrupts
	dsp_quick_command(dev, HCVR_INT_RST);
	dsp_quick_command(dev, HCVR_INT_DON);

	// Set the mode of the data path for reads (24->32 conversion)
	dev->comm_mode = DSP_PCI_MODE;
#ifdef NO_INTERRUPTS
	dev->comm_mode |= DSP_PCI_MODE_NOIRQ;
#endif /* NO_INTERRUPTS */
	dsp_write_hctr(dev->dsp, dev->comm_mode);

	// Reset the card
	dsp_quick_command(dev, HCVR_SYS_RST);

	// Install interrupt handler or polling timer
	dev->int_handler = NULL;
	if (dev->comm_mode & DSP_PCI_MODE_NOIRQ) {
		// Create timer for soft poll interrupt generation
		init_timer(&dev->tim_poll);
		dev->tim_poll.function = dsp_timer_function;
		dev->tim_poll.data = (unsigned long)dev;
		mod_timer(&dev->tim_poll, jiffies + DSP_POLL_JIFFIES);
	} else {
		// Install the interrupt handler (cast necessary for backward compat.)
                PRINT_ERR(card, "interrupt handler installed.\n");
		err = dsp_pci_set_handler(card, (irq_handler_t)pci_int_handler,
					  "mce_dsp");		
		if (err) goto fail;
	}

	// Assign handlers for REP and HEY interrupts.  These are for
	// DSP communications (rather than the MCE protocol).
	dsp_set_msg_handler(DSP_REP, dsp_reply_handler, (unsigned long)dev, card);
	dsp_set_msg_handler(DSP_HEY, dsp_hey_handler, (unsigned long)dev, card);

        PRINT_INFO(card, "ok\n");
	return card;

fail:
        PRINT_ERR(card, "failed!\n");
	return err;
}


int dsp_unconfigure(int card)
{
	struct dsp_dev_t *dev = dsp_dev + card;

	// Remove int handler or poll timer
	if (dev->comm_mode & DSP_PCI_MODE_NOIRQ) {
		del_timer_sync(&dev->tim_poll);
	} else {
		dsp_pci_remove_handler(dev);
	}
		
	if (dev->dsp!=NULL) {
		// PCI un-paperwork
		iounmap(dev->dsp);
		dev->dsp = NULL;
	}

	if (dev->pci != NULL) {
		pci_disable_device(dev->pci);
		pci_release_regions(dev->pci);
		dev->pci = NULL;
	}

	return card;
}

void olddsp_driver_remove(struct pci_dev *pci)
{
	int card;
	struct dsp_dev_t *dev;

        PRINT_INFO(NOCARD, "entry\n");
	if (pci == NULL) {
                PRINT_ERR(NOCARD, "called with null pointer, ignoring.\n");
		return;
	}

	// Match to existing card
	for (card=0; card < MAX_CARDS; card++) {
		dev = dsp_dev + card;
		if (pci == dev->pci)
			break;
	}
	if (card >= MAX_CARDS) {
                PRINT_ERR(card, "could not match configured device, "
                                "ignoring.\n");
		return;
	}
			
	// Disable higher-level features first
	mce_remove(card);
	del_timer_sync(&dev->tim_dsp);

	// Hopefully these aren't still running...
	tasklet_kill(&dev->priority_tasklet);
	tasklet_kill(&dev->handshake_tasklet);

	// Revert card to default mode
	dsp_write_hctr(dev->dsp, DSP_PCI_MODE);

	// Do DSP cleanup, free PCI resources
	dsp_unconfigure(card);

        PRINT_INFO(NOCARD, "ok\n");
}


int dsp_ready(int card) {
	struct dsp_dev_t *dev = dsp_dev + card;
	return dev->enabled;
}


/*
  dsp_driver_probe

  Called by kernel's PCI manager with each PCI device.  We first find
  a place to keep the card, then do the PCI level initialization

*/

int olddsp_driver_probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	int card;
	struct dsp_dev_t *dev = NULL;
        PRINT_INFO(NOCARD, "entry\n");

	// Setup data structure for the card, configure PCI stuff and
	// the DSP.  After this call, the DSP is ready to go.
	if ((card = dsp_configure(pci)) < 0)
		goto fail;
	dev = dsp_dev + card;

	// Get DSP version, which MCE driver likes to know...
	if (dsp_query_version(card) != 0)
		goto fail;

	if (dev->version >= DSP_U0105) {
		// Enable interrupt hand-shaking
		dev->comm_mode |= DSP_PCI_MODE_HANDSHAKE;
		dsp_write_hctr(dev->dsp, dev->comm_mode);
		dev->hcvr_bits &= ~HCVR_HNMI;
	} else {
		// All vector commands must be non-maskable on older firmware
		dev->hcvr_bits |= HCVR_HNMI;
	}

	dev->enabled = 1;

	// Set the PCI latency timer
	if (dsp_set_latency(card, 0))
		goto fail;

	// Enable the character device for this card.
	if (dsp_ops_probe(card) != 0)
		goto fail;

	// DSP is ready, setup a structure for MCE driver
	if (mce_probe(card, dsp_dev[card].version))
		goto fail;

        PRINT_INFO(NOCARD, "ok\n");
	return 0;

fail:
        PRINT_ERR(NOCARD, "failed, calling removal routine.\n");
	dsp_driver_remove(pci);
	return -1;
}


void olddsp_driver_cleanup(void)
{
	int i = 0;

        PRINT_INFO(NOCARD, "entry\n");

#ifdef FAKEMCE
	dsp_driver_remove();
	dsp_fake_cleanup();
#else
	pci_unregister_driver(&pci_driver);

	for(i=0; i < MAX_CARDS; i++) {
		struct dsp_dev_t *dev = dsp_dev + i;
		if ( dev->pci != NULL ) {
                        PRINT_ERR(NOCARD, "failed to zero dev->pci for "
                                        "card %i!\n", i);
		}
	}
#endif
	dsp_ops_cleanup();

	mce_cleanup();

	remove_proc_entry("mce_dsp", NULL);

        PRINT_INFO(NOCARD, "driver removed\n");
}    

/*****

 HACKING

*****/

#include "test/dspioctl.h"

typedef enum {
        DSP_IDLE = 0,
        DSP_CMD_QUED = 1,
        DSP_CMD_SENT = 2,
        DSP_REP_RECD = 3,
        DSP_REP_HNDL = 4
} newdsp_state_t;



typedef struct {
	/* int major; */
        int minor;
        int enabled;

	struct pci_dev *pci;
	dsp_reg_t *reg;

        int reply_recd;  // state... 0 is idle, 1 is reqd, 2 is recd.
        /* struct dsp_reply reply; */
        struct dsp_datagram reply;

        __s32 *data_buffer;
        int data_head;
        int data_tail;
        int data_size;

        int reply_buffer_size;
        void* reply_buffer_dma_virt;
        dma_addr_t reply_buffer_dma_handle;

        /* void* reply_buffer; */

        spinlock_t lock;
	wait_queue_head_t queue;
        newdsp_state_t state;

	/* struct semaphore sem; */

	int error;

	/* dsp_ops_state_t state; */

	/* dsp_message msg; */
	/* dsp_command cmd; */
        
} newdsp_t;

newdsp_t dspdata[MAX_CARDS];
int major = 0;


#define DSP_LOCK_DECLARE_FLAGS unsigned long irqflags
#define DSP_LOCK    spin_lock_irqsave(&dsp->lock, irqflags)
#define DSP_UNLOCK  spin_unlock_irqrestore(&dsp->lock, irqflags)
/* #define DSP_LOCK_DECLARE_FLAGS  */
/* #define DSP_LOCK  */
/* #define DSP_UNLOCK  */

static void inline dsp_set_state_wake(newdsp_t *dsp, int new_state) {
        DSP_LOCK_DECLARE_FLAGS;
        DSP_LOCK;
        dsp->state = new_state;
        DSP_UNLOCK;
        wake_up_interruptible(&dsp->queue);
}


irqreturn_t new_int_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Note that the regs argument is deprecated in newer kernels,
	 * do not use it.  It is left here for compatibility with
	 * -2.6.18                                                    */

	newdsp_t *dsp = dev_id;
        int k;
        int hctr;
        int n_wait;
        int n_wait_max;

        // Reject null devs
        if (dsp == NULL) {
                PRINT_ERR(NOCARD, "IRQ with null device\n");
                return IRQ_NONE;
        }

        // Is this really one of ours?
	for(k=0; k<MAX_CARDS; k++) {
                if (dsp == dspdata+k)
                        break;
        }

        if (k==MAX_CARDS)
                return IRQ_NONE;

        PRINT_INFO(dsp->minor, "Entry.\n");

	if (dsp == NULL || dsp->enabled == 0) {
                PRINT_ERR(NOCARD, "IRQ on disabled device\n");
                return IRQ_NONE;
        }

	// Confirm that device has raised interrupt
        n_wait = 0;
        n_wait_max = 100000;
        do {
                if (dsp_read_hstr(dsp->reg) & HSTR_HINT)
                        break;
        } while (++n_wait < n_wait_max);
        if (n_wait >= n_wait_max) {
                PRINT_ERR(dsp->minor,
                          "HINT did not rise in %i cycles; ignoring\n",
                          n_wait);
                return IRQ_NONE;
        }
        if (n_wait != 0) {
                PRINT_ERR(dsp->minor,
                          "HINT took %i cycles to rise; accepting\n",
                          n_wait);
	}

        PRINT_INFO(dsp->minor, "IRQ owned.\n");

        // Hand shake up...
        hctr = dsp_read_hctr(dsp->reg);
        dsp_write_hctr(dsp->reg, hctr | HCTR_HF0);

        //Is this a DSP reply? Update the DSP state.
        if (1) { // Yes, it is a DSP reply.
                DSP_LOCK_DECLARE_FLAGS;
                DSP_LOCK;
                switch(dsp->state) {
                case DSP_CMD_SENT:
                        PRINT_ERR(dsp->minor, "marking RECD\n");
                        dsp->state = DSP_REP_RECD;
                        //Copy into datagram holder
                        memcpy(&dsp->reply, dsp->reply_buffer_dma_virt,
                               sizeof(dsp->reply));
                        //Wake up any blockers
                        wake_up_interruptible(&dsp->queue);
                        break;
                default:
                        PRINT_ERR(dsp->minor,
                                  "unexpected reply packet (state=%i)\n",
                                  dsp->state);
                        dsp->state = DSP_IDLE;
                }
                DSP_UNLOCK;
        }

        n_wait = 0;
        n_wait_max = 1000000;
        do {
                if (!(dsp_read_hstr(dsp->reg) & HSTR_HINT))
                        break;
        } while (++n_wait < n_wait_max);
        if (n_wait!=0) {
                PRINT_ERR(dsp->minor, "int handler timed out waiting for HINT\n");
                PRINT_ERR(dsp->minor, "hobbling int handler so you get this message\n");
                /* global_fail = 1; */
        }

        PRINT_INFO(dsp->minor, "HSTR=%x\n", dsp_read_hstr(dsp->reg));

        // Hand shake down.
        dsp_write_hctr(dsp->reg, hctr & ~HCTR_HF0);

        PRINT_INFO(dsp->minor, "ok\n");
	return IRQ_HANDLED;
}


int dsp_driver_probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	int card;
        int err = 0;
	struct dsp_dev_t *dev = NULL;
	newdsp_t *dsp = NULL;
        PRINT_INFO(NOCARD, "entry\n");

        // Find open slot
        for (card=0; card<MAX_CARDS; card++) {
                dsp = dspdata + card;
                if (dsp->pci == NULL)
                        break;
        }

        if (card==MAX_CARDS) {
                PRINT_ERR(NOCARD, "too many cards, dspdata is full.\n");
                return -ENODEV;
        }

        PRINT_INFO(NOCARD, "assigned to minor=%i\n", card);
        dsp->minor = card;

        // Kernel structures...
        init_waitqueue_head(&dsp->queue);

        // Do this on success only...
        /* dsp->pci = pci; */

        // Now we can set up the kernel to talk to the card
	// PCI paperwork
	if ((err = pci_enable_device(pci))) {
                PRINT_ERR(card, "pci_enable_device failed.\n");
                goto fail;
        }
        // This was recently moved up; used to be under ioremap
	pci_set_master(pci);

	if (pci_request_regions(pci, DEVICE_NAME)!=0) {
                PRINT_ERR(card, "pci_request_regions failed.\n");
		err = -1;
		goto fail;
	}

        dsp->reg = (dsp_reg_t *)ioremap_nocache(pci_resource_start(pci, 0) &
						PCI_BASE_ADDRESS_MEM_MASK,
						sizeof(*dsp->reg));
	if (dsp->reg==NULL) {
                PRINT_ERR(card, "Could not map PCI registers!\n");
		pci_release_regions(pci);
		err = -EIO;
		goto fail;
	}

        //Allocate a buffer...
        /* dsp->data_size = 100000; */
        /* dsp->data_buffer = kmalloc(GFP_KERNEL, */
        /*                            dsp->data_size * sizeof(*dsp->data_buffer)); */

        // Allocate DMA-ready reply buffer.
        dsp->reply_buffer_size = 1024;
        dsp->reply_buffer_dma_virt =
                dma_alloc_coherent(&pci->dev, dsp->reply_buffer_size,
                                   &dsp->reply_buffer_dma_handle,
                                   GFP_KERNEL);

        if (dsp->reply_buffer_dma_virt==NULL) {
                PRINT_ERR(card, "Failed to allocate DMA memory.\n");
        }

        /* dsp->reply_buffer = kmalloc(dsp->reply_buffer_size, GFP_KERNEL); */
        /* if (dsp->reply_buffer==NULL) { */
        /*         PRINT_ERR(card, "Failed to allocate buffer copy.\n"); */
        /* } */

        // Interrupt handler?  Maybe this can wait til later...
	err = request_irq(pci->irq, (irq_handler_t)new_int_handler,
                          IRQ_FLAGS, DEVICE_NAME, dsp);
	if (err!=0) {
                PRINT_ERR(card, "irq request failed with error code %#x\n",
			  -err);
	}
        PRINT_ERR(card, "new interrupt handler installed.\n");

        //Ok, we made it.
        dsp->pci = pci;
        dsp->enabled = 1;

        return 0;
              
	// Setup data structure for the card, configure PCI stuff and
	// the DSP.  After this call, the DSP is ready to go.
	if ((card = dsp_configure(pci)) < 0)
		goto fail;
	dev = dsp_dev + card;

	// Get DSP version, which MCE driver likes to know...
	if (dsp_query_version(card) != 0)
		goto fail;

	if (dev->version >= DSP_U0105) {
		// Enable interrupt hand-shaking
		dev->comm_mode |= DSP_PCI_MODE_HANDSHAKE;
		dsp_write_hctr(dev->dsp, dev->comm_mode);
		dev->hcvr_bits &= ~HCVR_HNMI;
	} else {
		// All vector commands must be non-maskable on older firmware
		dev->hcvr_bits |= HCVR_HNMI;
	}

	dev->enabled = 1;

	// Set the PCI latency timer
	if (dsp_set_latency(card, 0))
		goto fail;

	// Enable the character device for this card.
	if (dsp_ops_probe(card) != 0)
		goto fail;

	// DSP is ready, setup a structure for MCE driver
	if (mce_probe(card, dsp_dev[card].version))
		goto fail;

        PRINT_INFO(NOCARD, "ok\n");
	return 0;

fail:
        PRINT_ERR(NOCARD, "failed, calling removal routine.\n");
	dsp_driver_remove(pci);
	return -1;
}


void dsp_driver_remove(struct pci_dev *pci)
{
	int card;
	newdsp_t *dsp = NULL;

        PRINT_INFO(NOCARD, "entry\n");
	if (pci == NULL) {
                PRINT_ERR(NOCARD, "called with null pointer, ignoring.\n");
		return;
	}

	// Match to existing card
	for (card=0; card < MAX_CARDS; card++) {
                dsp = dspdata + card;
		if (pci == dsp->pci)
			break;
	}
	if (card >= MAX_CARDS) {
                PRINT_ERR(card, "could not match configured device, "
                                "ignoring.\n");
		return;
	}
			
        dsp->enabled = 0;

        if (dsp->pci != NULL)
                free_irq(dsp->pci->irq, dsp);

        if (dsp->reply_buffer_dma_virt != NULL)
                dma_free_coherent(&dsp->pci->dev, dsp->reply_buffer_size,
                                  dsp->reply_buffer_dma_virt,
                                  dsp->reply_buffer_dma_handle);

        /* if (dsp->reply_buffer != NULL) */
        /*         kfree(dsp->reply_buffer); */

        // Free that buffer
        /* if (dsp->data_buffer != NULL) */
        /*         kfree(dsp->data_buffer); */

        //Unmap i/o...
        iounmap(dsp->reg);

        /* dev->dsp = NULL; */
        // Call release_regions *after* disable_device.
        pci_disable_device(dsp->pci);
        pci_release_regions(dsp->pci);

        dsp->pci = NULL;

	/* // Disable higher-level features first */
	/* mce_remove(card); */
	/* del_timer_sync(&dev->tim_dsp); */

	/* // Hopefully these aren't still running... */
	/* tasklet_kill(&dev->priority_tasklet); */
	/* tasklet_kill(&dev->handshake_tasklet); */

	/* // Revert card to default mode */
	/* dsp_write_hctr(dev->dsp, DSP_PCI_MODE); */

	/* // Do DSP cleanup, free PCI resources */
	/* dsp_unconfigure(card); */

        PRINT_INFO(NOCARD, "ok\n");
}

__s32 read_pair(newdsp_t *dsp) {
        __s32 i1, i2;
        while (!(dsp_read_hstr(dsp->reg) & HSTR_HRRQ));
        i1 = dsp_read_hrxs(dsp->reg);
        while (!(dsp_read_hstr(dsp->reg) & HSTR_HRRQ));
        i2 = dsp_read_hrxs(dsp->reg);
        return (i1 & 0xffff) << 16 | (i2 & 0xffff);
}

/*
int read_hrxs_packet(newdsp_t *dsp) {
        int word, code, size;
        int i;

        // Anything there?
        if (!(dsp_read_hstr(dsp->reg) & HSTR_HRRQ))
                return -EBUSY;

        // Yes, something is there.
        word = read_pair(dsp);
        code = (word >> 16) & 0xff;
        size = (word & 0xffff);
        
        if (code < DSP_CODE_REPLY_MAX) {
                // This is a reply to a command we sent.
                if (dsp->reply_recd == 2)
                        // Bad!  The driver contains an outstanding reply.
                        PRINT_ERR(NOCARD, "Reply trashes existing reply.");
                // Fill the reply.
                dsp->reply.size = size+1;
                dsp->reply.data[0] = word;
                for (i=1; i<dsp->reply.size; i++)
                        dsp->reply.data[i] = read_pair(dsp);
                dsp->reply_recd = 2;
        } else {
                PRINT_ERR(NOCARD, "MCE packet - size %i!\n", size);
                for (i=0; i<size; i++) {
                        dsp->data_buffer[dsp->data_head++] = read_pair(dsp);
                }
                PRINT_ERR(NOCARD, "Buffer state: %i %i\n",
                          dsp->data_head, dsp->data_tail);
        }
        return 0;
}
*/

int try_send_cmd(newdsp_t *dsp, struct dsp_command *cmd) {
        int i;
        int n_wait = 0;
        int n_wait_max = 10000;

        if (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ))
                return -EBUSY;

        for (i=0; i < cmd->size; i++) {
                while (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ) && ++n_wait < n_wait_max);
                dsp_write_htxr(dsp->reg, cmd->data[i]);
                PRINT_INFO(dsp->minor, "wrote %i=%x\n", i, cmd->data[i]);
        }
        if (n_wait >= n_wait_max) {
                PRINT_ERR(dsp->minor,
                          "failed to write command while waiting for HTRQ.\n");
                return -EIO;
        }
        return 0;
}

long newdsp_ioctl(struct file *filp, unsigned int iocmd, unsigned long arg)
{
//	struct filp_pdata *fpdata = filp->private_data;
//	struct dsp_ops_t *dops = dsp_ops + fpdata->minor;
        newdsp_t *dsp = (newdsp_t*)filp->private_data;
        int card = dsp->minor;

        struct dsp_command cmd;
        int i;
        unsigned x;
        DSP_LOCK_DECLARE_FLAGS;

        int nonblock = (filp->f_flags & O_NONBLOCK);

//        PRINT_INFO(card, "entry! minor=%d\n", card);

	switch(iocmd) {
        case DSPIOCT_R_HRXS:
                return dsp_read_hrxs(dsp->reg);
        case DSPIOCT_R_HSTR:
                return dsp_read_hstr(dsp->reg);
        case DSPIOCT_R_HCVR:
                return dsp_read_hcvr(dsp->reg);
        case DSPIOCT_R_HCTR:
                return dsp_read_hctr(dsp->reg);

        case DSPIOCT_W_HTXR:
                dsp_write_htxr(dsp->reg, (int)arg);
                return 0;
        case DSPIOCT_W_HCVR:
                dsp_write_hcvr(dsp->reg, (int)arg);
                return 0;
        case DSPIOCT_W_HCTR:
                dsp_write_hctr(dsp->reg, (int)arg);
                return 0;

        case DSPIOCT_COMMAND:
                PRINT_ERR(dsp->minor, "send_command\n");
                DSP_LOCK;
                while (dsp->state != DSP_IDLE) {
                        int last_state = dsp->state;
                        DSP_UNLOCK;
                        if (nonblock)
                                return -EBUSY;
                        // Wait for state change?
                        PRINT_ERR(dsp->minor, "wait once...\n");
                        if (wait_event_interruptible(
                                    dsp->queue, (dsp->state!=last_state))!=0)
                                return -ERESTARTSYS;
                        DSP_LOCK;
                }
                dsp->state = DSP_CMD_QUED;
                DSP_UNLOCK;

                PRINT_ERR(dsp->minor, "attempt send.\n");

                // Copy command from user
                copy_from_user(&cmd, (const void __user *)arg, sizeof(cmd));

                // Clear reply state
                //dsp->reply_recd = cmd.reply_reqd;
                //FIXME: register this for later writing.
                if (try_send_cmd(dsp, &cmd)!=0) {
                        dsp_set_state_wake(dsp, DSP_IDLE);
                        return -EBUSY;
                }

                // Update state
                dsp_set_state_wake(dsp,
                                   (cmd.flags & DSP_EXPECT_DSP_REPLY) ? 
                                   DSP_CMD_SENT : DSP_IDLE);

                return 0;

        case DSPIOCT_REPLY:
                /* Check for reply.  Only block if a command has been
                 * sent and thus a reply is expected in the near
                 * future. */
                PRINT_ERR(dsp->minor, "get_reply\n");
                DSP_LOCK;
                while (dsp->state != DSP_REP_RECD) {
                        int last_state = dsp->state;
                        DSP_UNLOCK;
                        if (nonblock ||
                            dsp->state == DSP_IDLE ||
                            dsp->state == DSP_REP_HNDL)
                                return -EBUSY;
                        // Wait for state change?
                        PRINT_ERR(dsp->minor, "wait once...\n");
                        if (wait_event_interruptible(
                                    dsp->queue, (dsp->state!=last_state))!=0)
                                return -ERESTARTSYS;
                        DSP_LOCK;
                }
                PRINT_ERR(dsp->minor, "process reply.\n");
                // Mark that we're handling the reply and release lock
                dsp->state = DSP_REP_HNDL;
                DSP_UNLOCK;

                copy_to_user((void __user*)arg, &dsp->reply, sizeof(dsp->reply));

                // Mark DSP as idle.
                dsp_set_state_wake(dsp, DSP_IDLE);

                return 0;

        case DSPIOCT_SET_REP_BUF:
                // Hand craftily upload the bus address.
                x = (unsigned) dsp->reply_buffer_dma_handle;
                PRINT_INFO(card, "Informing DSP of bus address=%lx\n",
                           (long)dsp->reply_buffer_dma_handle);

                cmd.data[0] = 0x090002;
                cmd.data[1] = (x) & 0xffff;
                cmd.data[2] = (x >> 16) & 0xffff;
                cmd.size = 3;
                cmd.flags = 0;
                //cmd.reply_reqd = 0;

                //FIXME: register this for later writing.
                
                if (try_send_cmd(dsp, &cmd) != 0)
                        return -EBUSY;

                return 0;

        case DSPIOCT_TRIGGER_FAKE:
                // Hand craftily upload the bus address.
                PRINT_INFO(card, "Triggering DSP write\n");

                cmd.data[0] = 0x310000;
                cmd.size = 1;
                //cmd.reply_reqd = 0;
                cmd.flags = 0;

                //FIXME: register this for later writing.
                if (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ))
                        return -EBUSY;
                
                try_send_cmd(dsp, &cmd);
                return 0;

        case DSPIOCT_DUMP_BUF:
                for (i=0; i<12; i++) {
                        if (i==2) i=8;
                        PRINT_INFO(card, "buffer %3i = %4x\n",
                                   i, (int)((__u32*)(dsp->reply_buffer_dma_virt))[i]);
                }
                return 0;

	default:
                PRINT_INFO(card, "unknown ioctl\n");
                return 0;
	}
	return 0;
}

int newdsp_mmap(struct file *filp, struct vm_area_struct *vma)
{
        newdsp_t *dsp = (newdsp_t*)filp->private_data;
        int card = dsp->minor;

	// Mark memory as reserved (prevents core dump inclusion) and
	// IO (prevents caching)
	vma->vm_flags |= VM_IO | VM_RESERVED;

	// Do args checking on vma... start, end, prot.
        PRINT_INFO(card, "mapping %#lx bytes to user address %#lx\n",
		   vma->vm_end - vma->vm_start, vma->vm_start);

	//remap_pfn_range(vma, virt, phys_page, size, vma->vm_page_prot);
	remap_pfn_range(vma, vma->vm_start,
			virt_to_phys(dsp->data_buffer) >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
	return 0;
}


int newdsp_open(struct inode *inode, struct file *filp)
{
	/* struct filp_pdata *fpdata; */
	int minor = iminor(inode);
        PRINT_INFO(minor, "entry! iminor(inode)=%d\n", minor);

        // Is this a real device?
        if (minor >= MAX_CARDS || !dspdata[minor].enabled) {
                PRINT_ERR(minor, "card %i not enabled.\n", minor);
		return -ENODEV;
	}

        // FIXME: populate .owner in fops?
	if(!try_module_get(THIS_MODULE))
		return -1;

        
	/* fpdata = kmalloc(sizeof(struct filp_pdata), GFP_KERNEL); */
        /* fpdata->minor = iminor(inode); */
	filp->private_data = dspdata + minor;

        PRINT_INFO(minor, "ok\n");
	return 0;
}

int newdsp_release(struct inode *inode, struct file *filp)
{
	/* struct filp_pdata *fpdata = filp->private_data; */
        /* int card = fpdata->minor; */
        newdsp_t *dsp = (newdsp_t*)filp->private_data;
        int card = dsp->minor;

        PRINT_INFO(card, "entry!\n");

	/* if (fpdata != NULL) { */
	/* 	kfree(fpdata); */
	/* } else */
        /*         PRINT_ERR(card, "called with NULL private_data!\n"); */

	module_put(THIS_MODULE);

        PRINT_INFO(card, "ok\n");
	return 0;
}


struct file_operations newdsp_fops = 
{
	.owner=   THIS_MODULE,
	.open=    newdsp_open,
	/* .read=    dsp_read, */
	.release= newdsp_release,
	/* .write=   dsp_write, */
	.unlocked_ioctl= newdsp_ioctl,
};



inline int dsp_driver_init(void)
{
	int i = 0;
	int err = 0;

        PRINT_ERR(NOCARD, "MAS driver version %s\n", VERSION_STRING);
	for(i=0; i<MAX_CARDS; i++) {
                newdsp_t *dev = dspdata + i;
		memset(dev, 0, sizeof(*dev));
                dev->minor = i;
                spin_lock_init(&dev->lock);
	}
  
        //	create_proc_read_entry("mce_dsp", 0, NULL, read_proc, NULL);

	err = register_chrdev(0, DEVICE_NAME, &newdsp_fops);
        major = err;

	/* err = dsp_ops_init(); */
	/* if(err != 0) goto out; */

	/* err = mce_init(); */
	/* if(err != 0) goto out; */

	err = pci_register_driver(&pci_driver);
	if (err) {
                PRINT_ERR(NOCARD, "pci_register_driver failed with code %i.\n",
                                err);
		err = -1;
		goto out;
	}			  

        PRINT_INFO(NOCARD, "ok\n");
	return 0;
out:
        PRINT_ERR(NOCARD, "exiting with error\n");
	return err;
}

void dsp_driver_cleanup(void)
{
        PRINT_INFO(NOCARD, "entry\n");

	pci_unregister_driver(&pci_driver);

        unregister_chrdev(major, DEVICE_NAME);

	/* dsp_ops_cleanup(); */

	/* mce_cleanup(); */

	/* remove_proc_entry("mce_dsp", NULL); */

        PRINT_INFO(NOCARD, "driver removed\n");
}    

module_init(dsp_driver_init);
module_exit(dsp_driver_cleanup);
