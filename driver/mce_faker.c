#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>

#include "driver.h"

#define MAX_FERR 100

#define FAKE_CMD_DELAY ((999 + HZ) / 1000)

typedef enum {
	MDAT_IDLE = 0,
	MDAT_CON,
	MDAT_ERR
} fake_state_t;

static struct mce_control {
	
	mce_command cmd;
	mce_reply rep;
	mce_callback callback;
 	struct semaphore sem;
	struct timer_list timer;

	int initialized;

  	spinlock_t state_lock;
	fake_state_t state;

	int ferror_count;
	int data_flags;

	mce_emu_t *mce_emu;
} mce_dat[MAX_CARDS];


static mce_interface_t fake_mce;

int fake_error_register(int card)
{
 	struct mce_control *mdat = mce_dat + card;

	if (mdat->ferror_count == MAX_FERR)
		PRINT_ERR("no further frame errors will be logged.\n");
						 
	return (mdat->ferror_count++ < MAX_FERR);
}

void fake_error_reset( int card )
{
 	struct mce_control *mdat = mce_dat + card;

	mdat->ferror_count = 0;
}


/*
 *   Fake MCE commanding is delightfully simple.
 *   When changing mdat->state, get mdat->state_lock.
 */

#define MDAT_LOCK    spin_lock_irqsave(&mdat->state_lock, irqflags)
#define MDAT_UNLOCK  spin_unlock_irqrestore(&mdat->state_lock, irqflags)

/* First set: interrupt context, no blocking and no sems! */

/* Generic error handler; reports error to caller and goes to IDLE state.
   Call while holding state_lock.
 */


int fake_command_do_callback( int error, mce_reply *rep, int card)
{
	/* spinlock is assumed held! */
 	struct mce_control *mdat = mce_dat + card;

	mdat->state = MDAT_IDLE;

	if ( mdat->callback != NULL ) {
		mdat->callback(error, rep, card);
	} else {
		PRINT_INFO("no callback specified\n");
	} 
	
	// Clear the buffer for the next reply
	memset(&mdat->rep, 0, sizeof(mdat->rep));
	mdat->callback = NULL;

	return 0;
}


int fake_qt_command( dsp_qt_code code, int arg1, int arg2, int card)
{
	/* QT config! */

	return -1;
}	


void fake_command_timer(unsigned long data)
{
	unsigned long irqflags;
	struct mce_control *mdat = (struct mce_control *)data;
	int card = mdat - mce_dat;
	mce_callback callback;
	mce_reply rep;

	MDAT_LOCK;
	memcpy(&rep, &mdat->rep, sizeof(mdat->rep));
	callback = mdat->callback;
	mdat->state = MDAT_IDLE;
	MDAT_UNLOCK;
	if (callback != NULL)
		callback(0, &rep, card);
}


int fake_send_command(mce_command *cmd, mce_callback callback, int non_block, int card)
{
	unsigned long irqflags;
 	struct mce_control *mdat = mce_dat + card;
	int ret_val = 0;
	
	if (non_block) {
		if (down_trylock(&mdat->sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(&mdat->sem))
			return -ERESTARTSYS;
	}

	// Protect mdat->state ; exit with up_and_out from now on.
	MDAT_LOCK;
	if (mdat->state != MDAT_IDLE) {
		PRINT_INFO("transaction in progress (state=%i)\n", mdat->state);
		ret_val = -MCE_ERR_ACTIVE;
		goto up_and_out;
	}
	
	// Copy the command and initiate
	memcpy(&mdat->cmd, cmd, sizeof(*cmd));
	mdat->callback = callback;
	mdat->state = MDAT_CON;
	// Fake a reply
	mce_fake_command(mdat->mce_emu, &mdat->cmd, &mdat->rep);
	// Fake a delay
	mod_timer(&mdat->timer, jiffies + FAKE_CMD_DELAY);

up_and_out:
	MDAT_UNLOCK;
	up(&mdat->sem);
	return ret_val;
}


int fake_proc(char *buf, int count, int card)
{
 	struct mce_control *mdat = mce_dat + card;
	int len = 0;
	if (!mdat->initialized)
		return len;
	if (len < count) {
		char sstr[64];
		switch (mdat->state) {
		case MDAT_IDLE:
			strcpy(sstr, "idle");
			break;
		case MDAT_CON:
			strcpy(sstr, "command initiated");
			break;
		case MDAT_ERR:
			strcpy(sstr, "error");
			break;
		}
		len += sprintf(buf+len, "    %-15s %25s\n", "state:", sstr);
	}
	return len;
}


/* Special DSP functionality */

int fake_hardware_reset(int card)
{
	return 0;
}

int fake_interface_reset(int card)
{
	return 0;
}


int fake_ready(int card) {
	struct mce_control *mdat = mce_dat + card;
	return mdat->initialized;
}

int fake_remove(int card);


mce_interface_t *fake_mce_create(int card)
{
 	struct mce_control *mdat = mce_dat + card;
	frame_buffer_mem_t *mem = NULL;
	int mem_size = FRAME_BUFFER_MAX_SIZE;
	int err = 0;

	PRINT_ERR("entry\n");
	memset(mdat, 0, sizeof(*mdat));

	init_MUTEX(&mdat->sem);

	init_timer(&mdat->timer);
	mdat->timer.function = fake_command_timer;
	mdat->timer.data = (unsigned long)mdat;

	mdat->state = MDAT_IDLE;
	mdat->data_flags = 0;

	for (; mem_size >= FRAME_BUFFER_MIN_SIZE; mem_size /= 2) {
		mem = pcimem_alloc(mem_size, NULL);
		if (mem != NULL)
			break;
	}
	if (mem == NULL) {
		PRINT_ERR("failed to allocate frame buffer.\n");
		goto out;
	}

	mdat->mce_emu = mce_fake_probe();
	if (mdat->mce_emu == NULL) goto out;

	err = data_probe(card, &fake_mce, mem, DEFAULT_DATA_SIZE);
	if (err !=0 ) goto out;

	err = mce_ops_probe(&fake_mce, card);
	if (err != 0) goto out;

	mceds_proc[card].mce = fake_mce.proc;
	mceds_proc[card].data = data_proc;

	mdat->initialized = 1;

	PRINT_INFO("ok.\n");
	return &fake_mce;

 out:
	PRINT_ERR("error!\n");

	fake_remove(card);
	return NULL;
}


int fake_remove(int card)
{
 	struct mce_control *mdat = mce_dat + card;

	PRINT_INFO("entry\n");

	if (!mdat->initialized) return 0;

	mdat->initialized = 0;
	del_timer_sync(&mdat->timer);
	mce_fake_remove(mdat->mce_emu);
	data_remove(card);

	PRINT_INFO("ok\n");
	return 0;
}


int mce_faker_init(int start, int count)
{
	int i;
	for (i=0; i<count; i++) {
		if (mce_interfaces[i+start] != NULL) {
			PRINT_ERR("cannot make MCE emulator at position %i.\n", i+start);
			continue;
		}
		mce_interfaces[i+start] = fake_mce_create(i+start);
	}
	return 0;
}


static mce_interface_t fake_mce = {
	.remove = fake_remove,
	.proc = fake_proc,
	.send_command = fake_send_command,
	.hardware_reset = fake_hardware_reset,
	.interface_reset = fake_interface_reset,
	.ready = fake_ready,
	.qt_command = fake_qt_command,
};
