#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#ifdef BIGPHYS
# include <linux/bigphysarea.h>
#endif

#include "dsp_driver.h"
#include "data.h"
#include "mce_driver.h"
#include "memory.h"

#ifdef OPT_WATCHER
# include "data_watcher.h"
#endif


frame_buffer_t frames;



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

  This is called in interrupt context.
*/

#define SUBNAME "data_frame_address: "

int data_frame_address(u32 *dest)
{
        *dest = (u32)(frames.base_busaddr)
                + frames.incr*(frames.head_index);
	
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

  This is called in interrupt context.
*/
  

#define SUBNAME "data_frame_increment: "

int data_frame_increment()
{
	int d;

#ifdef OPT_WATCHER
	if (watcher.on)
		watcher_file((frames.head_index
			      + frames.max_index
			      - frames.tail_index)
			     % frames.max_index);
#endif
        
	wake_up_interruptible(&frames.queue);

	d = (frames.head_index + 1) % frames.max_index;
	barrier();

	if ( d == frames.tail_index) {
		frames.dropped++;
		return -1;
	}

	frames.head_index = d;
        return 0;
}

#undef SUBNAME

/* data_frame_poll
 *
 * This function can be used by data distributors to check for full
 * buffers.  It returns 0 if no data is ready, and non-zero if data is
 * available.
 */

int data_frame_poll( void )
{
	return (frames.tail_index != frames.head_index);
}




#define SUBNAME "data_frame_resize: "

int data_frame_resize(int size)
{
	if (frames.tail_index != frames.head_index) {
		PRINT_ERR(SUBNAME "can't change frame size "
			  "while buffer not empty\n");
		return -1;
	}
	if (size<=0) {
		PRINT_ERR(SUBNAME "can't change frame size "
			  "to negative number\n");
		return -2;
	}
	if (size > frames.frame_size) {
		PRINT_ERR(SUBNAME "can't change data_size "
			  "to be larger than frame_size\n");
		return -3;
	}

	frames.data_size = size;
	return 0;
}

#undef SUBNAME


#define SUBNAME "data_frame_fake_stop: "

int data_frame_fake_stop( void )
{
	u32 *frame;

	//Mark current frame filled
	frames.head_index++;
	
	//Pointer to next frame
	frame = (u32*) (frames.base +
			frames.head_index*frames.frame_size);

	//Flag as stop
	frame[0] = 1;

	//Special id for our recognition
	frame[1] = 0x33333333;

	//Mark as filled
	frames.head_index++;

	//Wake up sleepers
	wake_up_interruptible(&frames.queue);

	return 0;

}

#undef SUBNAME


#define SUBNAME "data_frame_empty_buffers: "

int data_frame_empty_buffers( void )
{
	//Fix: lock?
	frames.head_index = 0;
	frames.tail_index = 0;
	frames.partial = 0;
	return 0;
}

#undef SUBNAME


/****************************************************************************/


#define SUBNAME "data_copy_frame: "

int data_copy_frame(void* __user user_buf, void *kern_buf,
		    int count)
{
	int d;
	int count_out = 0;

	//Are buffers well defined?  Warn...
	if (  !( (user_buf!=NULL) ^ (kern_buf!=NULL) ) ) {
		PRINT_ERR(SUBNAME "number of dest'n buffers != 1 (%x | %x)\n",
			  (int)user_buf, (int)kern_buf);
		return -1;
	}

	PRINT_INFO("data_copy_frame: sleeping\n");
	if (wait_event_interruptible(frames.queue,
				     (frames.flags & FRAME_ERR) ||
				     (frames.tail_index
				      != frames.head_index)))
		return -ERESTARTSYS;

	if (frames.tail_index != frames.head_index) {

		void *frame = frames.base +
			frames.tail_index*frames.frame_size;

		count_out = frames.data_size - frames.partial;
		if (count_out > count)
			count_out = count;
		
		if (user_buf!=NULL) {
			PRINT_INFO(SUBNAME "copy_to_user %x->[%x] now\n",
				  count_out, (int)user_buf);
			count_out -= copy_to_user(user_buf,
						  frame + frames.partial,
						  count_out);
		}
		if (kern_buf!=NULL) {
			PRINT_INFO(SUBNAME "memcpy to kernel %x now\n",
				  (int)kern_buf);
			memcpy(kern_buf, frame + frames.partial, count_out);
		}
		
		//Update data source

		frames.partial += count_out;
		if (frames.partial >= frames.data_size) {
			d = (frames.tail_index + 1) % frames.max_index;
			barrier();
			frames.tail_index = d;
			frames.partial = 0;
		}
	}

	return count_out;
}

#undef SUBNAME


void data_report() {
	PRINT_IOCT("data_report: head=%x/%x , tail=%x(%x)\n",
		  frames.head_index, frames.max_index,
		  frames.tail_index, frames.partial);
	PRINT_IOCT("data_report: flags=%x\n", frames.flags);
}


#define SUBNAME "data_alloc: "

void *data_alloc(int mem_size, int data_size, int borrow)
{
	int npg = (mem_size + PAGE_SIZE-1) / PAGE_SIZE;
	caddr_t virt;
	u32 phys;

	PRINT_INFO(SUBNAME "entry\n");

	mem_size = npg * PAGE_SIZE;

#ifdef BIGPHYS	
	// Virtual address?
	virt = bigphysarea_alloc_pages(npg, 0, GFP_KERNEL);

	if (virt==NULL) {
		PRINT_ERR(SUBNAME "bigphysarea_alloc_pages failed!\n");
		return NULL;
	}

#else
	virt = kmalloc(mem_size, GFP_KERNEL);

	if (virt==NULL) {
		PRINT_ERR(SUBNAME "kmalloc failed to allocate %i bytes\n",
			  mem_size);
		return NULL;
	}
#endif

	//Store
	//frames.bigphys_base = (void*)virt;

	//Bus address?
	phys = (u32)virt_to_bus(virt);

	//Construct data buffer

	frames.base = virt;
	frames.data_size = data_size;
	frames.frame_size = (data_size + DMA_ADDR_ALIGN - 1)
		& DMA_ADDR_MASK;
	frames.max_index  = (mem_size - borrow) /
		frames.frame_size;
	
	frames.base_busaddr = (caddr_t)phys;
	frames.top_busaddr  = (caddr_t)phys + PAGE_SIZE * npg - borrow;
	frames.incr = frames.frame_size;
	
	//Debug
	PRINT_ERR(SUBNAME "buffer: base=%x + %x gives %x of size %x, borrowed=%x\n",
		  (int)frames.base, 
		  (int)(PAGE_SIZE * npg - borrow), 
		  frames.max_index,
		  (int)frames.frame_size,
		  (int)(virt+mem_size - borrow));
	
	PRINT_ERR(SUBNAME "buffer: bus %x to %x, incr %x; borrowed_bus=%x\n",
		  (int)frames.base_busaddr, 
		  (int)frames.top_busaddr, 
		  (int)frames.incr,
		  (int)(phys+mem_size - borrow));
	
	return virt + mem_size - borrow;
}

#undef SUBNAME

int data_free(void)
{

	if (frames.base != NULL) {
#ifdef BIGPHYS
		bigphysarea_free_pages(frames.base);
#else
		kfree(frames.base);
#endif
	}
	return 0;
}


int data_force_escape()
{
	frames.flags |= FRAME_ERR;
	wake_up_interruptible(&frames.queue);
	return 0;
}

int data_reset()
{
	frames.head_index = 0;
	frames.tail_index = 0;
	frames.partial = 0;
	frames.flags = 0;
	frames.dropped = 0;
	return 0;
}


/**************************************************************************
 *                                                                        *
 *      Init and cleanup                                                  *
 *                                                                        *
 **************************************************************************/

void* data_init(int mem_size, int data_size, int borrow)
{
	init_waitqueue_head(&frames.queue);

	return data_alloc(mem_size, data_size, borrow);
}


int data_cleanup()
{
	return data_free();
}

