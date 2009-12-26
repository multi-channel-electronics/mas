#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/interrupt.h> /*Weird erorrs? try linux/sched.h before this*/

#include <mce_options.h>
#include <mce/types.h>

/* #include "dsp_fake.h" */
/* #include "dsp_state.h" */
#include "mce_fake.h"

#define PACKET_NONE    0
#define PACKET_DATA    1
#define PACKET_REPLY   2


void fake_data_now(unsigned long arg);

/* INIT */

mce_state_t *mce_fake_probe()
{
	mce_state_t *mce_state = kmalloc(sizeof(*mce_state), GFP_KERNEL); 
	if (mce_state == NULL) {
		PRINT_ERR("could not allocate mce_state.\n");
		return NULL;
	}
	memset(mce_state, 0, sizeof(*mce_state));
/* 	tasklet_init(&mce_state->msg_tasklet, */
/* 		     fake_notify_task, (unsigned long)mce_state); */
	mce_state->data_rate = DEFAULT_DATA_RATE;
	mce_state->num_rows_rep = DEFAULT_NUM_ROWS;
	mce_state->num_rows = DEFAULT_NUM_ROWS;
	mce_state->row_len = DEFAULT_ROW_LEN;
	mce_state->initialized = 1;

	return mce_state;
}

void mce_fake_remove(mce_state_t *mce_state)
{
	if (mce_state == NULL) return;
/* 	tasklet_kill(&mce_state->msg_tasklet); */
	kfree(mce_state);
}

/* RESET */

int mce_fake_reset(mce_state_t *mce_state)
{
	// Stop sequencing!
	mce_state->data_ready = 0;
	mce_state->ret_dat = 0;
	mce_state->go_now = 0;
/* 	tasklet_kill(&mce_state->msg_tasklet); */
	return 0;
}


/* fake_data_fill: put the next MCE frame data into dest */

int fake_data_fill(mce_state_t *mce_state, u32 *data)
{
	frame_header_t *header = (frame_header_t*)data;

	int card_count, card_start, card, cols, r, c;
	u32 chksum = 0;
	
	if (data==NULL) {
	  	mce_state->seq++;
		return 0;
	}

	header->flags = (mce_state->seq == mce_state->seq_last);
	header->sequence = mce_state->seq;
	header->row_len = mce_state->row_len;
	header->num_rows_rep = mce_state->num_rows_rep;
	header->data_rate = mce_state->data_rate;
	header->num_rows = mce_state->num_rows;
	header->header_v = HEADER_VERSION;
	header->run_id = mce_state->run_id;
	header->user_word = mce_state->user_word;
	
	cols = 8;
	card_count = 1;

	if (mce_state->ret_dat == RCS) {
		card_start = 0;
		card_count = 4;
	} else if (mce_state->ret_dat == RC1) {
		card_start = 0;
	} else if (mce_state->ret_dat == RC2) {
		card_start = 1;
	} else if (mce_state->ret_dat == RC3) {
		card_start = 2;
	} else if (mce_state->ret_dat == RC4) {
		card_start = 3;
	} else {
		card_start = 0;
		card_count = 0;
	}

	// Cue data to data location while calculating checksum
	while (data < (u32*) (header + 1) )
		chksum ^= *(data++);

	// Write the appointed number of columns
	for (card = card_start; card < card_start + card_count; card++) {
		for (r=0; r<mce_state->num_rows_rep ; r++) {
			for (c = 0; c < cols; c++) {
				*data = (r << 8) | (c+card*cols);
				chksum ^= *(data++);
			}
		}
	}

	//Write the checksum and increment the frame counter
	*(data++) = chksum;
	mce_state->seq++;

	return (int)( (void*)data - (void*)header);
}


/* MCE COMMAND HANDLERS
 *
 * These are hacked together to provide basic frame
 * functionality... most other commands will be ignored or seg-fault
 * (wait, that's bad...).
 */

int  fake_writeblock(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep);
int  fake_readblock(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep);
int  fake_go(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep);
int  fake_stop(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep);
void fake_checksum(mce_reply *rep, int n_data);

#define SUBNAME "mce_fake_command: "

int mce_fake_command(mce_state_t *mce_state, mce_command *cmd, mce_reply *rep)
{
	PRINT_INFO(SUBNAME "entry\n");

	if (cmd==NULL || rep==NULL) {
		PRINT_ERR(SUBNAME "NULL command or reply\n");
		return -1;
	}

	// Initialize reply
	rep->ok_er = MCE_OK;
	rep->command = cmd->command;
	rep->para_id = cmd->para_id;
	rep->card_id = cmd->card_id;
	rep->data[0] = 0;    /* errno */
	mce_state->reply_data_count = 1;

	switch(cmd->command) {

	case MCE_WB:
		fake_writeblock(mce_state, cmd, rep);
		break;

	case MCE_RB:
		mce_state->reply_data_count = cmd->count;
		fake_readblock(mce_state, cmd, rep);
		break;

	case MCE_GO:
		fake_go(mce_state, cmd, rep);
		break;

	case MCE_ST:
		fake_stop(mce_state, cmd, rep);
		break;

	case MCE_RS:
		//Um...
		break;

	default:
		rep->ok_er = MCE_ER;
	}

	if (rep->ok_er == MCE_ER) {
		rep->data[0] = 0xffffffff;
		mce_state->reply_data_count = 1;
	}

	fake_checksum(rep, mce_state->reply_data_count);
	return 0;
}

#undef SUBNAME


#define SUBNAME "fake_writeblock: "

int fake_writeblock(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep)
{
	int card = cmd->card_id;

	PRINT_INFO(SUBNAME "entry\n");

	switch(cmd->para_id) {

	case LED:
		mce_state->led ^= cmd->data[0];
		break;

	case DATA_RATE:
		if (card == CC) {
			mce_state->data_rate = cmd->data[0];
			
			if (cmd->data[0] >= MIN_DATA_RATE) {
				mce_state->data_rate = cmd->data[0];
			} else {
				PRINT_ERR(SUBNAME "data_rate too high, max "
					  "freq is %i HZ = %#x units\n",
					  HZ, MIN_DATA_RATE);
				mce_state->data_rate = MIN_DATA_RATE;
			}				
		} 
		break;

	case RET_DAT_S:
		mce_state->seq_first = cmd->data[0];
		mce_state->seq_last  = cmd->data[1];
		break;

	case NUM_ROWS_REPORTED:
		mce_state->num_rows_rep = cmd->data[0];
		break;

	case NUM_ROWS:
		mce_state->num_rows = cmd->data[0];
		break;

	case ROW_LEN:
		mce_state->row_len = cmd->data[0];
		break;

	case USER_WORD:
		mce_state->user_word = cmd->data[0];
		break;

	case RUN_ID:
		mce_state->run_id = cmd->data[0];
		break;

/* 	case TIMER_OUT: /\* select_clk ! *\/ */
/* 		/\* This will cause the command to timeout, useful for testing *\/ */
/* 		mce_state->timeout_trigger = 1; */
/* 		break; */

/* 	default: */
/* 		//Who am I to stop you? */
		
	}
	return 0;
}

#undef SUBNAME


int fake_readblock(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep)
{
	int i;	

	switch(cmd->para_id) {

	case LED:
		rep->data[0] = mce_state->led;
		break;

	case FW_REV:
		rep->data[0] = 0xde00ad;
		break;

	case RET_DAT_S:
		rep->data[0] = mce_state->seq_first;
		rep->data[1] = mce_state->seq_last;
		break;

	case NUM_ROWS_REPORTED:
		rep->data[0] = mce_state->num_rows_rep;
		break;

	case NUM_ROWS:
		rep->data[0] = mce_state->num_rows;
		break;

	case ROW_LEN:
		rep->data[0] = mce_state->row_len;
		break;

	case DATA_RATE:
		rep->data[0] = mce_state->data_rate;
		break;

	case USER_WORD:
		rep->data[0] = mce_state->user_word;
		break;

	case RUN_ID:
		rep->data[0] = mce_state->run_id;
		break;


	default:
		//Here, have some data.
		for (i=0; i<cmd->count; i++)
			rep->data[i] = i;
	}

	return 0;
}


int fake_go(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep)
{
	int card = cmd->card_id;

	switch(cmd->para_id) {

	case RET_DAT:
		mce_state->go_now = 1;
		mce_state->ret_dat = card;
		mce_state->seq = mce_state->seq_first;

		// I THINK YOU SHOULD GO NOW //

		PRINT_INFO("fake_go! card=%x, frames %i %i\n",
			  card, mce_state->seq_first, mce_state->seq_last);
		
		break;

/* 	default: */
/* 		//Who am I to stop you? */
	}
	return 0;
}

int fake_stop(mce_state_t *mce_state, const mce_command *cmd, mce_reply *rep)
{
	switch(cmd->para_id) {

	case RET_DAT:
		PRINT_INFO("fake_stop. card=%x\n", cmd->card_id);
		mce_state->data_ready = 0;
		mce_state->ret_dat = 0;
		mce_state->go_now = 0;
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
