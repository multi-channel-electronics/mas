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

	int flags;
#define   OPS_COMMANDED  0x1
#define   OPS_REPLIED    0x2
#define   OPS_ERROR      0x8

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
	int read_count = 0;
	int err = 0;
	int last_flags = 0;

	PRINT_INFO(SUBNAME "flags=%#x\n", mce_ops.flags);

	// Separate blocking/non-blocking to get to the point where a
	// reply can be copied.
	
	if (filp->f_flags & O_NONBLOCK) {
		// Non-blocking code must copy reply immediately

		PRINT_INFO(SUBNAME "non-blocking call\n");

		if (down_trylock(&mce_ops.sem))
			return -EAGAIN;

		if (mce_ops.flags & OPS_ERROR) {
			mce_ops.flags = 0;
			goto out;
		}
		
		if ( !(mce_ops.flags & OPS_COMMANDED) )
			goto out;

		if ( !(mce_ops.flags & OPS_REPLIED) ) {
			read_count = -EAGAIN;
			goto out;
		}
		
	} else {
                //Blocking, wait for reply if command has been sent.

		PRINT_INFO(SUBNAME "blocking call\n");

		if (down_interruptible(&mce_ops.sem))
			return -ERESTARTSYS;
		last_flags = mce_ops.flags;
		barrier();

		// Loop exits cleanly on REPLY, escapes if necessary

		while ( !(last_flags & OPS_REPLIED) ) {

			// With sem held, check error and commanded flags.

			if (last_flags & OPS_ERROR) {
				mce_ops.flags = 0;
				goto out;
			}

			if ( !(last_flags & OPS_COMMANDED) )
				goto out;
			
			up(&mce_ops.sem);

			PRINT_INFO(SUBNAME "sleeping with flags=%x\n", last_flags);
			
			if (wait_event_interruptible(mce_ops.queue,
						     mce_ops.flags != last_flags)) {
				PRINT_INFO(SUBNAME "interrupted\n");
				read_count = -ERESTARTSYS;
				goto out;
			}

			PRINT_INFO(SUBNAME "waking with flags=%x\n", mce_ops.flags);
			
			if (down_interruptible(&mce_ops.sem))
				return -ERESTARTSYS;

			last_flags = mce_ops.flags;
			barrier();
		}
	}

/* 	if (mce_ops.flags & OPS_ERROR) { */
/* 		PRINT_INFO(SUBNAME "ops error, clearing and exiting\n"); */
/* 		mce_ops.flags = 0; */
/* 		goto out; */
/* 	} */
	
/* 	if ( !(mce_ops.flags & OPS_COMMANDED) ) { */
/* 		PRINT_INFO(SUBNAME "not awaiting reply\n"); */
/* 		goto out; */
/* 	} */

/* 	PRINT_INFO(SUBNAME "sleeping\n"); */

/* 	if (wait_event_interruptible(mce_ops.queue, */
/* 				     mce_ops.flags & (OPS_REPLIED |  */
/* 						   OPS_ERROR))) { */
/* 		PRINT_INFO(SUBNAME "interrupted\n"); */
/* 		read_count = -ERESTARTSYS; */
/* 		goto out; */
/* 	} */

/* 	if ( mce_ops.flags & OPS_ERROR ) { */
/* 		PRINT_INFO(SUBNAME "awoke with error\n"); */
/* 		mce_ops.flags = 0; */
/* 		goto out;	 */
/* 	} */
	
/* 	if ( !(mce_ops.flags & OPS_REPLIED) ) { */
/* 		PRINT_INFO(SUBNAME "awoke without reply\n"); */
/* 		mce_ops.flags = 0; */
/* 		goto out;	 */
/* 	} */

	read_count = sizeof(mce_ops.rep);
	if (read_count > count) read_count = count;
	
	err = copy_to_user(buf, (void*)&mce_ops.rep, sizeof(mce_ops.rep));
	read_count -= err;
	if (err)
		PRINT_ERR(SUBNAME "could not copy %#x bytes to user\n",
			  err );
	
	mce_ops.flags = 0;
	
 out:
	PRINT_INFO(SUBNAME "exiting (%i)\n", read_count);
	
	up(&mce_ops.sem);
	return read_count;	
}

#undef SUBNAME


#define SUBNAME "mce_write_callback: "

int mce_write_callback( int error, mce_reply* rep ) {
	
	wake_up_interruptible(&mce_ops.queue);

	if (error) {
		PRINT_INFO(SUBNAME "called with error\n");
		mce_ops.flags |= OPS_ERROR;
		return 0;
	}
	
	if (mce_ops.flags != OPS_COMMANDED) {
		PRINT_ERR(SUBNAME "flags in unexpected state, %#x\n",
			  mce_ops.flags);
		return -1;
	}			  

	PRINT_INFO(SUBNAME "msg=%#lx\n", (long)rep);
	if (rep==NULL) {
		PRINT_INFO(SUBNAME "called with null reply\n");
		mce_ops.flags |= OPS_ERROR;
		return -1;
	}

	memcpy(&mce_ops.rep, rep, sizeof(mce_ops.rep));
	mce_ops.flags |= OPS_REPLIED;

	return 0;
}

#undef SUBNAME

#define SUBNAME "mce_write: "

ssize_t mce_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos)
{
	int write_bytes = 0;

	PRINT_INFO(SUBNAME "ops_flags=%#x\n", mce_ops.flags);

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

	// Must be idle
	if (mce_ops.flags & (OPS_ERROR | OPS_COMMANDED | OPS_REPLIED) ) {
		mce_ops.flags = 0;
		goto out;
	}

	// Command size check
	if (count != sizeof(mce_ops.cmd)) {
		PRINT_ERR(SUBNAME "count != sizeof(mce_command)\n");
		goto out;
	}

	// Copy command to local buffer
	if (copy_from_user( &mce_ops.cmd, buf, sizeof(mce_ops.cmd))) {
		PRINT_ERR(SUBNAME "copy_from_user incomplete\n");
		goto out;
	}

	mce_ops.flags |= OPS_COMMANDED;

	if (mce_send_command(&mce_ops.cmd, mce_write_callback)) {
		PRINT_ERR(SUBNAME "mce_send_command failed\n");
		mce_ops.flags &= ~OPS_COMMANDED;
		goto out;
	}
 
	write_bytes = count;
 out:
	up(&mce_ops.sem);

	PRINT_INFO(SUBNAME "exiting with %#x\n", write_bytes);
	if (write_bytes==0) return -EPROTOTYPE;
	return write_bytes;
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
