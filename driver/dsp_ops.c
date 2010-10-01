/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#include <linux/init.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/sched.h>

#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#include "mce/dsp_errors.h"

#include "mce/dsp.h"
#include "dsp_ops.h"
#include "dsp_driver.h"
#include "mce/dsp_ioctl.h"

typedef enum {
	OPS_IDLE = 0,
	OPS_CMD,
	OPS_REP,
	OPS_ERR
} dsp_ops_state_t;

struct filp_pdata {
	int minor;
};


struct dsp_ops_t {

	int major;

	struct semaphore sem;
	wait_queue_head_t queue;

	int error;

	dsp_ops_state_t state;

	dsp_message msg;
	dsp_command cmd;

} dsp_ops[MAX_CARDS];


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

ssize_t dsp_read(struct file *filp, char __user *buf, size_t count,
                 loff_t *f_pos)
{
	struct filp_pdata *fpdata = filp->private_data;
	struct dsp_ops_t *dops = dsp_ops + fpdata->minor;
        int card = fpdata->minor;
	int read_count = 0;
	int ret_val = 0;
	int err = 0;

        PRINT_INFO(card, "entry! fpdata->minor=%d state=%#x\n", 
		   fpdata->minor, dops->state);

	if (filp->f_flags & O_NONBLOCK) {
		if (down_trylock(&dops->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&dops->sem))
			return -ERESTARTSYS;
	}		

	switch (dops->state) {
	case OPS_CMD:
		if (filp->f_flags & O_NONBLOCK) {
			ret_val = -EAGAIN;
			goto out;
		} 

		if (wait_event_interruptible(dops->queue,
					     dops->state != OPS_CMD)) {
			ret_val = -ERESTARTSYS;
			goto out;
		}

		break;
		
	case OPS_REP:
	case OPS_ERR:
		break;

	default:
		ret_val = 0;
		goto out;
	}
	
	if (dops->state == OPS_REP) {
		
		read_count = sizeof(dops->msg);
		if (read_count > count) read_count = count;
		
		err = copy_to_user(buf, (void*)&dops->msg, sizeof(dops->msg));
		read_count -= err;
		if (err) {
                        PRINT_ERR(card, "could not copy %#x bytes to user\n",
				  err );
		}
	} else {
		read_count = 0;
	}

	dops->state = OPS_IDLE;
	ret_val = read_count;

 out:
        PRINT_INFO(card, "exiting (ret_val=%i)\n", ret_val);

	up(&dops->sem);
	return ret_val;
}


/*
  dsp_write

  The service will attempt to copy 16 bytes from the user buffer and
  send them as a dsp command.  The return value will be 16 on a
  successful send, 0 if the data could not be sent, or a negative
  error value in some other cases.

  Following a successful command, the reply will eventually be
  available via the dsp_read method.
*/

int dsp_write_callback(int error, dsp_message* msg, int card);

ssize_t dsp_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos)
{
	struct filp_pdata *fpdata = filp->private_data;
	struct dsp_ops_t *dops = dsp_ops + fpdata->minor;
        int card = fpdata->minor;
	int ret_val = 0;
	int err = 0;

        PRINT_INFO(card, "entry! fpdata->minor=%d state=%#x\n", 
		   fpdata->minor, dops->state);

	if (filp->f_flags & O_NONBLOCK) {
		if (down_trylock(&dops->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&dops->sem))
			return -ERESTARTSYS;
	}		

	dops->error = 0;

	switch (dops->state) {
	case OPS_IDLE:
		break;

	case OPS_CMD:
	case OPS_REP:
		ret_val = 0;
		dops->error = -DSP_ERR_ACTIVE;
		goto out;

	default:
		ret_val = 0;
		dops->error = -DSP_ERR_STATE;
		goto out;
	}

	//Flags ok, so try to get the command data
  
	if (count != sizeof(dops->cmd)) {
                PRINT_ERR(card, "count != sizeof(dsp_command)\n");
		ret_val = -EPROTO;
		goto out;
	}

	if (copy_from_user(&dops->cmd, buf, sizeof(dops->cmd))) {
                PRINT_ERR(card, "copy_from_user incomplete\n");
		ret_val = 0;
		dops->error = -DSP_ERR_KERNEL;
		goto out;
	}

	dops->state = OPS_CMD;
	if ((err=dsp_send_command(&dops->cmd, dsp_write_callback, fpdata->minor, 0))!=0) {
		dops->state = OPS_IDLE;
                PRINT_ERR(card, "dsp_send_command failed [%#0x]\n", err);
		ret_val = 0;
		dops->error = err;
		goto out;
	}

	ret_val = count;

 out:
	up(&dops->sem);

        PRINT_INFO(card, "exiting with code %i.\n", ret_val);
	return ret_val;
}

/* 
   dsp_write_callback

   This callback is called by the dsp comm system once a reply to a
   dsp_ops command has been received.  It executes in interrupt
   context!  No semaphores!
*/

int dsp_write_callback(int error, dsp_message* msg, int card)
{
        struct dsp_ops_t *dops = dsp_ops + card;

	if (dops->state != OPS_CMD) {
                PRINT_ERR(card, "state is %#x, expected %#x\n",
			  dops->state, OPS_CMD);
		return -1;
	}			  

	if (error) {
                PRINT_ERR(card, "called with error\n");
		memset(&dops->msg, 0, sizeof(dops->msg));
		dops->state = OPS_ERR;
		dops->error = error;
		return 0;
	}

	if (msg==NULL) {
                PRINT_ERR(card, "called with null message\n");
		memset(&dops->msg, 0, sizeof(dops->msg));
		dops->state = OPS_ERR;
		dops->error = -DSP_ERR_UNKNOWN;
		return 0;
	}

        PRINT_INFO(card, "type=%#x\n", msg->type);
	memcpy(&dops->msg, msg, sizeof(dops->msg));
	dops->state = OPS_REP;
	wake_up_interruptible(&dops->queue);

	return 0;
}

int dsp_ioctl(struct inode *inode, struct file *filp,
	      unsigned int iocmd, unsigned long arg)
{
	struct filp_pdata *fpdata = filp->private_data;
	struct dsp_ops_t *dops = dsp_ops + fpdata->minor;
        int card = fpdata->minor;
	int x;

        PRINT_INFO(card, "entry! fpdata->minor=%d\n", fpdata->minor);

	switch(iocmd) {

	case DSPDEV_IOCT_RESET:
                PRINT_IOCT(card, "resetting comm flags\n");
		if (down_interruptible(&dops->sem)) {
			return -ERESTARTSYS;
		}
		dops->state = OPS_IDLE;
		dops->error = 0;

		up(&dops->sem);
		break;

	case DSPDEV_IOCT_ERROR:
		x = dops->error;
		dops->error = 0;
		return x;

	default:
                PRINT_INFO(card, "ok\n");
		return dsp_driver_ioctl(iocmd, arg, fpdata->minor);
	}

        PRINT_INFO(card, "ok\n");
	return 0;

}

int dsp_open(struct inode *inode, struct file *filp)
{
	struct filp_pdata *fpdata;
	int minor = iminor(inode);
        PRINT_INFO(minor, "entry! iminor(inode)=%d\n", minor);

	if (!dsp_ready(minor)) {
                PRINT_ERR(minor, "card %i not enabled.\n", minor);
		return -ENODEV;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_INC_USE_COUNT;
#else
	if(!try_module_get(THIS_MODULE))
		return -1;
#endif   
	fpdata = kmalloc(sizeof(struct filp_pdata), GFP_KERNEL);
        fpdata->minor = iminor(inode);
	filp->private_data = fpdata;

        PRINT_INFO(minor, "ok\n");
	return 0;
}

int dsp_release(struct inode *inode, struct file *filp)
{
	struct filp_pdata *fpdata = filp->private_data;
        int card = fpdata->minor;

        PRINT_INFO(card, "entry!\n");

	if (fpdata != NULL) {
		kfree(fpdata);
	} else
                PRINT_ERR(card, "called with NULL private_data!\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_DEC_USE_COUNT;
#else
	module_put(THIS_MODULE);
#endif
        PRINT_INFO(card, "ok\n");
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


int dsp_ops_probe(int card)
{
	struct dsp_ops_t *dops = dsp_ops + card;

	init_waitqueue_head(&dops->queue);
	init_MUTEX(&dops->sem);

	dops->state = OPS_IDLE;

	return 0;
}

void dsp_ops_cleanup(void)
{
	if (dsp_ops->major != 0) 
		unregister_chrdev(dsp_ops->major, DSPDEV_NAME);

	return;
}

int dsp_ops_init(void)
{
	int i = 0;
	int err = 0;
	
	err = register_chrdev(0, DSPDEV_NAME, &dsp_fops);

	if (err<0) {
                PRINT_ERR(NOCARD, "dsp_ops_init: could not register_chrdev, "
			  "err=%#x\n", -err);
	} else {
		for(i=0; i<MAX_CARDS; i++) {
			struct dsp_ops_t *dops = dsp_ops + i;
			dops->major = err;
		}
		err = 0;
	}

	return err;
}
