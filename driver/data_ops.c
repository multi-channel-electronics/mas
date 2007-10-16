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

ssize_t data_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	int read_count = 0;

	if (filp->f_flags & O_NONBLOCK) {
		if (down_trylock(&frames.sem))
			return -EAGAIN;
		if (!data_frame_poll()) {
			up(&frames.sem);
			return -EAGAIN;
		}
	} else {
		if (down_interruptible(&frames.sem))
			return -ERESTARTSYS;
	}

	PRINT_INFO(SUBNAME "local sem obtained, calling data_copy\n");

	read_count = data_copy_frame(buf, NULL, count);

	up(&frames.sem);

	if (read_count<0)
		return 0;

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

