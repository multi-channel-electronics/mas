#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#include "mce_ops.h"
#include "mce_driver.h"

struct file_operations mce_fops = 
{
 	.owner=   THIS_MODULE,
 	.open=    mce_open, 
	.read=    mce_read,
 	.release= mce_release,
	.write=   mce_write,
// 	.ioctl=   mce_ioctl,
};

typedef enum {
	OPS_IDLE = 0,
	OPS_CMD,
	OPS_REP,
	OPS_ERR
} mce_ops_state_t;

struct mce_ops_t {

	int major;

	struct semaphore sem;
	wait_queue_head_t queue;

	mce_ops_state_t state;

/* 	int flags; */
/* #define   OPS_COMMANDED  0x1 */
/* #define   OPS_REPLIED    0x2 */
/* #define   OPS_ERROR      0x8 */

	mce_reply   rep;
	mce_command cmd;

} mce_ops;

/* The behaviour of the flags shall be as follows:

OPS_ERROR indicates a failed command/reply, and blocks further
    commands and reply notifications. OPS_ERROR should be cleared as
    soon as the user is informed of the error by calling the read
    method.

OPS_COMMANDED indicates that a command has been sent and that we are
    awaiting a reply.  No other commands are permitted while this flag
    is set, and these will fail without changing the flags.

OPS_REPLIED indicates that a reply has been received.  It is set by
    the interrupt handler only if OPS_COMMANDED is set and OPS_ERROR
    is not.  It is cleared simultaneously with OPS_COMMANDED when the
    reply is passed out via the read method.

*/



#define SUBNAME "mce_read: "

ssize_t mce_read(struct file *filp, char __user *buf, size_t count,
                 loff_t *f_pos)
{
	int err = 0;
	int ret_val = 0;

	PRINT_INFO(SUBNAME "state=%#x\n", mce_ops.state);

	// Lock semaphore

	if (filp->f_flags & O_NONBLOCK) {
		PRINT_INFO(SUBNAME "non-blocking call\n");
		if (down_trylock(&mce_ops.sem))
			return -EAGAIN;
	} else {
		PRINT_INFO(SUBNAME "blocking call\n");
		if (down_interruptible(&mce_ops.sem))
			return -ERESTARTSYS;
	}


	// If command in progress, block if we're allowed.

	while ( !(filp->f_flags & O_NONBLOCK) && (mce_ops.state == OPS_CMD) ) {
		if (wait_event_interruptible(mce_ops.queue,
					     mce_ops.state != OPS_CMD)) {
			ret_val = -ERESTARTSYS;
			goto out;
		}
	}

	switch (mce_ops.state) {

	case OPS_IDLE:
		ret_val = 0;
		goto out;
		
	case OPS_CMD:
		// Not possible in blocking calls
		ret_val = -EAGAIN;
		goto out;
			
	case OPS_REP:
		// Fix me: partial packet reads not supported
		ret_val = sizeof(mce_ops.rep);
		if (ret_val > count) ret_val = count;
	
		err = copy_to_user(buf, (void*)&mce_ops.rep, ret_val);
		ret_val -= err;
		if (err)
			PRINT_ERR(SUBNAME
				  "could not copy %#x bytes to user\n", err );

		mce_ops.state = OPS_IDLE;
		break;
		
	case OPS_ERR:
	default:
		ret_val = 0;
		mce_ops.state = OPS_IDLE;
	}
		
out:
	PRINT_INFO(SUBNAME "exiting (%i)\n", ret_val);
	
	up(&mce_ops.sem);
	return ret_val;
}

#undef SUBNAME


#define SUBNAME "mce_write_callback: "

int mce_write_callback( int error, mce_reply* rep )
{
	// Reject unexpected interrupts
	if (mce_ops.state != OPS_CMD) {
		PRINT_ERR(SUBNAME "state is %#x, expected %#x\n",
			  mce_ops.state, OPS_CMD);
		return -1;
	}			  

	// Change state to REP or ERR, awaken readers, exit with success.
	if (error || rep==NULL) {
		PRINT_ERR(SUBNAME "called with error=%i, rep=%lx\n",
			  error, (unsigned long)rep);
		memset(&mce_ops.rep, 0, sizeof(mce_ops.rep));
		mce_ops.state = OPS_ERR;
	} else {
		PRINT_INFO(SUBNAME "type=%#x\n", rep->ok_er);
		memcpy(&mce_ops.rep, rep, sizeof(mce_ops.rep));
		mce_ops.state = OPS_REP;
	}

	wake_up_interruptible(&mce_ops.queue);
	return 0;
}

#undef SUBNAME

#define SUBNAME "mce_write: "

ssize_t mce_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos)
{
	int err = 0;
	int ret_val = 0;

	PRINT_INFO(SUBNAME "state=%#x\n", mce_ops.state);

	// Non-blocking version may not wait on semaphore.

	if (filp->f_flags & O_NONBLOCK) {
		PRINT_INFO(SUBNAME "non-blocking call\n");
		if (down_trylock(&mce_ops.sem))
			return -EAGAIN;
	} else {
		PRINT_INFO(SUBNAME "blocking call\n");
		if (down_interruptible(&mce_ops.sem))
			return -ERESTARTSYS;
	}

	switch (mce_ops.state) {

	case OPS_IDLE:
		break;

	case OPS_CMD:
	case OPS_REP:
	case OPS_ERR:
	default:
		ret_val = 0;
		goto out;
	}

	// Command size check
	if (count != sizeof(mce_ops.cmd)) {
		PRINT_ERR(SUBNAME "count != sizeof(mce_command)\n");
		ret_val = -EPROTO;
		goto out;
	}

	// Copy command to local buffer
	if ( (err=copy_from_user(&mce_ops.cmd, buf,
				 sizeof(mce_ops.cmd)))!=0) {
		PRINT_ERR(SUBNAME "copy_from_user incomplete\n");
		ret_val = count - err;
		goto out;
	}

	// Set CMD, then go back to IDLE on failure.
	//  - leaves us vulnerable to unexpected interrupts
	//  - the alternative risks losing expected interrupts

	mce_ops.state = OPS_CMD;
	if (mce_send_command(&mce_ops.cmd, mce_write_callback, 1)) {
		PRINT_ERR(SUBNAME "mce_send_command failed\n");
		mce_ops.state = OPS_IDLE;
		ret_val = 0;
		goto out;
	}
 
	ret_val = count;
 out:
	up(&mce_ops.sem);

	PRINT_INFO(SUBNAME "exiting [%#x]\n", ret_val);
	return ret_val;
}

#undef SUBNAME


int mce_open(struct inode *inode, struct file *filp)
{
	PRINT_INFO("mce_open\n");
	return 0;
}


int mce_release(struct inode *inode, struct file *filp)
{
	PRINT_INFO("mce_release\n");
	return 0;
}


int mce_ops_init(void) {
	int err = 0;

	PRINT_INFO("mce_ops_init: entry\n");

	init_waitqueue_head(&mce_ops.queue);
	init_MUTEX(&mce_ops.sem);

	err = register_chrdev(0, MCEDEV_NAME, &mce_fops);
	if (err<0) {
		PRINT_ERR("mce_ops_init: could not register_chrdev,"
			  "err=%#x\n", -err);
	} else {	  
		mce_ops.major = err;
		err = 0;
	}

	return err;
}

int mce_ops_cleanup(void)
{
	if (mce_ops.major != 0) 
		unregister_chrdev(mce_ops.major, MCEDEV_NAME);

	return 0;
}
