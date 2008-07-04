/*
 * dsp_driver.c
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>

#include <asm/uaccess.h>
#include <linux/interrupt.h>

#include "mce_options.h"
#include "kversion.h"

#include "dsp_driver.h"

#include "mce/dsp_ioctl.h"

#ifdef FAKEMCE
#  include <dsp_fake.h>
#else
#  include "dsp_pci.h"
#endif

#include "mce_driver.h"
#include "dsp_ops.h"
#include "proc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Matthew Hasselfield"); 


typedef struct {

	u32 code;
	dsp_handler handler;
	unsigned long data;

} dsp_handler_entry;

#define MAX_HANDLERS 16


typedef enum {

	DDAT_IDLE = 0,
	DDAT_CMD,

} dsp_state_t;


struct dsp_control {
	
	struct semaphore sem;
	struct timer_list tim;

	dsp_state_t state;
	int version;

	struct dsp_dev_t* dev;
	
	int n_handlers;
	dsp_handler_entry handlers[MAX_HANDLERS];

	dsp_callback callback;

} dsp_dat[MAX_CARDS];


#define SUBNAME "dsp_set_handler: "

int dsp_set_handler(u32 code, dsp_handler handler, unsigned long data)
{
	struct dsp_control *ddat = dsp_dat;
	int i;

	// Replace handler if it exists
	for (i=0; i<ddat->n_handlers; i++) {
		if (ddat->handlers[i].code == code) {
			ddat->handlers[i].handler = handler;
			ddat->handlers[i].data = data;
			return 0;
		}
	}

	// Add to end of list
	if (i < MAX_HANDLERS) {
		ddat->handlers[i].code = code;
		ddat->handlers[i].handler = handler;
		ddat->handlers[i].data = data;
	        ddat->n_handlers++;
		return 0;
	}

	PRINT_ERR(SUBNAME "no available handler slots\n");
	return -1;
}

#undef SUBNAME


#define SUBNAME "dsp_clear_handler: "

int dsp_clear_handler(u32 code)
{
	struct dsp_control *ddat = dsp_dat;
	int i = 0;
	for (i=0; i<ddat->n_handlers; i++) {
		if (ddat->handlers[i].code == code)
			break;
	}
	
	if (i>=ddat->n_handlers--)
		return -1;
	
	// Move entries i+1 to i.
	for ( ; i<ddat->n_handlers; i++) {
		memcpy(ddat->handlers + i, ddat->handlers + i + 1,
		       sizeof(*ddat->handlers));
	}

	// Clear the removed entry
	memset(ddat->handlers + ddat->n_handlers, 0, sizeof(*ddat->handlers));

	return 0;
}

#undef SUBNAME


/*
  dsp_int_handler

  This is not to be confused with pci_int_handler, which is the entry
  point for the DSP's hardware interrupts over the PCI bus.  Rather,
  this is the function that is called once pci_int_handler has
  identified a message as an REP.

  Although we'd like to respond to NFY immediately, we may be in the
  middle of some other command.  Not!
*/


#define SUBNAME "dsp_int_handler: "

int dsp_int_handler(dsp_message *msg)
{
	struct dsp_control *ddat = dsp_dat;
	int i;

	if (msg==NULL) {
		PRINT_ERR(SUBNAME "called with NULL message pointer!\n");
		return -1;
	}

	// Discover handler for this message type
	
	for (i=0; i < ddat->n_handlers; i++) {
		if ( (ddat->handlers[i].code == msg->type) &&
		     (ddat->handlers[i].handler != NULL) ) {
			ddat->handlers[i].handler(msg, ddat->handlers[i].data);
			return 0;
		}
	}

	PRINT_ERR(SUBNAME "unknown message type: %#06x\n", msg->type);
	return -1;
}

#undef SUBNAME


#define SUBNAME "dsp_reply_handler: "

/* This will handle REP interrupts from the DSP, which correspond to
 * replies to DSP commands.
 */

int dsp_reply_handler(dsp_message *msg, unsigned long data)
{
	struct dsp_control *ddat = dsp_dat;
	if (ddat->state == DDAT_CMD) {
		PRINT_INFO(SUBNAME
			   "REP received, calling back.\n");

		// Call the registered callbacks
		if (ddat->callback != NULL) {
			ddat->callback(0, msg);
		} else {
			PRINT_ERR(SUBNAME "no handler defined\n");
		}
		ddat->state = DDAT_IDLE;
	} else {
		PRINT_ERR(SUBNAME
			  "unexpected REP received [state=%i].\n",
			  ddat->state);
	}
	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_hey_handler: "

/* This will handle HEY interrupts from the DSP, which are generic
 * communications from the DSP typically used for debugging.
 */

int dsp_hey_handler(dsp_message *msg, unsigned long data)
{
	PRINT_ERR(SUBNAME "dsp HEY received: %06x %06x %06x\n",
		  msg->command, msg->reply, msg->data);
	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_timeout: "

void dsp_timeout(unsigned long data)
{
	struct dsp_control *my_ddat = (struct dsp_control*)data;

	if (my_ddat->state == DDAT_IDLE) {
		PRINT_INFO(SUBNAME "timer ignored\n");
		return;
	}

	PRINT_ERR(SUBNAME "dsp reply timed out!\n");
	if (my_ddat->callback != NULL)
		my_ddat->callback(-DSP_ERR_TIMEOUT, NULL);
	my_ddat->state = DDAT_IDLE;
}

#undef SUBNAME


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


#define SUBNAME "dsp_send_command: "

/* For better or worse, this routine returns linux error codes.
   
   The callback error codes are 0 (success, msg will be non-null) or
   DSP_ERR_TIMEOUT (failure, msg will be null).                    */

int dsp_send_command(dsp_command *cmd, dsp_callback callback, int card)
{
	struct dsp_control *ddat = dsp_dat;
	int err = 0;

	//INVALID WILL BE REMOVED SOON!
	ddat->dev = ddat->dev + card; 

	// This will often be called in atomic context
	if (down_trylock(&ddat->sem)) {
		PRINT_ERR(SUBNAME "could not get sem\n");
		return -EAGAIN;
	}
	
	PRINT_INFO(SUBNAME "entry\n");
		
	ddat->callback = callback;
	ddat->state = DDAT_CMD;

	if ( (err = dsp_send_command_now(cmd, ddat->dev)) ) {
		ddat->callback = NULL;
		ddat->state = DDAT_IDLE;
	} else {
		mod_timer(&ddat->tim, jiffies + DSP_DEFAULT_TIMEOUT);
	}

	PRINT_INFO(SUBNAME "returning [%i]\n", err);
	up(&ddat->sem);
	return err;
}
	
#undef SUBNAME


/*
 * Completely blocking commander - why do we have this?
 *
 * If this isn't in use by 2008, delete it.
 */
	
struct {

	struct semaphore sem;
	wait_queue_head_t queue;
	dsp_message *msg;
	int flags;
#define   LOCAL_CMD 0x01
#define   LOCAL_REP 0x02
#define   LOCAL_ERR 0x08

} dsp_local;

#define SUBNAME "dsp_send_command_wait_callback"

int dsp_send_command_wait_callback(int error, dsp_message *msg)
{
	wake_up_interruptible(&dsp_local.queue);

	if (dsp_local.flags != LOCAL_CMD) {
		PRINT_ERR(SUBNAME "unexpected flags, cmd=%x rep=%x err=%x\n",
			  dsp_local.flags & LOCAL_CMD,
			  dsp_local.flags & LOCAL_REP,
			  dsp_local.flags & LOCAL_ERR);
		return -1;
	}
	memcpy(dsp_local.msg, msg, sizeof(*dsp_local.msg));
	dsp_local.flags |= LOCAL_REP;

	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_send_command_wait: "

int dsp_send_command_wait(dsp_command *cmd,
			  dsp_message *msg)
{
	int err = 0;

	PRINT_INFO(SUBNAME "entry\n");

	// Try to get the default sem (spinlock!)
	if (down_trylock(&dsp_local.sem))
		return -ERESTARTSYS;

	//Register message for our callback to fill
	dsp_local.msg = msg;
	dsp_local.flags = LOCAL_CMD;
	
	if ((err=dsp_send_command(cmd, dsp_send_command_wait_callback, DEFAULT_CARD)) != 0)
		goto up_and_out;

	PRINT_INFO(SUBNAME "commanded, waiting\n");
	if (wait_event_interruptible(dsp_local.queue,
				     dsp_local.flags
				     & (LOCAL_REP | LOCAL_ERR))) {
		dsp_local.flags = 0;
		err = -ERESTARTSYS;
		goto up_and_out;
	}
	
	err = (dsp_local.flags & LOCAL_ERR) ? -EIO : 0;
	
 up_and_out:

	PRINT_INFO(SUBNAME "returning %x\n", err);
	up(&dsp_local.sem);
	return err;
}

#undef SUBNAME


/*
 *  IOCTL, for what it's worth...
 */

#define SUBNAME "dsp_driver_ioctl: "

int dsp_driver_ioctl(unsigned int iocmd, unsigned long arg)
{
	struct dsp_control *ddat = dsp_dat;

	switch(iocmd) {

	case DSPDEV_IOCT_SPEAK:
		PRINT_IOCT(SUBNAME "state=%#x\n", ddat->state);
		break;

	case DSPDEV_IOCT_CORE:
	case DSPDEV_IOCT_CORE_IRQ:
		return dsp_pci_ioctl(iocmd, arg, ddat->dev);

	default:
		PRINT_IOCT(SUBNAME "unknown command\n");
		return -1;
	}

	return 0;
}

#undef SUBNAME


int dsp_proc(char *buf, int count, int card)
{
	struct dsp_control *ddat = dsp_dat + card;
	int len = 0;
	
	if (len < count) {
		len += sprintf(buf+len, "    state:    ");
		switch (ddat->state) {
		case DDAT_IDLE:
			len += sprintf(buf+len, "idle\n");
			break;
		case DDAT_CMD:
			len += sprintf(buf+len, "commanded\n");
			break;
		default:
			len += sprintf(buf+len, "unknown! [%i]\n", ddat->state);
			break;
		}
	}

/* 	len += sprintf(buf+len, "    virtual: %#010x\n", */
/* 		       (unsigned)frames.base); */
	return len;
}


/*
 *  Initialization and clean-up
 */


#define SUBNAME "dsp_query_version: "

int dsp_query_version(void)
{
	struct dsp_control *ddat = dsp_dat;
	int err = 0;
	dsp_command cmd = { DSP_VER, {0,0,0} };
	dsp_message msg;
	char version[8] = "<=U0103";
	
	ddat->version = 0;
	if ( (err=dsp_send_command_wait(&cmd, &msg)) != 0 )
		return err;

	ddat->version = DSP_U0103;

	if (msg.reply == DSP_ACK) {
	
		version[0] = msg.data >> 16;
		sprintf(version+1, "%02i%02i",
			(msg.data >> 8) & 0xff, msg.data & 0xff);

		ddat->version = msg.data;
	}
		
	PRINT_ERR(SUBNAME " discovered PCI card DSP code version %s\n", version);
	return 0;
}

#undef SUBNAME




#define SUBNAME "dsp_driver_probe: "

int dsp_driver_probe(struct dsp_dev_t* dev)
{	
	struct dsp_control *ddat = dsp_dat;
	int err = 0;

	init_MUTEX(&ddat->sem);
	init_MUTEX(&dsp_local.sem);
	init_waitqueue_head(&dsp_local.queue);
	init_timer(&ddat->tim);

	ddat->tim.function = dsp_timeout;
	ddat->tim.data = (unsigned long)&ddat;
	ddat->state = DDAT_IDLE;
	ddat->dev = dev;

	// Set up handlers for the DSP interrupts - additional
	//  handlers will be set up by sub-modules.
	dsp_set_handler(DSP_REP, dsp_reply_handler, 0);
	dsp_set_handler(DSP_HEY, dsp_hey_handler, 0);

	// Version can only be obtained after REP handler has been set
	if (dsp_query_version()) {
		err = -1;
		goto out;
	}

	if (dsp_ops_init()) {
		err = -1;
		goto out;
	}
	
	if (mce_init_module(ddat->version)) {
		err = -1;
		goto out;
	}

	PRINT_INFO(SUBNAME "driver ok\n");
	return 0;

 out:
  
	dsp_driver_remove();

	PRINT_ERR(SUBNAME "exiting with errors!\n");
	return err;
}

#undef SUBNAME


#define SUBNAME "dsp_driver_remove: "

void dsp_driver_remove(void)
{
	struct dsp_control *ddat = dsp_dat;

	del_timer_sync(&ddat->tim);

	mce_cleanup();

	dsp_ops_cleanup();
}

#undef SUBNAME


#define SUBNAME "cleanup_module: "

void dsp_driver_cleanup(void)
{
#ifdef FAKEMCE
	dsp_driver_remove();
	dsp_fake_cleanup();
#else
	dsp_pci_cleanup();
#endif

	remove_proc_entry("mce_dsp", NULL);

	PRINT_INFO(SUBNAME "driver removed\n");
}    

#undef SUBNAME


#define SUBNAME "init_module: "

inline int dsp_driver_init(void)
{
	struct dsp_control *ddat = dsp_dat;
	int i = 0;

	PRINT_INFO(SUBNAME "driver init...\n");

	for(i = 0; i < MAX_CARDS; i++) { 
		ddat = dsp_dat +i;
		ddat->dev = NULL; 
	}

	create_proc_read_entry("mce_dsp", 0, NULL, read_proc, NULL);

#ifdef FAKEMCE
	dsp_fake_init( DSPDEV_NAME );
#else
	if (dsp_pci_init( DSPDEV_NAME )) {
		return -1;
	}
#endif
	return 0;
}

module_init(dsp_driver_init);
module_exit(dsp_driver_cleanup);

#undef SUBNAME
