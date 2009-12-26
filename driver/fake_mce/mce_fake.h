#ifndef _MCE_FAKE_H_
#define _MCE_FAKE_H_

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
#define ROW_LEN           0x30
#define RUN_ID            0x56
#define USER_WORD         0x57
#define TIMER_OUT         0xA2 /* select_clk :) */

/* Clock divider: 50 MHz / 41 / 64 = 19055 Hz */
#define CLK_DIV         19055

/* The min_data_rate used to be limited by the system timing resolution */
#define MIN_DATA_RATE   ( 1 )


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
#define DEFAULT_ROW_LEN    64

#define HEADER_VERSION 6

#define FAKE_MAX_DATA 0x2000 /* 32kB max packet size */

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


typedef struct mce_state_struct {

	u32 led;
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

	frame_header_t header;
	int frame_size;
	int frame_ticks;
	float frame_rate;

        /* Not the MCE... */

	int initialized;

	struct tasklet_struct msg_tasklet;

	int jiffies_counter;

	/* HST locking:
	 *  reply_pending != 0 when command received and reply imminent
	 *  reply_ready != 0 when reply is ready to be NFYed
	 *  data_ready != 0 when data is ready to be NFYed
	 *  notification == PACKET_* indicates type of last NFY, next HST
         *  - Do not send data packet if reply_pending or reply_ready.
	 */

	int go_now;

	int reply_pending;
	int reply_ready;
	int data_ready;
	int notification;

	int timeout_trigger;

	mce_reply reply;
	int reply_data_count;

	int data_frame[FAKE_MAX_DATA];
	int data_size;

} mce_state_t;


mce_state_t *mce_fake_probe(void);

void mce_fake_remove (mce_state_t *m);

int mce_fake_command(mce_state_t *mce_state, mce_command *cmd, mce_reply *rep);

int mce_fake_reset(mce_state_t *mce_state);

int fake_data_fill(mce_state_t *mce_state, u32 *data);


#endif
