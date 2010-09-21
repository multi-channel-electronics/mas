#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#ifdef BIGPHYS
# include <linux/bigphysarea.h>
#endif

#include "dsp_driver.h"
#include "data.h"
#include "data_ops.h"
#include "data_qt.h"
#include "mce_driver.h"
#include "memory.h"

#ifdef OPT_WATCHER
# include "data_watcher.h"
#endif

frame_buffer_t data_frames[MAX_CARDS];


/**************************************************************************
 *                                                                        *
 *      Buffer management and interrupt service                           *
 *                                                                        *
 **************************************************************************/

/*
  data_frame_address

  This function is called by whatever mechanism is passing buffer
  addresses to the DSP card.  It puts the current head_index buffer
  address into the argument, and does not change the state of the
  indices.

  Return value is 0 if dest represents an ok place to dump data, or
  negative otherwise.

  In this implementation of the buffer, there is always a place to
  write data, but this packet may be lost if the head_index cannot be
  incremented because that would make it equal to tail_index.  So the
  "buffer full" error is actually returned by data_frame_increment.

  This is called in interrupt context.  It is not used in quiet
  transfer mode.
*/

#define SUBNAME "data_frame_address: "
int data_frame_address(u32 *dest, int card)
{
	frame_buffer_t *dframes = data_frames + card;

        *dest = (u32)(unsigned long)(dframes->base_busaddr)
                + dframes->frame_size*(dframes->head_index);
	
        return 0;
}
#undef SUBNAME


/*
  data_frame_increment

  This function is called by whatever mechanism records the successful
  upload of a frame from the DSP card.  It will attempt to increment
  the head_index, effectively marking the buffer as complete and
  valid.  This will not be possible if the incrementation would make
  the head_index equal to the tail_index, since that is the indicator
  of an empty buffer.

  On success the function returns 0.  If the buffer is full and
  head_index has not been incremented, -1 is returned and the data
  will be overwritten by the next frame.

  This is called in interrupt context.  It is not used in quiet
  transfer mode.
*/
  

#define SUBNAME "data_frame_increment: "
int data_frame_increment(int card)
{
	frame_buffer_t *dframes = data_frames + card;
	int d;

#ifdef OPT_WATCHER
	if (watcher.on)
		watcher_file((dframes->head_index
			      + dframes->max_index
			      - dframes->tail_index)
			     % dframes->max_index);
#endif
        
	d = (dframes->head_index + 1) % dframes->max_index;
	wake_up_interruptible(&dframes->queue);

	if ( d == dframes->tail_index) {
		dframes->dropped++;
		return -1;
	}

	dframes->head_index = d;
        return 0;
}
#undef SUBNAME


/* Quiet transfer mode buffer update 
 *
 * data_frame_contribute is called by the interrupt service routine to
 * update the head of the circular buffer.  The buffers between head
 * and new_head must already have been updated with new data.  At the
 * end of this function any threads waiting for frame data will be
 * awoken and the tasklet that updates the buffer status on the PCI
 * card is scheduled.
 */


#define SUBNAME "data_frame_contribute: "
int data_frame_contribute(int new_head, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	int d;

#ifdef OPT_WATCHER
	if (watcher.on)
		watcher_file((dframes->head_index
			      + dframes->max_index
			      - dframes->tail_index)
			     % dframes->max_index);
#endif
        
	// Ensure that new_head >= head >= tail
	//   or        head >= tail > new_head
        //   or        tail > new_head >= head

	d = 
		(new_head >= dframes->head_index) +
		(dframes->head_index >= dframes->tail_index) +
		(dframes->tail_index > new_head);
	
	if (d != 2) {
		PRINT_ERR(SUBNAME "buffer trashed!\n");
		dframes->head_index = new_head;
		dframes->tail_index = (new_head+1) % dframes->max_index;
	} else {
		dframes->head_index = new_head;
	}
	dsp_request_grant(card, dframes->tail_index);

	wake_up_interruptible(&dframes->queue);

        return 0;
}
#undef SUBNAME

/* data_frame_poll
 *
 * This function can be used by data distributors to check for full
 * buffers.  It returns 0 if no data is ready, and non-zero if data is
 * available.
 */

int data_frame_poll(int card)
{
	frame_buffer_t *dframes = data_frames + card;

	return (dframes->tail_index != dframes->head_index);
}


#define SUBNAME "data_frame_resize: "
int data_frame_resize(int size, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	PRINT_INFO(SUBNAME "entry\n");
	PRINT_INFO(SUBNAME "size: %d card: %d \n", size, card);

	if (size == dframes->data_size)
		return 0;
	
	if (dframes->tail_index != dframes->head_index) {
		PRINT_ERR(SUBNAME "can't change frame size "
			  "while buffer not empty\n");
		return -1;
	}
	if (size<=0) {
		PRINT_ERR(SUBNAME "can't change frame size "
			  "to non-positive number\n");
		return -2;
	}

	if (data_frame_divide(size, card)) {
		PRINT_ERR(SUBNAME "failed to divide the buffer by %#x\n", size);
		return -3;
	}
	
	if (dframes->data_mode == DATAMODE_QUIET &&
	    data_qt_configure(1, card)!=0) {
		PRINT_ERR(SUBNAME "can't set DSP quiet mode frame size\n");
		return -4;
	}

	return 0;
}
#undef SUBNAME


#define SUBNAME "data_frame_fake_stop: "
int data_frame_fake_stop(int card)
{
	frame_buffer_t *dframes = data_frames + card;
	u32 *frame;

	//Mark current frame filled
	dframes->head_index++;
	
	//Pointer to next frame
	frame = (u32*) (dframes->base +
			dframes->head_index*dframes->frame_size);

	//Flag as stop
	frame[0] = 1;

	//Special id for our recognition
	frame[1] = 0x33333333;

	//Mark as filled
	dframes->head_index++;

	//Wake up sleepers
	wake_up_interruptible(&dframes->queue);

	return 0;

}
#undef SUBNAME


#define SUBNAME "data_frame_empty_buffers: "
int data_frame_empty_buffers(int card)
{
	frame_buffer_t *dframes = data_frames + card;
	//Fix: lock?
	dframes->head_index = 0;
	dframes->tail_index = 0;
	dframes->partial = 0;
	return 0;
}
#undef SUBNAME

#define SUBNAME "data_frame_divide: "
int data_frame_divide( int new_data_size, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	PRINT_INFO(SUBNAME "entry\n");
	PRINT_INFO(SUBNAME "new_data_size: %d\n", new_data_size);

	// Recompute the division of the buffer into frames
	if (new_data_size >= 0) dframes->data_size = new_data_size;

	// Round the frame size to a size convenient for DMA
	dframes->frame_size =
		(dframes->data_size + DMA_ADDR_ALIGN - 1) & DMA_ADDR_MASK;
	PRINT_ERR(SUBNAME "size: %d frame_size: %d\n", dframes->size, dframes->frame_size);
	dframes->max_index = dframes->size / dframes->frame_size;

	if (dframes->max_index <= 1) {
		PRINT_ERR("data_frame_divide: buffer can only hold %i data packet!\n",
			  dframes->max_index);
		return -1;
	}

	PRINT_INFO(SUBNAME "ok\n");
	return 0;
}
#undef SUBNAME

/****************************************************************************/



/* data_copy_frame - copy up to one complete frame into buffer
 *
 * This is the primary exporter of buffered frame data; this
 * effectively pops a frame or part of a frame from the circular
 * buffer, freeing the space.  The frames semaphore should be held
 * when calling this routine.  This routine is not re-entrant.
 */

#define SUBNAME "data_copy_frame: "
int data_copy_frame(void* __user user_buf, void *kern_buf,
		    int count, int nonblock, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	void *source;
	int count_out = 0;
	int this_read;

	// Are buffers well defined?  Warn...
	if (  !( (user_buf!=NULL) ^ (kern_buf!=NULL) ) ) {
		PRINT_ERR(SUBNAME "number of dest'n buffers != 1 (%lx | %lx)\n",
			  (unsigned long)user_buf, (unsigned long)kern_buf);
		return -1;
	}

	// Exit once supply runs out or demand is satisfied.
	while ((dframes->tail_index != dframes->head_index) && count > 0) {

		source = dframes->base + dframes->tail_index*dframes->frame_size + dframes->partial;

		// Don't read past end of frame.
		this_read = (dframes->data_size - dframes->partial < count) ?
			dframes->data_size - dframes->partial : count;
		
		if (user_buf!=NULL) {
			PRINT_INFO(SUBNAME "copy_to_user %x->[%lx] now\n",
				   count, (unsigned long)user_buf);
			this_read -= copy_to_user(user_buf, source, this_read);
		}
		if (kern_buf!=NULL) {
			PRINT_INFO(SUBNAME "memcpy to kernel %lx now\n",
				   (unsigned long)kern_buf);
			memcpy(kern_buf, source, this_read);
		}

		// Update demand
		count -= this_read;
		count_out += this_read;
	
		// Update supply
		dframes->partial += this_read;
		if (dframes->partial >= dframes->data_size) {
			unsigned d = (dframes->tail_index + 1) % dframes->max_index;
			barrier();
			dframes->tail_index = d;
			dframes->partial = 0;
		}
	}

	return count_out;
}
#undef SUBNAME


/* Call tail_increment to mark a frame as consumed. */
#define SUBNAME "data_head_increment: "
int data_tail_increment(int card)
{
	frame_buffer_t *dframes = data_frames + card;
	unsigned d = (dframes->tail_index + 1) % dframes->max_index;
	if (dframes->head_index == dframes->tail_index)
		return -1;
	barrier();
	dframes->tail_index = d;
	dframes->partial = 0;
	return 0;
}
#undef SUBNAME


#define SUBNAME "data_alloc: "
int data_alloc(int mem_size, int data_size, int card)
{
	frame_buffer_t *dframes = data_frames + card;
	int npg = (mem_size + PAGE_SIZE-1) / PAGE_SIZE;
	caddr_t virt;
#ifndef BIGPHYS
	unsigned long phys;               // Not needed / used with bigphys
#endif

	PRINT_INFO(SUBNAME "entry\n");

	mem_size = npg * PAGE_SIZE;

#ifdef BIGPHYS	
	
        // Virtual address?
	virt = bigphysarea_alloc_pages(npg, 0, GFP_KERNEL);
	PRINT_ERR(SUBNAME "BIGPHYS selected\n");

	if (virt==NULL) {
		PRINT_ERR(SUBNAME "bigphysarea_alloc_pages failed!\n");
		return -ENOMEM;
	}

	// Save the buffer address
	dframes->base = virt;
	
	// Save physical address for hardware

	// Note virt_to_bus is on the deprecation list... we will want
	// to switch to the DMA-API, dma_map_single does what we want.
	dframes->base_busaddr = virt_to_bus(virt);

#else

#ifdef MEMMAP 

#ifdef CONFIG_HIGHMEM

	phys = (caddr_t) (num_physpages * PAGE_SIZE);
	PRINT_ERR(SUBNAME "highmem; phys=%lx\n", (unsigned long)phys);
#else
	phys = (caddr_t)__pa( high_memory );
	PRINT_ERR(SUBNAME "!highmem; phys=%lx\n", (unsigned long)phys);
#endif
	// Save the buffer address
	dframes->base = NULL;

	// Save physical address for hardware
	dframes->base_busaddr = phys;

#else /*!MEMMAP*/

	PRINT_ERR(SUBNAME "kernel DMA selected\n");

	virt  = dsp_allocate_dma(mem_size, &phys);

	if (virt==NULL) {
		PRINT_ERR(SUBNAME "dsp_allocate_dma failed to allocate %i bytes\n",
			  mem_size);
		return -ENOMEM;
	}

	// Save physical address for hardware
	dframes->base_busaddr = phys;
	dframes->base = &virt;

#endif /* MEMMAP */

#endif /* BIG PHYS */

	// Save the maximum size
	dframes->size = mem_size;
 
	// Partition buffer into blocks of some default size
	data_frame_divide(data_size, card);

	//Debug
	PRINT_INFO(SUBNAME "buffer: base=%lx + %x of size %x\n",
		   (unsigned long)dframes->base, 
		   dframes->max_index,
		   dframes->frame_size);
	
	return 0;
}
#undef SUBNAME

#define SUBNAME "dma_free: "
int data_free(int card)
{
	frame_buffer_t *dframes = data_frames + card;

	if (dframes->base != NULL) {
#ifdef BIGPHYS
		PRINT_ERR(SUBNAME "freeing BIGPHYS\n");
		bigphysarea_free_pages(dframes->base);
		return 0;
#else
#ifndef MEMMAP
		PRINT_ERR(SUBNAME "freeing kernel DMA\n");
		dsp_free_dma(dframes->base, dframes->size, 
			     dframes->base_busaddr);
		dframes->size = 0;
		return 0;
#endif /* CONFIG_HIGH MEM */

#endif /* BIGPHYS */
	}
	
	PRINT_ERR(SUBNAME "freeing MEMMAP\n");
	return 0;
}
#undef SUBNAME

#define SUBNAME "data_alloc: "
int data_reset(int card)
{
	frame_buffer_t *dframes = data_frames + card;

	dframes->head_index = 0;
	dframes->tail_index = 0;
	dframes->partial = 0;
	dframes->flags = 0;
	dframes->dropped = 0;
	
	if (dframes->data_mode == DATAMODE_QUIET) {
		if (mce_qt_command(DSP_QT_TAIL  , dframes->tail_index, 0, card) ||
		    mce_qt_command(DSP_QT_HEAD  , dframes->head_index, 0, card) ) {
			PRINT_ERR(SUBNAME
				  "Could not reset DSP QT indexes; disabling.");
			data_qt_enable(0, card);
		}
	}

	return 0;
}
#undef SUBNAME


/* Data device locking support */

#define SUBNAME "data_lock_operation: "

int data_lock_operation(int card, int operation, void *filp)
{
	frame_buffer_t *dframes = data_frames + card;
	int ret_val = 0;

	switch (operation) {
	case LOCK_QUERY:
		return dframes->data_lock != NULL;

	case LOCK_RESET:
		dframes->data_lock = NULL;
		return 0;

	case LOCK_DOWN:
	case LOCK_UP:
		/* 'Up' and 'down' block for dframes semaphore! */
		if (down_interruptible(&dframes->sem))
			return -ERESTARTSYS;
		if (operation == LOCK_DOWN) {
			if (dframes->data_lock != NULL) {
				ret_val = -EBUSY;
			} else {
				dframes->data_lock = filp;
			}
		} else /* LOCK_UP */ {
			if (dframes->data_lock == filp) {
				dframes->data_lock = NULL;
			}
		}
		up(&dframes->sem);
		return ret_val;
	}
	PRINT_ERR(SUBNAME "unknown operation (%i).\n", operation);
	return -1;
}

#undef SUBNAME

int data_lock_down(int card, void *filp)
{
	int ret_val = 0;
	frame_buffer_t *dframes = data_frames + card;
	if (down_interruptible(&dframes->sem))
		return -ERESTARTSYS;
	if (dframes->data_lock != NULL) {
		ret_val = -EBUSY;
	} else {
		dframes->data_lock = filp;
	}
	up(&dframes->sem);
	return ret_val;
}

int data_lock_up(int card, void *filp)
{
	int ret_val = 0;
	frame_buffer_t *dframes = data_frames + card;
	if (down_interruptible(&dframes->sem))
		return -ERESTARTSYS;
	if (dframes->data_lock == filp) {
		dframes->data_lock = NULL;
	}
	up(&dframes->sem);
	return ret_val;
}


/* Proc! */

int data_proc(char *buf, int count, int card)
{
	frame_buffer_t *dframes = data_frames + card;

	int len = 0;
	if (dframes->size <= 0)
		return 0;
	if (len < count) {
		if (sizeof(long) > 4) {
			len += sprintf(buf+len, "    %-22s %#018lx\n",
				       "virtual:", (unsigned long)dframes->base);
			len += sprintf(buf+len, "    %-22s %#018lx\n",
				       "bus:", (unsigned long)dframes->base_busaddr);
		} else {
			len += sprintf(buf+len, "    %-30s %#010lx\n",
				       "virtual:", (unsigned long)dframes->base);
			len += sprintf(buf+len, "    %-30s %#010lx\n",
				       "bus:", (unsigned long)dframes->base_busaddr);
		}
	}
	if (len < count)
		len += sprintf(buf+len, "    %-15s %25i\n", "count:", dframes->max_index);
	if (len < count)
		len += sprintf(buf+len, "    %-15s %25i\n", "head:", dframes->head_index);
	if (len < count)
		len += sprintf(buf+len, "    %-15s %25i\n", "tail:", dframes->tail_index);
	if (len < count)
		len += sprintf(buf+len, "    %-15s %25i\n", "drops:", dframes->dropped);
	if (len < count)
		len += sprintf(buf+len, "    %-15s %#25x\n", "size:", dframes->frame_size);
	if (len < count)
		len += sprintf(buf+len, "    %-15s %#25x\n", "data:", dframes->data_size);
	if (len < count) {
		char sstr[64];
		switch (dframes->data_mode) {
		case DATAMODE_CLASSIC:
			strcpy(sstr, "classic notify");
			break;
		case DATAMODE_QUIET:
			strcpy(sstr, "quiet mode");
			break;
		}
		len += sprintf(buf+len, "    %-15s %25s\n", "mode:", sstr);
	}

	return len;
}


/**************************************************************************
 *                                                                        *
 *      Probe, Init, Remove and Cleanup                                   *
 *                                                                        *
 **************************************************************************/


#define SUBNAME "data_probe: "
int data_probe(int dsp_version, int card, int mem_size, int data_size)
{
	frame_buffer_t *dframes = data_frames + card;
	int err = 0;

	init_waitqueue_head(&dframes->queue);

	err = data_alloc(mem_size, data_size, card);
	if (err != 0) return err;

	err = data_ops_probe(card);
	if (err != 0) return err;

	data_reset(card);

	switch (dsp_version) {
	case 0:
		PRINT_ERR(SUBNAME
			  "DSP code is old, you'll get checksum errors.\n");
		break;

	case DSP_U0103:
		PRINT_ERR(SUBNAME "DSP code wants to be upgraded to U0104!\n");
		break;
		
	case DSP_U0104:
	case DSP_U0105:
		if (data_qt_configure(1, card))
			return -EIO;
		break;
		
	default:
		PRINT_ERR(SUBNAME
			  "DSP code not recognized, attempting quiet transfer mode...\n");
		if (data_qt_configure(1, card))
			return -EIO;
		break;
		}

	return 0;
}
#undef SUBNAME

int data_remove(int card)
{
	frame_buffer_t *dframes = data_frames + card;

	if(dframes->data_mode != DATAMODE_CLASSIC)
		data_qt_enable(0, card);

	return data_free(card);
}

