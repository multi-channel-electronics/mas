/*
 * dsp_driver.c
 */


#include <linux/config.h>
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

#include "dsp_ioctl.h"

#ifdef FAKEMCE
#  include <dsp_fake.h>
#else
#  include "dsp_pci.h"
#endif

#include "mce_driver.h"
#include "dsp_ops.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Matthew Hasselfield"); 


typedef enum {

	DDAT_IDLE = 0,
	DDAT_CMD,

} dsp_state_t;


struct dsp_control {
	
	struct semaphore sem;
	struct timer_list tim;
	int tim_ignore;

	dsp_state_t state;
	int evil_flag;

	dsp_callback callback;

} ddat;



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
	if (msg==NULL) {
		PRINT_ERR(SUBNAME "called with NULL message pointer!\n");
		return -1;
	}

	// Do not dispatch NFY unless pending REP have been handled.

	switch((dsp_message_code)msg->type) {
		
	case DSP_REP: // Message is a reply to a DSP command
		
		if (ddat.state == DDAT_CMD) {
			PRINT_INFO(SUBNAME
				   "REP received, calling back.\n");

			// Call the registered callbacks
			if (ddat.callback != NULL) {
				ddat.callback(0, msg);
			} else {
				PRINT_ERR(SUBNAME "no handler defined\n");
			}
			ddat.tim_ignore = 1;
			ddat.state = DDAT_IDLE;
		} else {
			PRINT_ERR(SUBNAME
				  "unexpected REP received.\n");
		}
		break;
		
	case DSP_NFY: // Message is notification of MCE packet

		// Register the NFY; may result in HST being issued.
		mce_int_handler( msg );

		break;

	case DSP_QTI:

		mce_qti_handler( msg );

		break;

	default:
		PRINT_ERR(SUBNAME "unknown message type: %#06x\n", msg->type);
		return -1;
	}

	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_timeout: "

void dsp_timeout(unsigned long data)
{
	dsp_callback callback = (dsp_callback)data;

	if (ddat.tim_ignore) {
		PRINT_INFO(SUBNAME "timer ignored\n");
		ddat.tim_ignore = 0;
		return;
	}

	PRINT_ERR(SUBNAME "dsp reply timed out!\n");
	if (callback != NULL)
		callback(-1, NULL);
	ddat.state = DDAT_IDLE;
}

#undef SUBNAME


/* #define SUBNAME "nfy_task: " */

/* void nfy_task(unsigned long data) */
/* { */
/* 	if (ddat.state == DDAT_IDLE) { */
/* 		PRINT_INFO(SUBNAME "NFY being dispatched " */
/* 			   "to mce_int_handler\n"); */
/* 		mce_int_handler( &ddat.nfy_storage ); */
/* 		ddat.nfy_waiting = 0; */
/* 	} else { */
/* 		tasklet_schedule(&ddat.nfy_tasklet); */
/* 	} */
/* } */

/* #undef SUBNAME */


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

int dsp_send_command(dsp_command *cmd,
		     dsp_callback callback)
{
	int err = 0;
	
	if (down_trylock(&ddat.sem)) {
		return -DSP_ERR_SYSTEM;
	}
	
	PRINT_INFO(SUBNAME "entry\n");

	// An interrupt that commands will set this flag.
	ddat.evil_flag = 0;
	barrier();

	if (ddat.state != DDAT_IDLE) {
		err = -1;
		goto up_and_out;
	}

	// After this call we are safe from interrupt routine
	ddat.state = DDAT_CMD;
	barrier();
	
	if (ddat.evil_flag) {
		// Damn you, interrupt
		err = -DSP_ERR_SYSTEM;
		goto up_and_out;
	}
		
	ddat.callback = callback;
	ddat.state = DDAT_CMD;

	if ( (err = dsp_send_command_now(cmd)) ) {
		ddat.callback = NULL;
		ddat.state = DDAT_IDLE;
	} else {
		//Setup new timeout
		del_timer_sync(&ddat.tim);
		ddat.tim.function = dsp_timeout;
		ddat.tim.data = (unsigned long)ddat.callback;
		ddat.tim.expires = jiffies + HZ;
		ddat.tim_ignore = 0;
		add_timer(&ddat.tim);
	}

up_and_out:
	PRINT_INFO(SUBNAME "returning [%i]\n", err);
	up(&ddat.sem);
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
		return -1;

	PRINT_INFO(SUBNAME "register\n");

	//Register message for our callback to fill
	dsp_local.msg = msg;
	dsp_local.flags = LOCAL_CMD;
	
	PRINT_INFO(SUBNAME "send\n");

	if (dsp_send_command(cmd, dsp_send_command_wait_callback)) {
		err = -1;
		goto up_and_out;
	}

	PRINT_INFO(SUBNAME "wait\n");

	if (wait_event_interruptible(dsp_local.queue,
				     dsp_local.flags
				     & (LOCAL_REP | LOCAL_ERR))) {
		dsp_local.flags = 0;
		err = -ERESTARTSYS;
		goto up_and_out;
	}
	
	PRINT_INFO(SUBNAME "check success\n");
	err = (dsp_local.flags & LOCAL_ERR) ? -1 : 0;
	
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
	switch(iocmd) {

	case DSPDEV_IOCT_SPEAK:
		PRINT_IOCT(SUBNAME "state=%#x\n", ddat.state);
		break;

	case DSPDEV_IOCT_CORE:
	case DSPDEV_IOCT_CORE_IRQ:
		return dsp_pci_ioctl(iocmd, arg);

	default:
		PRINT_IOCT(SUBNAME "unknown command\n");
		return -1;
	}

	return 0;
}

#undef SUBNAME


/*
 *  Initialization and clean-up
 */

#define SUBNAME "cleanup_module: "

void driver_cleanup(void)
{
	del_timer_sync(&ddat.tim);

	mce_cleanup();

	dsp_ops_cleanup();

#ifdef FAKEMCE
	dsp_fake_cleanup();
#else
	dsp_pci_cleanup();
#endif

	PRINT_INFO(SUBNAME "driver removed\n");
}    

#undef SUBNAME


#define SUBNAME "init_module: "

inline int driver_init(void)
{
	int err = 0;

	PRINT_INFO(SUBNAME "driver init...\n");

	init_MUTEX(&ddat.sem);

	init_MUTEX(&dsp_local.sem);
	init_waitqueue_head(&dsp_local.queue);

	init_timer(&ddat.tim);
  
#ifdef FAKEMCE
	dsp_fake_init( DSPDEV_NAME );
#else
	if (dsp_pci_init( DSPDEV_NAME )) {
		err = -1;
		goto out;
	}
#endif

	if (dsp_ops_init()) {
		err = -1;
		goto out;
	}
	
	if (mce_init_module()) {
		err = -1;
		goto out;
	}

	PRINT_INFO(SUBNAME "driver ok\n");
	return 0;

 out:
  
	driver_cleanup();

	PRINT_ERR(SUBNAME "exiting with errors!\n");
	return err;
}

module_init(driver_init);
module_exit(driver_cleanup);

#undef SUBNAME
