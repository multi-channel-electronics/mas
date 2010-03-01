#ifndef _DATA_H_
#define _DATA_H_

/*
 * MCE data frames model
 *
 * An object of type frame_buffer_t contains a pointer "base" to a
 * region of memory that holds "max_index" buffers of size
 * "frame_size".  The bus address of the region is "base_busaddr".  In
 * each frame, the data is expected to occupy only "data_size" bytes.
 *
 * The circular buffer is implemented by having a read and write
 * index, called head_index and tail_index respectively.  When head is
 * equal to tail, there is no un-read data in the buffer.  When head
 * is not equal to tail, there is data to be consumed by the reader.
 * 
 * Once the buffer pointed to by head is filled, head_index is
 * incremented.  If head_index becomes equal to max_index, head_index
 * is instead set to 0.  If the new value of head_index would be equal
 * to the value of read_index, then read_index must also be
 * incremented.
 *
 * The data at tail_index can be processed only if tail_index !=
 * head_index.  Once the buffer has been read, tail_index shall be
 * incremented.
 * 
 * The partial data member allows parts of the buffer at tail_index to
 * be consumed.  Readers that consume part of the buffer at tail_index
 * should increment partial and not increment tail_index.  Once the
 * buffer has been totally consumed, tail_index should be incremented
 * and partial should be reset to 0.
 */

#include <linux/interrupt.h>

#include "kversion.h"
#include "mce/data_ioctl.h"
#include "data_qt.h"


#define DATAMODE_CLASSIC 0
#define DATAMODE_QUIET   1

typedef struct {

	// Buffer addresses are, for 0 <= i < max_index,
        //  addr[i] = base + frame_size*i
	// But each buffer contains only data_size bytes of real data.

	void*     base;
	u32       size;

	int       frame_size;
	int       data_size;
        int       max_index;

        // New data is written at head, consumer data is read at tail.
	volatile
	int       head_index;
	volatile
	int       tail_index;

	// Data mode of the DSP - DATAMODE_*
	unsigned  data_mode;

	// If frame at tail_index has been only partially consumed, 
	//  partial will indicate the current index into the buffer.
	int       partial;

	// Low level equivalent information
	unsigned long base_busaddr;
	
	// Semaphore should be held when modifying structure, but
	// interrupt routines may modify head_index at any time.

	struct semaphore sem;

	// Device lock - controls read access on data device and
	// provides a system for checking whether the system is
	// mid-acquisition.  Should be NULL (for idle) or pointer to
        // reader's filp (for locking).

	void *data_lock;	

        // Tasklet for updating grants on card.
	int task_pending;
 	struct tasklet_struct grant_tasklet;
	int grant_sched_count;

	int dropped;

	int flags;
#define     FRAME_ERR 0x1
	wait_queue_head_t queue;

	int major;

} frame_buffer_t;

extern frame_buffer_t data_frames[MAX_CARDS];

/* Alternative model: keep track of total index, not circular index.
 * Then a linked list of readers can be installed, each doing whatever
 * it wants with the data whenever it feels like it.  One reader can
 * be identified as priveleged and warned if it begins to fall behind.
 */


/* Prototypes */

int data_probe(int dsp_version, int card, int mem_size, int data_size);
int data_init(int mem_size, int data_size);
int data_remove(int card);


int data_force_escape( void );
int data_reset(int card);

int  data_frame_address(u32 *dest, int card);
int  data_frame_increment(int card);
int  data_frame_contribute(int count, int card);
int  data_frame_divide(int new_data_size, int card);

int data_copy_frame(void* __user user_buf, void *kern_buf,
		    int count, int nonblock, int card);
int data_frame_fake_stop(int card);

int data_frame_resize(int size, int card);

int data_frame_empty_buffers(int card);

int data_frame_poll(int card);

int data_tail_increment(int card);

int data_lock_operation(int card, int operation, void *filp);

int data_proc(char *buf, int count, int card);

#endif
