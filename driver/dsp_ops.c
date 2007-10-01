#include <linux/init.h>
#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>

#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#include "dsp_errors.h"

#include "dsp.h"
#include "dsp_ops.h"
#include "dsp_driver.h"
#include "dsp_ioctl.h"


struct dsp_ops_t {

	int major;

	struct semaphore sem;
	wait_queue_head_t queue;

	int error;

	int flags;
#define   OPS_COMMANDED 0x1
#define   OPS_REPLIED   0x2
#define   OPS_ERROR     0x4

	dsp_message msg;
	dsp_command cmd;

} dsp_ops;


/*
  dsp_read: read method for mce_dsp driver

  Behaviour: if a command has been issued (flags & OPS_COMMANDED),
  dsp_read will block until the reply has been received or an error
  has been detected.  If no command has been issued by the time
  dsp_read obtains the reply semaphore, it returns with 0 bytes
  written.

  This will usually result in only a single reply packet being
  returned to a greedy user (e.g. a cat /dev/mce_dsp), but this is not
  guaranteed.  If a second command is issued immediately after the
  first reply is read but before the read has had a chance to return
  0, it will copy the second reply into the same buffer.

  The read routine cannot insist on returning 0 before allowing
  another response because an intelligent user process would only ever
  ask for a single response, which would not result in a second read
  call.
*/

#define SUBNAME "dsp_read: "

ssize_t dsp_read(struct file *filp, char __user *buf, size_t count,
                 loff_t *f_pos)
{
	int read_count = 0;
	int err = 0;

	PRINT_INFO(SUBNAME "flags=%#x\n", dsp_ops.flags);
	
	if (down_interruptible(&dsp_ops.sem)) {
		return -ERESTARTSYS;
	}

	if (dsp_ops.flags & OPS_ERROR) {
		PRINT_INFO(SUBNAME "ops error, exiting\n");
		dsp_ops.error = -DSP_ERR_ERRORS;
		goto out;
	}
   
	if (!dsp_ops.flags & OPS_COMMANDED) {
		PRINT_INFO(SUBNAME "not awaiting reply\n");
		dsp_ops.error = -DSP_ERR_COMMAND;
		goto out;
	}

	/* Blocking version */
	if (wait_event_interruptible(dsp_ops.queue,
				     dsp_ops.flags & (OPS_REPLIED | 
						      OPS_ERROR))) {
		PRINT_INFO(SUBNAME "interrupted\n");
		read_count = -ERESTARTSYS;
		goto out;
	}
	/* Non blocking version wouldn't do that :) */
	
	if ( !(dsp_ops.flags & OPS_REPLIED) ) {
		PRINT_INFO(SUBNAME "awoke with errors\n");
		goto out;
	}

	read_count = sizeof(dsp_ops.msg);
	if (read_count > count) read_count = count;

	err = copy_to_user(buf, (void*)&dsp_ops.msg, sizeof(dsp_ops.msg));
	read_count -= err;
	if (err) {
		PRINT_ERR(SUBNAME "could not copy %#x bytes to user\n",
			  err );
		dsp_ops.error = DSP_ERR_SYSTEM;
	}

	dsp_ops.flags &= OPS_ERROR; //~(OPS_COMMANDED | OPS_REPLIED);

 out:
	PRINT_INFO(SUBNAME "exiting (%i)\n", read_count);

	up(&dsp_ops.sem);
	return read_count;
}

#undef SUBNAME


/*
  dsp_write

  The service will attempt to copy 16 bytes from the user buffer and
  send them as a dsp command.  The return value will be 16 on a
  successful send, 0 if the data could not be sent, or a negative
  error value in some other cases.

  Following a successful command, the reply will eventually be
  available via the dsp_read method.
*/

int dsp_write_callback( int error, dsp_message* msg );

#define SUBNAME "dsp_write: "

ssize_t dsp_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos)
{
	int write_bytes = 0;

	PRINT_INFO("write: comm_flags=%#x\n", dsp_ops.flags);

	if (down_interruptible(&dsp_ops.sem))
		return -ERESTARTSYS;

	if (dsp_ops.flags & OPS_COMMANDED) {
		dsp_ops.flags |= OPS_ERROR;
		dsp_ops.error = DSP_ERR_COMMAND;
		goto out;
	}

	if (dsp_ops.flags & OPS_ERROR) {
		dsp_ops.error = DSP_ERR_ERRORS;
		goto out;
	}

	//Flags ok, so try to get the command data
  
	if (count != sizeof(dsp_ops.cmd)) {
		PRINT_ERR(SUBNAME "count != sizeof(dsp_command)\n");
		dsp_ops.error = DSP_ERR_FORMAT;
		goto out;
	}

	if (copy_from_user(&dsp_ops.cmd, buf, sizeof(dsp_ops.cmd))) {
		PRINT_ERR(SUBNAME "copy_from_user incomplete\n");
		dsp_ops.error = DSP_ERR_SYSTEM;
		goto out;
	}

	/* BLOCKING VERSION */
        /*
	  if (dsp_send_command_wait(&dsp_ops.cmd, &dsp_ops.msg)) {
	  dsp_ops.flags |= OPS_ERROR;
	  goto out;
	  }
	  dsp_ops.flags |= OPS_COMMANDED | OPS_REPLIED;
	  write_bytes = count;
	*/
	/* END */

	/* NON-BLOCKING VERSION*/
	if (dsp_send_command(&dsp_ops.cmd, dsp_write_callback)) {
		dsp_ops.flags |= OPS_ERROR;
		dsp_ops.error = DSP_ERR_IO;
		goto out;
	}
	dsp_ops.flags |= OPS_COMMANDED;
	write_bytes = count;
	/* END */

 out:
	up(&dsp_ops.sem);

	if (write_bytes==0) return -EPROTOTYPE;
	return write_bytes;
}

#undef SUBNAME

/* 
   dsp_write_callback

   This callback is called by the dsp comm system once a reply to a
   dsp_ops command has been received.  It executes in interrupt
   context!  No semaphores!
*/

int dsp_write_callback( int error, dsp_message* msg ) {
	
	wake_up_interruptible(&dsp_ops.queue);

	if (dsp_ops.flags != OPS_COMMANDED) {
		PRINT_ERR("dsp_write_callback: flags in "
			  "unexpected state, %#x\n", dsp_ops.flags);
		dsp_ops.flags |= OPS_ERROR;
		dsp_ops.error = DSP_ERR_FLAGS;
		return -1;
	}			  

	PRINT_INFO("dsp_write_callback: type=%#x\n", msg->type);
	memcpy(&dsp_ops.msg, msg, sizeof(dsp_ops.msg));
	dsp_ops.flags |= OPS_REPLIED;

	return 0;
}



int dsp_ioctl(struct inode *inode, struct file *filp,
	      unsigned int iocmd, unsigned long arg)
{
	switch(iocmd) {

	case DSPDEV_IOCT_RESET:
		PRINT_IOCT("ioctl: resetting comm flags\n");
		if (down_interruptible(&dsp_ops.sem)) {
			return -ERESTARTSYS;
		}
		dsp_ops.flags = 0;
		dsp_ops.error = 0;

		up(&dsp_ops.sem);
		break;

	case DSPDEV_IOCT_ERROR:
		return dsp_ops.error;

	default:
		return dsp_driver_ioctl(iocmd, arg);
	}

	return 0;

}


int dsp_open(struct inode *inode, struct file *filp)
{
	PRINT_INFO("dsp_open\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_INC_USE_COUNT;
#else
	if(!try_module_get(THIS_MODULE))
		return -1;
#endif   
	return 0;
}


int dsp_release(struct inode *inode, struct file *filp)
{
	PRINT_INFO("dsp_release\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_DEC_USE_COUNT;
#else
	module_put(THIS_MODULE);
#endif
	return 0;
}


struct file_operations dsp_fops = 
{
	.owner=   THIS_MODULE,
	.open=    dsp_open,
	.read=    dsp_read,
	.release= dsp_release,
	.write=   dsp_write,
	.ioctl=   dsp_ioctl,
};


int dsp_ops_init(void)
{
	int err = 0;

	init_waitqueue_head(&dsp_ops.queue);
	init_MUTEX(&dsp_ops.sem);

	err = register_chrdev(0, DSPDEV_NAME, &dsp_fops);
	if (err<0) {
		PRINT_ERR("dsp_ops_init: could not register_chrdev, "
			  "err=%#x\n", -err);
	} else {	  
		dsp_ops.major = err;
		err = 0;
	}

	return err;
}

int dsp_ops_cleanup(void) {

	if (dsp_ops.major != 0) 
		unregister_chrdev(dsp_ops.major, DSPDEV_NAME);

	return 0;
}
