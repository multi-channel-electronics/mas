/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/mm.h>

#include "mce_options.h"

#include "dsp_driver.h"
#include "data.h"
#include "mce_driver.h"

#ifdef OPT_WATCHER
# include "data_watcher.h"
#endif


struct filp_pdata {
	int minor;
        int properties;

        // Private read pointers for DATA_LEECH secondary readers
        int leech_tail;
        int leech_partial;
        int leech_acq;
};

/**************************************************************************
 *                                                                        *
 *      Character device operations                                       *
 *                                                                        *
 **************************************************************************/


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
	struct filp_pdata *fpdata = filp->private_data;
	int card = fpdata->minor;
	frame_buffer_t *dframes = data_frames + card;
	int read_count = 0;
	int this_read = 0;

        if (fpdata->properties & DATA_LEECH) {
                //Leeches don't care about semaphores or anything.
                while (count > 0) {
                        //But they need to have the latest configuration.
                        if (fpdata->leech_acq != dframes->acq_index)
                                return -ENODATA;

                        this_read = data_peek_frame(buf, count, card,
                                                    &fpdata->leech_tail,
                                                    &fpdata->leech_partial);
                        if (this_read > 0) {
                                count -= this_read;
                                read_count += this_read;
                                continue;
                        }
                        
                        /* Buffer is empty.  O_NONBLOCK exits now, other
                           readers exit as long as *some* data has been
                           copied. */
                        if (filp->f_flags & O_NONBLOCK || read_count > 0)
                                break;

                        /* Wait for more data or change of configuration */
                        if (wait_event_interruptible(dframes->queue,
                                                     (dframes->flags & FRAME_ERR) ||
                                                     (fpdata->leech_tail
                                                      != dframes->head_index) ||
                                                     (fpdata->leech_acq
                                                      != dframes->acq_index))) {
                                return -ERESTARTSYS;
                        }
		}
                return read_count;
        }

	if (filp->f_flags & O_NONBLOCK) {
		if (down_trylock(&dframes->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&dframes->sem))
			return -ERESTARTSYS;
	}

        PRINT_INFO(card, "user demands %li with nonblock=%i\n",
		   (long) count, filp->f_flags & O_NONBLOCK);

	while (count > 0) {
		
		// Pop count bytes from frame buffer; data_copy frame
		// will return at most 1 frame of data.
		this_read = data_copy_frame(buf, NULL, count, 0, card);

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

		if (wait_event_interruptible(dframes->queue,
					      (dframes->flags & FRAME_ERR) ||
					      (dframes->tail_index
					       != dframes->head_index))) {
			read_count = -ERESTARTSYS;
			break;
		}
	}

	up(&dframes->sem);
	return read_count;
}


ssize_t data_write(struct file *filp, const char __user *buf, size_t count,
		   loff_t *f_pos)
{
	return count;
}


/* Map the DSP buffer into user space (rather than occupying limited
 * kernel space.  The driver doesn't need direct access to the buffer
 * provided we can tell the user how to get the data. */
int data_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct filp_pdata *fpdata = filp->private_data;
	frame_buffer_t *dframes = data_frames + fpdata->minor;

	// Mark memory as reserved (prevents core dump inclusion and caching)
#ifdef VM_DONTDUMP
        /* 3.0.+ kernels */
        vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
#else
        /* 2.6.x kernels */
        vma->vm_flags |= VM_IO | VM_RESERVED;
#endif

	// Do args checking on vma... start, end, prot.
        PRINT_INFO(fpdata->minor, "mapping %#lx bytes to user address %#lx\n",
		   vma->vm_end - vma->vm_start, vma->vm_start);

	//remap_pfn_range(vma, virt, phys_page, size, vma->vm_page_prot);
	remap_pfn_range(vma, vma->vm_start,
			(unsigned long)dframes->base_busaddr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
	return 0;
}


long data_ioctl(struct file *filp, unsigned int iocmd, unsigned long arg)
{
	struct filp_pdata *fpdata = filp->private_data;
	int card = fpdata->minor;
	frame_buffer_t *dframes = data_frames + card;
        int props = fpdata->properties;

	switch(iocmd) {

	case DATADEV_IOCT_RESET:
                PRINT_INFO(card, "reset\n");
                if (props & DATA_LEECH) {
                        // Leeches just get their read pointer re-queued
                        fpdata->leech_tail = dframes->head_index;
                        fpdata->leech_partial = 0;
                        fpdata->leech_acq = dframes->acq_index;
                }
		break;

	case DATADEV_IOCT_QUERY:
                PRINT_INFO(card, "query\n");
		switch (arg) {
		case QUERY_HEAD:
			return dframes->head_index;
		case QUERY_TAIL:
			return dframes->tail_index;
		case QUERY_MAX:
			return dframes->max_index;
		case QUERY_PARTIAL:
			return dframes->partial;
		case QUERY_DATASIZE:			
			return dframes->data_size;
		case QUERY_FRAMESIZE:
			return dframes->frame_size;
		case QUERY_BUFSIZE:
			return dframes->size;
                case QUERY_LTAIL:
                        return fpdata->leech_tail;
                case QUERY_LPARTIAL:
                        return fpdata->leech_partial;
                case QUERY_LVALID:
                        return (int)(fpdata->leech_acq == dframes->acq_index);
		default:
			return -1;
		}
		break;

	case DATADEV_IOCT_SET_DATASIZE:
                PRINT_INFO(card, "set data_size to %li (%#lx)\n",
			   arg, arg);
		return data_frame_resize(arg, card);

	case DATADEV_IOCT_FAKE_STOPFRAME:
                PRINT_ERR(card, "fake_stopframe initiated!\n");
		return data_frame_fake_stop(card);

	case DATADEV_IOCT_WATCH:
	case DATADEV_IOCT_WATCH_DL:
#ifdef OPT_WATCHER
                PRINT_INFO(card, "watch control\n");
		return watcher_ioctl(iocmd, arg);
#else
                PRINT_IOCT(card, "watch function; disabled!\n");
		return -1;
#endif

	case DATADEV_IOCT_EMPTY:
                if (props & DATA_LEECH) {
                        // Leeches just get their read pointer re-queued
                        fpdata->leech_tail = 0;
                        fpdata->leech_partial = 0;
                        fpdata->leech_acq = dframes->acq_index;
                        return 0;
                }
                PRINT_INFO(card, "reset data buffer\n");
		mce_error_reset(card);
		return data_frame_empty_buffers(card);

	case DATADEV_IOCT_QT_CONFIG:
                PRINT_INFO(card, "configure Quiet Transfer mode "
			   "[inform=%li]\n", arg);
		return data_qt_configure(arg, card);

	case DATADEV_IOCT_QT_ENABLE:
                PRINT_INFO(card, "enable/disable quiet Transfer mode "
			   "[on=%li]\n", arg);
		return data_qt_enable(arg, card);

        case DATADEV_IOCT_FRAME_POLL:
                // Used by mmap'ed data reader to access the buffer.
                return (data_frame_poll(card) ? dframes->tail_index*dframes->frame_size : -1);

	case DATADEV_IOCT_FRAME_CONSUME:
		return data_tail_increment(card);

	case DATADEV_IOCT_LOCK:
		return data_lock_operation(card, arg, filp);

	case DATADEV_IOCT_GET:
		return props;
		
        case DATADEV_IOCT_SET:
                // Update properties
                fpdata->properties = (int)arg;
                // Changes?
                props = fpdata->properties & ~props;
                if (props & DATA_LEECH) {
                        fpdata->leech_tail = dframes->head_index;
                        fpdata->leech_partial = 0;
                        fpdata->leech_acq = dframes->acq_index;
                }
                return 0;

	default:
                PRINT_ERR(card, "unknown command (%#x)\n", iocmd );
	}

	return 0;
}

int data_open(struct inode *inode, struct file *filp)
{
	struct filp_pdata *fpdata = kmalloc(sizeof(struct filp_pdata), GFP_KERNEL);
        PRINT_INFO(iminor(inode), "entry\n");

        memset(fpdata, 0, sizeof(*fpdata));
	fpdata->minor = iminor(inode);
        fpdata->leech_acq = -1;

	filp->private_data = fpdata;

        PRINT_INFO(fpdata->minor, "ok\n");
	return 0;
}

int data_release(struct inode *inode, struct file *filp)
{
	struct filp_pdata *fpdata = filp->private_data;
	if (fpdata != NULL) {
		data_lock_operation(fpdata->minor, LOCK_UP, filp);
		kfree(fpdata);
	}
	return 0;
}

struct file_operations data_fops = 
{
	.owner=   THIS_MODULE,
 	.unlocked_ioctl= data_ioctl,
	.mmap=    data_mmap,
	.open=    data_open, 
	.read=    data_read,
	.release= data_release,
	.write=   data_write,
};


/**************************************************************************
 *                                                                        *
 *      Init, cleanup                                                     *
 *                                                                        *
 **************************************************************************/

int data_ops_init(void)
{
	int err = 0;
	int i = 0;
        PRINT_INFO(NOCARD, "entry\n");
	
	err = register_chrdev(0, MCEDATA_NAME, &data_fops);
	if (err<0) {
                PRINT_ERR(NOCARD, "could not register_chrdev, err=%#x\n", err);
	} else {
		for(i=0; i<MAX_CARDS; i++) {
			frame_buffer_t *dframes = data_frames + i;
			dframes->major = err;
		}
		err = 0;
	}

        PRINT_INFO(NOCARD, "ok\n");
	return err;
}

int data_ops_cleanup(void)
{
        PRINT_INFO(NOCARD, "entry\n");

	if (data_frames->major != 0) 
		unregister_chrdev(data_frames->major, MCEDATA_NAME);

        PRINT_INFO(NOCARD, "ok\n");
	return 0;
}
