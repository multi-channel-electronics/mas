#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#include "dsp_driver.h"
#include "data.h"
#include "mce_driver.h"

#ifdef OPT_WATCHER
# include "data_watcher.h"
#endif



/**************************************************************************
 *                                                                        *
 *      Character device operations                                       *
 *                                                                        *
 **************************************************************************/

#define SUBNAME "data_read: "

/* Non-blocking behaviour: return -EAGAIN if we can't get the
 * semaphore.  Return 0 if no data is available.  Otherwise, copy as
 * much data as you can and return the count.
 *
 * Blocking behaviour: block for the semaphore.  If data is available,
 * copy as much as you can and return it.  If data is not available,
 * block for availability; once available, copy as much as possible
 * and exit.  (This will always block for a single frame but never
 * hang if the requested buffer size exceeds the data size.)  This
 * method calls back the reader in quiet periods and keeps buffers
 * flushed and stuff like that.
 */


ssize_t data_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	int read_count = 0;
	int this_read = 0;

	if (filp->f_flags & O_NONBLOCK) {
		if (down_trylock(&frames.sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&frames.sem))
			return -ERESTARTSYS;
	}

	PRINT_INFO(SUBNAME "user demands %i with nonblock=%i\n",
		   count, filp->f_flags & O_NONBLOCK);

	while (count > 0) {
		
		// Pop count bytes from frame buffer; data_copy frame
		// will return at most 1 frame of data.
		this_read = data_copy_frame(buf, NULL, count, 0);

		if (this_read < 0) {
			// On error, exit with the current count.
			break;
		} else if (this_read == 0) {
			// Buffer is empty, for now, so unless we
			// haven't copied any bytes yet or we're
			// O_NONBLOCK, exit back to the user.
			if (filp->f_flags & O_NONBLOCK || read_count > 0)
				break;
			if (wait_event_interruptible(
				    frames.queue,
				    (frames.flags & FRAME_ERR) ||
				    (frames.tail_index
				     != frames.head_index))) {
				read_count = -ERESTARTSYS;
				goto up_and_out;
			}
		} else {
			// Update counts and read again.
			read_count += this_read;
			count -= this_read;
		}
	}

up_and_out:
	up(&frames.sem);
	return read_count;
}

ssize_t data_write(struct file *filp, const char __user *buf, size_t count,
		   loff_t *f_pos)
{
	data_report();
	return count;
}


int data_ioctl(struct inode *inode, struct file *filp,
	       unsigned int iocmd, unsigned long arg)
{
	switch(iocmd) {

	case DATADEV_IOCT_RESET:
		PRINT_INFO("ioctl: reset\n");
		break;

	case DATADEV_IOCT_QUERY:
		PRINT_INFO("ioctl: query\n");
		switch (arg) {
		case QUERY_HEAD:
			return frames.head_index;
		case QUERY_TAIL:
			return frames.tail_index;
		case QUERY_MAX:
			return frames.max_index;
		case QUERY_PARTIAL:
			return frames.partial;
		case QUERY_DATASIZE:			
			return frames.data_size;
		case QUERY_FRAMESIZE:
			return frames.frame_size;
		default:
			return -1;
		}
		break;

	case DATADEV_IOCT_SET_DATASIZE:
		PRINT_INFO(SUBNAME "set data_size to %li (%#lx)\n",
			   arg, arg);
		return data_frame_resize(arg);

	case DATADEV_IOCT_FAKE_STOPFRAME:
		PRINT_ERR(SUBNAME "fake_stopframe initiated!\n");
		return data_frame_fake_stop();

	case DATADEV_IOCT_WATCH:
	case DATADEV_IOCT_WATCH_DL:
#ifdef OPT_WATCHER
		PRINT_INFO("ioctl: watch control\n");
		return watcher_ioctl(iocmd, arg);
#else
		PRINT_IOCT("ioctl: watch function; disabled!\n");
		return -1;
#endif

	case DATADEV_IOCT_EMPTY:
		PRINT_INFO("ioctl: reset data buffer\n");
		mce_error_reset();
		return data_frame_empty_buffers();

	case DATADEV_IOCT_QT_CONFIG:
		PRINT_INFO("ioctl: configure Quiet Transfer mode "
			   "[inform=%li]\n", arg);
		return data_qt_configure(arg);

	case DATADEV_IOCT_QT_ENABLE:
		PRINT_INFO("ioctl: enable/disable quiet Transfer mode "
			   "[on=%li]\n", arg);
		return data_qt_enable(arg);

	default:
		PRINT_ERR("ioctl: unknown command (%#x)\n", iocmd );
	}

	return 0;
}

#undef SUBNAME

int data_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int data_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations data_fops = 
{
	.owner=   THIS_MODULE,
	.open=    data_open, 
	.read=    data_read,
	.release= data_release,
	.write=   data_write,
 	.ioctl=   data_ioctl,
};


/**************************************************************************
 *                                                                        *
 *      Init and cleanup                                                  *
 *                                                                        *
 **************************************************************************/

#define SUBNAME "data_ops_init: "

int data_ops_init(void) {
	int err = 0;

	PRINT_INFO(SUBNAME "entry\n");

 	init_MUTEX(&frames.sem); 
	
	if ((err = register_chrdev(0, MCEDATA_NAME, &data_fops))<=0) {
		PRINT_ERR(SUBNAME "could not register_chrdev, "
			  "err=%#x\n", err);
		return err;
	}
	frames.major = err;

	return 0;
}

int data_ops_cleanup(void)
{
	if (frames.major != 0) 
		unregister_chrdev(frames.major, MCEDATA_NAME);

	return 0;
}

