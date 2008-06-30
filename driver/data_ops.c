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


typedef struct {
	unsigned minor;
	u32 *user_map;
} data_ops_t;


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
	data_ops_t* d = filp->private_data;
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
		} else if (this_read > 0) {
			// More reading, more reading...
			read_count += this_read;
			count -= this_read;
			continue;
		}

		/* Buffer is empty.  O_NONBLOCK exits now, other
		   readers exit as long as *some* data has been
		   copied. */
		if (filp->f_flags & O_NONBLOCK || read_count > 0)
			break;
		
		if (wait_event_interruptible(frames.queue,
					     (frames.flags & FRAME_ERR) ||
					     (frames.tail_index
					      != frames.head_index))) {
			read_count = -ERESTARTSYS;
			break;
		}
	}

	up(&frames.sem);
	return read_count;
}

#undef SUBNAME


ssize_t data_write(struct file *filp, const char __user *buf, size_t count,
		   loff_t *f_pos)
{
	return count;
}


#define SUBNAME "data_mmap: "

/* Map the DSP buffer into user space (rather than occupying limited
 * kernel space.  The driver doesn't need direct access to the buffer
 * provided we can tell the user how to get the data. */

int data_mmap(struct file *filp, struct vm_area_struct *vma)
{
	data_ops_t* d = filp->private_data;

	// Do args checking on vma... start, end, prot.
	PRINT_ERR(SUBNAME "mapping %#lx to user virtual %#lx\n",
		  vma->vm_start, vma->vm_end - vma->vm_start);

	//remap_pfn_range(vma, virt, phys_page, size, vma->vm_page_prot);
	remap_pfn_range(vma, vma->vm_start,
			(unsigned long)frames.base_busaddr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);

	d->user_map = (u32*)vma->vm_start;

	return 0;
}

#undef SUBNAME


#define SUBNAME "data_ioctl: "

int data_ioctl(struct inode *inode, struct file *filp,
	       unsigned int iocmd, unsigned long arg)
{
	switch(iocmd) {

	case DATADEV_IOCT_RESET:
		PRINT_INFO(SUBNAME"reset\n");
		break;

	case DATADEV_IOCT_QUERY:
		PRINT_INFO(SUBNAME "query\n");
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
		case QUERY_BUFSIZE:
			if (frames.base == NULL) return 0;
			return frames.size;
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
		PRINT_INFO(SUBNAME "watch control\n");
		return watcher_ioctl(iocmd, arg);
#else
		PRINT_IOCT(SUBNAME "watch function; disabled!\n");
		return -1;
#endif

	case DATADEV_IOCT_EMPTY:
		PRINT_INFO(SUBNAME "reset data buffer\n");
		mce_error_reset();
		return data_frame_empty_buffers();

	case DATADEV_IOCT_QT_CONFIG:
		PRINT_INFO(SUBNAME "configure Quiet Transfer mode "
			   "[inform=%li]\n", arg);
		return data_qt_configure(arg);

	case DATADEV_IOCT_QT_ENABLE:
		PRINT_INFO(SUBNAME "enable/disable quiet Transfer mode "
			   "[on=%li]\n", arg);
		return data_qt_enable(arg);

	case DATADEV_IOCT_FRAME_POLL:
		return (data_frame_poll() ? frames.tail_index*frames.frame_size : -1);

	case DATADEV_IOCT_FRAME_CONSUME:
		return data_tail_increment();

	default:
		PRINT_ERR(SUBNAME "unknown command (%#x)\n", iocmd );
	}

	return 0;
}

#undef SUBNAME

int data_open(struct inode *inode, struct file *filp)
{
	// Store details of inode in private data
	data_ops_t* d = kmalloc(sizeof(data_ops_t), GFP_KERNEL);
	d->minor = iminor(inode);
	d->user_map = NULL;

	filp->private_data = d;

	return 0;
}

int data_release(struct inode *inode, struct file *filp)
{
	if (filp->private_data != NULL) kfree(filp->private_data);
	return 0;
}

struct file_operations data_fops = 
{
	.owner=   THIS_MODULE,
 	.ioctl=   data_ioctl,
	.mmap=    data_mmap,
	.open=    data_open, 
	.read=    data_read,
	.release= data_release,
	.write=   data_write,
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

