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

#define DSP_INFORM_RATE    10 /* Hz */
#define DSP_INFORM_COUNTS  (50000000 / DSP_INFORM_RATE) 


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

/*

  Quiet Transfer Mode support

  Information interrupts from DSP are directed to
  data_frame_contribute, which checks for consistency of the informed
  value and then updates tail_index in the driver.

  Following information, a tasklet is scheduled to update the head
  index on the DSP.

  QT configuration is achieved with calls to data_qt_setup and
  data_qt_enable.

*/


#define SUBNAME "data_frame_contribute: "

int data_frame_contribute(int new_head)
{
	int d;

#ifdef OPT_WATCHER
	if (watcher.on)
		watcher_file((frames.head_index
			      + frames.max_index
			      - frames.tail_index)
			     % frames.max_index);
#endif
        
	// Ensure that new_head >= head >= tail
	//   or        head >= tail > new_head
        //   or        tail > new_head >= head

	d = 
		(new_head >= frames.head_index) +
		(frames.head_index >= frames.tail_index) +
		(frames.tail_index > new_head);
	
	if (d != 2) {
		PRINT_ERR(SUBNAME "buffer trashed!\n");
		frames.head_index = new_head;
		frames.tail_index = (new_head+1) % frames.max_index;
	} else {
		frames.head_index = new_head;
	}

	tasklet_schedule(&frames.grant_tasklet);

	wake_up_interruptible(&frames.queue);

        return 0;
}

#undef SUBNAME


#define SUBNAME "data_grant_callback: "

int  data_grant_callback( int error, dsp_message *msg )
{
	if (error != 0 || msg==NULL) {
		PRINT_ERR(SUBNAME "error or NULL message.\n");
	}
	return 0;		
}

#undef SUBNAME


#define SUBNAME "data_grant_task: "

void data_grant_task(unsigned long arg)
{
	int err;

	dsp_command cmd = { DSP_QTS, { DSP_QT_TAIL, frames.tail_index, 0 } };

	PRINT_INFO(SUBNAME "trying update to tail=%i\n", frames.tail_index);

	if ( (err=dsp_send_command( &cmd, data_grant_callback )) ) {
		// FIX ME: discriminate between would-block errors and fatals!
		PRINT_INFO(SUBNAME "dsp busy; rescheduling.\n");
		tasklet_schedule(&frames.grant_tasklet);
		return;
	}
	

}

#undef SUBNAME

int data_qt_cmd( dsp_qt_code code, int arg1, int arg2)
{
	dsp_command cmd = { DSP_QTS, {code,arg1,arg2} };
	dsp_message reply;
	return dsp_send_command_wait( &cmd, &reply );
}	


#define SUBNAME "data_qt_enable: "

int data_qt_enable(int on)
{
	int err = data_qt_cmd(DSP_QT_ENABLE, on, 0);
	if (!err)
		frames.data_mode = (on ? DATAMODE_QUIET : DATAMODE_CLASSIC);
	return err;
}

#undef SUBNAME


#define SUBNAME "data_qt_setup: "

int data_qt_configure( int qt_interval )
{
	int err = 0;
	PRINT_INFO(SUBNAME "entry\n");

	if ( data_qt_enable(0) || data_reset() )
		err = -1;

	if (!err)
		err = data_qt_cmd(DSP_QT_DELTA , frames.frame_size, 0);
	
	if (!err)
		err = data_qt_cmd(DSP_QT_NUMBER, frames.max_index, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_INFORM, qt_interval, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_PERIOD, DSP_INFORM_COUNTS, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_SIZE  , frames.data_size, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_TAIL  , frames.tail_index, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_HEAD  , frames.head_index, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_DROPS , 0, 0);

	if (!err)
		err = data_qt_cmd(DSP_QT_BASE,
				  ((long)frames.base_busaddr      ) & 0xffff,
				  ((long)frames.base_busaddr >> 16) & 0xffff);
	
	return err;
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
	if (size == frames.data_size)
		return 0;
	
	if (frames.tail_index != frames.head_index) {
		PRINT_ERR(SUBNAME "can't change frame size "
			  "while buffer not empty\n");
		return -1;
	}
	if (size<=0) {
		PRINT_ERR(SUBNAME "can't change frame size "
			  "to non-positive number\n");
		return -2;
	}
	if (size > frames.frame_size) {
		PRINT_ERR(SUBNAME "can't change data_size "
			  "to be larger than frame_size\n");
		return -3;
	}

	if (frames.data_mode == DATAMODE_QUIET &&
	    data_qt_cmd(DSP_QT_SIZE, size, 0)!=0) {
		PRINT_ERR(SUBNAME "can't set DSP quiet mode frame size\n");
		return -4;
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


#define SUBNAME "data_alloc: "

int data_reset()
{
	frames.head_index = 0;
	frames.tail_index = 0;
	frames.partial = 0;
	frames.flags = 0;
	frames.dropped = 0;
	
	if (frames.data_mode == DATAMODE_QUIET) {
		if (data_qt_cmd(DSP_QT_TAIL  , frames.tail_index, 0) ||
		    data_qt_cmd(DSP_QT_HEAD  , frames.head_index, 0) ) {
			PRINT_ERR(SUBNAME
				  "Could not reset DSP QT indexes; disabling.");
			data_qt_enable(0);
		}
	}

	return 0;
}

#undef SUBNAME


int data_proc(char *buf, int count)
{
	int len = 0;
	if (len < count)
		len += sprintf(buf+len, "    virtual:  %#010x\n",
			       (unsigned)frames.base);
	if (len < count)
		len += sprintf(buf+len, "    bus:      %#010x\n",
			       (unsigned)frames.base_busaddr);
	if (len < count)
		len += sprintf(buf+len, "    count:    %10i\n", frames.max_index);
	if (len < count)
		len += sprintf(buf+len, "    head:     %10i\n", frames.head_index);
	if (len < count)
		len += sprintf(buf+len, "    tail:     %10i\n", frames.tail_index);
	if (len < count)
		len += sprintf(buf+len, "    drops:    %10i\n", frames.dropped);
	if (len < count)
		len += sprintf(buf+len, "    size:     %#10x\n", frames.frame_size);
	if (len < count)
		len += sprintf(buf+len, "    data:     %#10x\n", frames.data_size);
	if (len < count) {
		len += sprintf(buf+len, "    mode:     ");
		switch (frames.data_mode) {
		case DATAMODE_CLASSIC:
			len += sprintf(buf+len, "classic notify\n");
			break;
		case DATAMODE_QUIET:
			len += sprintf(buf+len, "quiet mode\n");
			break;
		}
	}

	return len;
}


/**************************************************************************
 *                                                                        *
 *      Init and cleanup                                                  *
 *                                                                        *
 **************************************************************************/

void* data_init(int mem_size, int data_size, int borrow)
{
	init_waitqueue_head(&frames.queue);

	tasklet_init(&frames.grant_tasklet,
		     data_grant_task, 0);

	data_reset();

	return data_alloc(mem_size, data_size, borrow);
}


int data_cleanup()
{
	tasklet_kill(&frames.grant_tasklet);
	return data_free();
}

