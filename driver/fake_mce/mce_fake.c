#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/interrupt.h> /*Weird erorrs? try linux/sched.h before this*/

#include <mce_options.h>
#include <mce.h>

#include "dsp_fake.h"
#include "dsp_state.h"


/*  CARDS!  */

#define CC    0x2
#define RC1   0x3
#define RC2   0x4
#define RC3   0x5
#define RC4   0x6
#define RCS   0xb


/*  COMMANDS!  */

#define FW_REV            0x96
#define LED               0x99
#define RET_DAT           0x16
#define RET_DAT_S         0x53
#define DATA_RATE         0xa0

#define NUM_ROWS_REPORTED 0x55
#define NUM_ROWS          0x31
#define ROW_LEN           0x50
#define RUN_ID            0x56
#define USER_WORD         0x57
#define TIMER_OUT         0xA2 /* select_clk :) */

/* Other stuff... */

#define N_LED 16

/* Clock divider: 50 MHz / 41 / 64 = 19055 Hz */
#define CLK_DIV         19055

/* The min_data_rate is limited by the system timing resolution */
#define MIN_DATA_RATE   ( (CLK_DIV + HZ - 1) / HZ )


/* Needlessly precise timing model:
 *
 * System has HZ jiffies per second.  We want some fractional number
 * of jiffies between frames, namely (DATA * HZ / CLK_DIV).  Thus,
 * accumulate fractional jiffies in jiffies_counter.  At each timer
 * setup, we do
 *        jiffies_counter += DATA * HZ;
 *        delta_jiffies = jiffies_counter / CLK_DIV;
 *        jiffies_counter -= delta_jiffies * CLK_DIV;
 * The left over jiffie fractions eventually add up and get waited
 * for.
 */


/* Revised ugly packet queuing model:
 *
 * Tasklet is a nice touch for ordinary replies, but doesn't cut it
 * for data frames - NFY DA should come directly from the timer
 * interrupt.  To avoid crashing with the NFY RP from the go command,
 * don't schedule the timer until NFY RP has been sent, i.e. after NFY
 * RP the user has a jiffy before the data starts coming.
 *
 * Ordinary command replies are scheduled for tasklet issuing (then
 * they arrive in interrupt context, just like real live data).  Go
 * commands are replied to in the same way, and the timer is started
 * inside the NFY tasklet for the go command.  The timer is stopped
 * during the ret_dat command, before the reply to the stop command is
 * queued.
 */


/* Some default settings */

#define DEFAULT_DATA_RATE  0x5f /* 200 Hz */
#define DEFAULT_NUM_ROWS   41


#define INIT_KEY 0xa43125

#define HEADER_VERSION 6

typedef struct frame_header_struct {

	u32 flags;
	u32 sequence;
	u32 row_len;
	u32 num_rows_rep;
	u32 data_rate;
	u32 sync_num; // cc return to 0
	u32 header_v;
	u32 ramp_val;
	u32 ramp_card;
	u32 num_rows;
	u32 sync_dv;
	u32 run_id;
	u32 user_word;

	u32 other[30];

} frame_header_t;


#define PACKET_NONE    0
#define PACKET_DATA    1
#define PACKET_REPLY   2

#define MAX_DATA 0x2000 /* 2kB max packet size */

struct mce_state_struct {

	int led[N_LED];
	u32 data_rate;
	
	u32 ret_dat;
	u32 seq_first;
	u32 seq_last;
	u32 num_rows_rep;
	u32 num_rows;
	u32 row_len;
	u32 run_id;
	u32 user_word;

	int seq;
	int go;

        /* Not the MCE... */

	int initialized;

	struct tasklet_struct msg_tasklet;

	struct timer_list timer;
	
	int jiffies_counter;

	/* HST locking:
	 *  reply_pending != 0 when command received and reply imminent
	 *  reply_ready != 0 when reply is ready to be NFYed
	 *  data_ready != 0 when data is ready to be NFYed
	 *  notification == PACKET_* indicates type of last NFY, next HST
         *  - Do not send data packet if reply_pending or reply_ready.
	 */

	int reply_pending;
	int reply_ready;
	int data_ready;
	int notification;

	int timeout_trigger;

	mce_reply reply;
	int reply_data_count;

	int data_frame[MAX_DATA];
	int data_size;

} mce_state;


int fake_notify( int packet_type );
void fake_notify_task( unsigned long data );
/* void fake_nfy( unsigned long data); */
void fake_data_now(unsigned long arg);
inline void set_timer(void);


/* INIT */

int mce_fake_init( void )
{
	tasklet_init(&mce_state.msg_tasklet,
		     fake_notify_task, (unsigned long)&mce_state);

	init_timer(&mce_state.timer);
	mce_state.timer.function = fake_data_now;
	mce_state.timer.data = (unsigned long)&mce_state;

        /* Defaults */
	if (DEFAULT_DATA_RATE < MIN_DATA_RATE) {
		mce_state.data_rate = MIN_DATA_RATE;
	} else {
		mce_state.data_rate = DEFAULT_DATA_RATE;
	}	
	mce_state.num_rows_rep = DEFAULT_NUM_ROWS;

	mce_state.initialized = INIT_KEY;

	return 0;
}

int mce_fake_cleanup ( void )
{
	del_timer_sync(&mce_state.timer);
	tasklet_kill(&mce_state.msg_tasklet);
	return 0;
}

/* RESET */

int mce_fake_reset( void )
{
	// Stop sequencing!
	del_timer_sync(&mce_state.timer);
	mce_state.data_ready = 0;
	tasklet_kill(&mce_state.msg_tasklet);

	return 0;
}

/*
 * Notify / Host system
 */


int fake_notify( int packet_type )
{
	if (mce_state.timeout_trigger) {
		mce_state.timeout_trigger = 0;
		return 0;
	}

	switch (packet_type) {

	case PACKET_DATA:
		//Notify now
		mce_state.notification = PACKET_DATA;
		dsp_state_nfy_da(mce_state.data_size / sizeof(u32));
		break;

	case PACKET_REPLY:
		//Schedule tasklet
		if (mce_state.initialized != INIT_KEY) return -1;
		tasklet_schedule(&mce_state.msg_tasklet);
		break;

	default:
		return -1;
	}

	return 0;
}


void fake_notify_task( unsigned long data )
{
	if (mce_state.notification != PACKET_NONE) {
		PRINT_ERR("fake_notify_task: overwriting outstanding NFY!\n");
	}

	mce_state.notification = PACKET_REPLY;
	dsp_state_nfy_rp(sizeof(mce_state.reply) / sizeof(u32));

	if (mce_state.go)
		set_timer();
}


/* Le table d'host */

int mce_fake_host(void *dest)
{
	if (dest==NULL) return -1;
	switch (mce_state.notification) {

	case PACKET_DATA:
		memcpy(dest, &mce_state.data_frame, mce_state.data_size);
		mce_state.data_ready = 0;
		break;

	case PACKET_REPLY:
		memcpy(dest, &mce_state.reply, sizeof(mce_state.reply));
		mce_state.reply_ready = 0;
		break;

	default:
		PRINT_ERR("mce_fake_host: called with no packet ready\n");
	}

	mce_state.notification = PACKET_NONE;

	return 0;
}


/*
 * Data frame generator, called on some timer ticks
 */

void fake_data_now(unsigned long arg)
{
	frame_header_t *header = (frame_header_t*)&mce_state.data_frame;
	u32 *data = (u32*)header;

	int card_count, card_start, card, cols, r, c;
	u32 chksum = 0;
	
	PRINT_INFO("fake_data_now: frame!\n");

	header->flags = (mce_state.seq == mce_state.seq_last);
	header->sequence = mce_state.seq;
	header->row_len = mce_state.row_len;
	header->num_rows_rep = mce_state.num_rows_rep;
	header->data_rate = mce_state.data_rate;
	header->num_rows = mce_state.num_rows;
	header->header_v = HEADER_VERSION;
	header->run_id = mce_state.run_id;
	header->user_word = mce_state.user_word;
	

	cols = 8;
	card_count = 1;

	switch (mce_state.ret_dat) {
	case RCS:
		card_start = 0;
		card_count = 4;
		break;

	case RC1:
		card_start = 0;
		break;

	case RC2:
		card_start = 1;
		break;

	case RC3:
		card_start = 2;
		break;

	case RC4:
		card_start = 3;
		break;

	default:
		card_start = 0;
		card_count = 0;
	}

	// Cue data to data location while calculating checksum
	while (data < (u32*) (header + 1) )
		chksum ^= *(data++);

	// Write the appointed number of columns
	for (card = card_start; card < card_start + card_count; card++) {
		for (r=0; r<mce_state.num_rows_rep ; r++) {
			for (c = 0; c < cols; c++) {
				*data = (r << 8) | (c+card*cols);
				chksum ^= *(data++);
			}
		}
	}

	//Write the checksum
	*(data++) = chksum;

	if ( mce_state.go && (mce_state.seq != mce_state.seq_last) ) {

		//Reschedule
		mce_state.seq++;
		set_timer();

	} else {

		mce_state.go = 0;

	}

	//NFY!
	mce_state.data_size = (int)( (void*)data - (void*)header);
	PRINT_INFO("fake_data_frame: size=%i\n", mce_state.data_size);
	
        fake_notify( PACKET_DATA );
}


/* Data frame timing */

inline void set_timer(void)
{
	int j = mce_state.jiffies_counter + mce_state.data_rate * HZ;
	int d = j / CLK_DIV;
	mce_state.jiffies_counter = j - d * CLK_DIV;

	mce_state.timer.expires = jiffies + d;
	add_timer(&mce_state.timer);
}


/* MCE COMMAND HANDLERS
 *
 * These are hacked together to provide basic frame
 * functionality... most other commands will be ignored or seg-fault
 * (wait, that's bad...).
 */

int  fake_writeblock(const mce_command *cmd, mce_reply *rep);
int  fake_readblock(const mce_command *cmd, mce_reply *rep);
int  fake_go(const mce_command *cmd, mce_reply *rep);
int  fake_stop(const mce_command *cmd, mce_reply *rep);
void fake_checksum(mce_reply *rep, int n_data);

#define SUBNAME "mce_fake_command: "

int  mce_fake_command(void *src)
{
	mce_command *cmd = (mce_command*)src;
	mce_reply   *rep = &mce_state.reply;

	PRINT_INFO(SUBNAME "entry\n");

	if (cmd==NULL) {
		PRINT_ERR(SUBNAME "NULL command\n");
		return -1;
	}

	if (mce_state.reply_pending || mce_state.reply_ready) {
		PRINT_ERR(SUBNAME "data/reply packet outstanding...\n");
		return -1;
	}

	rep->ok_er = MCE_OK;
	rep->command = cmd->command;
	rep->para_id = cmd->para_id;
	rep->card_id = cmd->card_id;
	rep->data[0] = 0;    /* errno */
	mce_state.reply_data_count = 1;

	switch(cmd->command) {

	case MCE_WB:
		fake_writeblock(cmd, rep);
		break;

	case MCE_RB:
		mce_state.reply_data_count = cmd->count;
		fake_readblock(cmd, rep);
		break;

	case MCE_GO:
		fake_go(cmd, rep);
		break;

	case MCE_ST:
		fake_stop(cmd, rep);
		break;

	case MCE_RS:
		//Um...
		break;

	default:
		rep->ok_er = MCE_ER;
	}

	if (rep->ok_er == MCE_ER) {
		rep->data[0] = 0xffffffff;
		mce_state.reply_data_count = 1;
	}

	fake_checksum(rep, mce_state.reply_data_count);

	fake_notify(PACKET_REPLY);

	return 0;
}

#undef SUBNAME


#define SUBNAME "fake_writeblock: "

int fake_writeblock(const mce_command *cmd, mce_reply *rep)
{
	int card = cmd->card_id;

	PRINT_INFO(SUBNAME "entry\n");

	switch(cmd->para_id) {

	case LED:
		if (card < 0 || card >= N_LED) break;
		mce_state.led[card] ^= cmd->data[0];
		break;

	case DATA_RATE:
		if (card == CC) {
			mce_state.data_rate = cmd->data[0];
			
			if (cmd->data[0] >= MIN_DATA_RATE) {
				mce_state.data_rate = cmd->data[0];
			} else {
				PRINT_ERR(SUBNAME "data_rate too high, max "
					  "freq is %i HZ = %#x units\n",
					  HZ, MIN_DATA_RATE);
				mce_state.data_rate = MIN_DATA_RATE;
			}				
		} 
		break;

	case RET_DAT_S:
		mce_state.seq_first = cmd->data[0];
		mce_state.seq_last  = cmd->data[1];
		break;

	case NUM_ROWS_REPORTED:
		mce_state.num_rows_rep = cmd->data[0];
		break;

	case NUM_ROWS:
		mce_state.num_rows = cmd->data[0];
		break;

	case ROW_LEN:
		mce_state.row_len = cmd->data[0];
		break;

	case USER_WORD:
		mce_state.user_word = cmd->data[0];
		break;

	case RUN_ID:
		mce_state.run_id = cmd->data[0];
		break;

/* 	case TIMER_OUT: /\* select_clk ! *\/ */
/* 		/\* This will cause the command to timeout, useful for testing *\/ */
/* 		mce_state.timeout_trigger = 1; */
/* 		break; */

/* 	default: */
/* 		//Who am I to stop you? */
		
	}
	return 0;
}

#undef SUBNAME


int fake_readblock(const mce_command *cmd, mce_reply *rep)
{
	int card = cmd->card_id;
	int i;	

	switch(cmd->para_id) {

	case LED:
		if (card < 0 || card >= N_LED) break;
		rep->data[0] = mce_state.led[card];
		break;

	case FW_REV:
		rep->data[0] = 0xde00ad;
		break;

	case RET_DAT_S:
		rep->data[0] = mce_state.seq_first;
		rep->data[1] = mce_state.seq_last;
		break;

	case NUM_ROWS_REPORTED:
		rep->data[0] = mce_state.num_rows_rep;
		break;

	case NUM_ROWS:
		rep->data[0] = mce_state.num_rows;
		break;

	case ROW_LEN:
		rep->data[0] = mce_state.row_len;
		break;

	case USER_WORD:
		rep->data[0] = mce_state.user_word;
		break;

	case RUN_ID:
		rep->data[0] = mce_state.run_id;
		break;


	default:
		//Here, have some data.
		for (i=0; i<cmd->count; i++)
			rep->data[i] = i;
	}

	return 0;
}


int fake_go(const mce_command *cmd, mce_reply *rep)
{
	int card = cmd->card_id;

	switch(cmd->para_id) {

	case RET_DAT:
		mce_state.ret_dat = card;

		// I THINK YOU SHOULD GO NOW //

		PRINT_INFO("fake_go! card=%x, frames %i %i\n",
			  card, mce_state.seq_first, mce_state.seq_last);
		
		del_timer_sync(&mce_state.timer);
		mce_state.jiffies_counter = 0;
		mce_state.seq = mce_state.seq_first;
		mce_state.go = 1;

		break;

/* 	default: */
/* 		//Who am I to stop you? */
	}
	return 0;
}

int fake_stop(const mce_command *cmd, mce_reply *rep)
{
	switch(cmd->para_id) {

	case RET_DAT:
		// PLEASE STOP //

		PRINT_INFO("fake_stop. card=%x\n", cmd->card_id);
		
		del_timer_sync(&mce_state.timer);
		mce_state.ret_dat = 0;

		break;

/* 	default: */
/* 		//Who am I to stop you? */
	}
	return 0;
}


void fake_checksum(mce_reply *rep, int n_data)
{
	int sum = 0;
	u32 *d = (u32*)rep;
	while ( d < rep->data + n_data ) sum ^= *(d++);
	*(d++) = sum;
	while ( d < rep->data + MCE_REP_DATA_MAX ) *(d++) = 0;
}
