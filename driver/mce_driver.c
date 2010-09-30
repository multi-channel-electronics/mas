/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include "kversion.h"
#include "mce_options.h"
#include "memory.h"

#include "mce/dsp.h"
#include "mce/mce_errors.h"

#include "mce_driver.h"
#include "mce_ops.h"
#include "data.h"
#include "data_ops.h"
#include "dsp_driver.h"

#define MAX_FERR 100


typedef struct {

  mce_command *command;
  mce_reply   *reply;
  u32          reply_size;

  /* Since PCI is 32-bit, our bus addresses are u32. */
  u32 command_busaddr;
  u32 reply_busaddr;

  int dma_size;

} mce_comm_buffer;

typedef enum {
  MDAT_IDLE = 0,
  MDAT_CON,
  MDAT_CONOK,
  MDAT_NFY,
  MDAT_HST,
  MDAT_ERR,
} mce_state_t;

struct mce_local {

  struct semaphore sem;
  wait_queue_head_t queue;
  mce_reply *rep;
  int flags;
#define   LOCAL_CMD 0x01
#define   LOCAL_REP 0x02
#define   LOCAL_ERR 0x08

};

struct mce_control {

  struct mce_local local;
  struct semaphore sem;
  struct timer_list timer;
  struct tasklet_struct hst_tasklet;

  struct timer_list dtimer;
  wait_queue_head_t dqueue;
  int dexpired;

  int initialized;
  int quiet_rp;

  spinlock_t state_lock;
  volatile
    mce_state_t state;

  int ferror_count;
  int hst_sched_count;

  int data_flags;
#define   MDAT_DGO   0x001
#define   MDAT_DHST  0x002

  mce_callback callback;

  dsp_message hst_msg;

  mce_comm_buffer buff;

} mce_dat[MAX_CARDS];


int mce_error_register(int card)
{
  struct mce_control *mdat = mce_dat + card;

  if (mdat->ferror_count == MAX_FERR)
    PRINT_ERR(card, "no further frame errors will be logged.\n");

  return (mdat->ferror_count++ < MAX_FERR);
}

void mce_error_reset( int card )
{
  struct mce_control *mdat = mce_dat + card;

  mdat->ferror_count = 0;
}


/*
 *   MCE command routines.  Holy, elaborate.
 *   When changing mdat->state, get mdat->state_lock.
 */

int  mce_CON_dsp_callback( int error, dsp_message *msg, int card);
int  mce_NFY_RP_handler( int error, dsp_message *msg, int card);
void mce_do_HST_or_schedule( unsigned long data );
int  mce_HST_dsp_callback( int error, dsp_message *msg, int card);


#define MDAT_LOCK    spin_lock_irqsave(&mdat->state_lock, irqflags)
#define MDAT_UNLOCK  spin_unlock_irqrestore(&mdat->state_lock, irqflags)

/* First set: interrupt context, no blocking and no sems! */

/* Generic error handler; reports error to caller and goes to IDLE state.
   Call while holding state_lock.
   */

int mce_command_do_callback( int error, mce_reply *rep, int card)
{
  /* spinlock is assumed held! */
  struct mce_control *mdat = mce_dat + card;

  mdat->state = MDAT_IDLE;

  if ( mdat->callback != NULL ) {
    mdat->callback(error, rep, card);
  } else {
    PRINT_INFO(card, "no callback specified\n");
  }

  // Clear the buffer for the next reply
  memset(mdat->buff.reply, 0, sizeof(*mdat->buff.reply));
  mdat->callback = NULL;

  return 0;
}


int mce_CON_dsp_callback(int error, dsp_message *msg, int card)
{
  unsigned long irqflags;
  struct mce_control *mdat = mce_dat + card;

  PRINT_INFO(card, "entry\n");

  MDAT_LOCK;
  if (mdat->state != MDAT_CON) {
    PRINT_ERR(card, "unexpected callback! (state=%i)\n", mdat->state);
    MDAT_UNLOCK;
    return -1;
  }

  if (error<0 || msg==NULL) {
    PRINT_ERR(card, "called with error %i\n", error);
    if (error == DSP_ERR_TIMEOUT) {
      mce_command_do_callback(-MCE_ERR_INT_TIMEOUT, NULL, card);
    } else {
      mce_command_do_callback(-MCE_ERR_INT_UNKNOWN, NULL, card);
    }
    MDAT_UNLOCK;
    return 0;
  }

  if (msg->command != DSP_CON) {
    PRINT_ERR(card, "dsp command was not CON!\n");
    mce_command_do_callback(-MCE_ERR_INT_PROTO, NULL, card);
    MDAT_UNLOCK;
    return 0;
  }

  if (msg->reply != DSP_ACK) {
    PRINT_ERR(card, "dsp reply was not ACK!\n");
    mce_command_do_callback(-MCE_ERR_INT_FAILURE, NULL, card);
    MDAT_UNLOCK;
    return 0;
  }

  mdat->state = MDAT_CONOK;
  MDAT_UNLOCK;
  PRINT_INFO(card, "state<-CONOK\n");

  return 0;
}


int mce_NFY_RP_handler( int error, dsp_message *msg, int card)
{
  unsigned long irqflags;
  struct mce_control *mdat = mce_dat + card;

  MDAT_LOCK;

  // We'll just trust the NFY for now, assuming no error.
  if ( error || (msg==NULL) ) {
    PRINT_ERR(card, "called error=%i, msg=%lx\n", error, (unsigned long)msg);
    mce_command_do_callback(-MCE_ERR_INT_SURPRISE, NULL, card);
    dsp_unreserve(card);
    MDAT_UNLOCK;
    return 0;
  }

  if (mdat->state != MDAT_CONOK) {
    PRINT_ERR(card, "unexpected state=%i\n", mdat->state);
    MDAT_UNLOCK;
    return -1;
  }

  mdat->state = MDAT_NFY;
  mdat->hst_sched_count = 0;
  dsp_unreserve(card);
  MDAT_UNLOCK;

  /* do_HST needs the lock not held. */
  mce_do_HST_or_schedule( (unsigned long)mdat );
  return 0;
}


#define HST_FILL(cmd, bus) cmd.command = DSP_HST; \
                                         cmd.args[0] = (bus >> 16) & 0xffff; \
cmd.args[1] = bus & 0xffff

void mce_do_HST_or_schedule(unsigned long data)
{
  /* Do not call with state_lock held!
   * Attempts to issue HST command to PCI card.  If PCI card
   * refuses, the operation is scheduled for later. */
  unsigned long irqflags;
  struct mce_control *mdat = (struct mce_control *)data;
  int err;
  int card = mdat - mce_dat;
  dsp_command cmd;
  HST_FILL(cmd, mdat->buff.reply_busaddr);

  MDAT_LOCK;
  if (mdat->state != MDAT_NFY) {
    PRINT_ERR(card, "unexpected state=%i\n", mdat->state);
    goto up_and_out;
  }

  if ( (err=dsp_send_command(&cmd, mce_HST_dsp_callback, card, 0)) ) {
    if(err == -EAGAIN) {
      if (mdat->hst_sched_count == 0)
        PRINT_ERR(card, "dsp busy; rescheduling.\n");
      mdat->hst_sched_count++;
      tasklet_schedule(&mdat->hst_tasklet);
    } else {
      PRINT_ERR(card, "dsp_send_command failed after %i "
          "tries.\n", mdat->hst_sched_count);
      mce_command_do_callback(-MCE_ERR_INT_FAILURE, NULL, card);
    }
  } else {
    if (mdat->hst_sched_count > 0)
      PRINT_ERR(card, "succeeded after %i tries.\n",
          mdat->hst_sched_count);
    mdat->state = MDAT_HST;;
  }
up_and_out:
  MDAT_UNLOCK;
  return;
}


int mce_HST_dsp_callback(int error, dsp_message *msg, int card)
{
  unsigned long irqflags;
  struct mce_control *mdat = mce_dat + card;
  int ret_val = 0;

  MDAT_LOCK;
  if (mdat->state != MDAT_HST) {
    PRINT_ERR(card, "unexpected callback! (state=%i)\n",
        mdat->state);
    ret_val = -1;
    goto up_and_out;
  }

  if (error<0 || msg==NULL) {
    PRINT_ERR(card, "called with error %i\n", error);
    if (error == DSP_ERR_TIMEOUT) {
      mce_command_do_callback(-MCE_ERR_INT_TIMEOUT, NULL, card);
    } else {
      mce_command_do_callback(-MCE_ERR_INT_UNKNOWN, NULL, card);
    }
    goto up_and_out;
  }

  if (msg->command != DSP_HST) {
    PRINT_ERR(card, "dsp command was not HST!\n");
    mce_command_do_callback(-MCE_ERR_INT_PROTO, NULL, card);
    goto up_and_out;
  }

  if (msg->reply != DSP_ACK) {
    PRINT_ERR(card, "dsp reply was not ACK!\n");
    mce_command_do_callback(-MCE_ERR_INT_FAILURE, NULL, card);
    goto up_and_out;
  }

  mce_command_do_callback(0, mdat->buff.reply, card);
up_and_out:
  MDAT_UNLOCK;
  return ret_val;
}

/* Simplified reply system, for DSP >= U0105.  Reply buffer address
 * is pre-loaded to the DSP, so we don't have to hand-shake in
 * real-time.
 *
 * mce_NFY_RPQ_handler: immediately calls back with the mce reply.
 */

int mce_NFY_RPQ_handler( int error, dsp_message *msg, int card)
{
  unsigned long irqflags;
  struct mce_control *mdat = mce_dat + card;

  MDAT_LOCK;

  // We'll just trust the NFY for now, assuming no error.
  if ( error || (msg==NULL) ) {
    PRINT_ERR(card, "called error=%i, msg=%lx\n", error, (unsigned long)msg);
    mce_command_do_callback(-MCE_ERR_INT_SURPRISE, NULL, card);
    dsp_unreserve(card);
    MDAT_UNLOCK;
    return 0;
  }

  if (mdat->state != MDAT_CONOK) {
    PRINT_ERR(card, "unexpected state=%i\n", mdat->state);
    MDAT_UNLOCK;
    return -1;
  }

  // Callback, which must copy away the reply
  mce_command_do_callback(0, mdat->buff.reply, card);

  mdat->state = MDAT_IDLE;
  dsp_request_clear_RP(card);
  dsp_unreserve(card);
  MDAT_UNLOCK;

  return 0;
}

int mce_qt_command( dsp_qt_code code, int arg1, int arg2, int card)
{
  dsp_command cmd = { DSP_QTS, {code,arg1,arg2} };
  dsp_message reply;
  return dsp_send_command_wait(&cmd, &reply, card);
}

int mce_quiet_RP_config(int enable, int card)
{
  struct mce_control *mdat = mce_dat + card;
  int err = 0;
  u32 bus = mdat->buff.reply_busaddr;

  PRINT_INFO(card, "disabling...\n");

  err |= mce_qt_command(DSP_QT_RPENAB, 0, 0, card);
  mdat->quiet_rp = 0;
  if (err) {
    PRINT_ERR(card, "failed to disable quiet RP\n");
    return -1;
  }
  if (!enable) return 0;

  // Enable qt replies
  PRINT_INFO(card, "enabling...\n");

  err |= mce_qt_command(DSP_QT_RPSIZE, sizeof(mce_reply), 0, card);
  err |= mce_qt_command(DSP_QT_RPBASE,
      (bus      ) & 0xFFFF,
      (bus >> 16) & 0xFFFF, card );
  err |= mce_qt_command(DSP_QT_RPENAB, 1, 0, card);

  if (err) {
    PRINT_ERR(card, "failed to configure DSP.\n");
    return -1;
  }

  mdat->quiet_rp = 1;
  return 0;
}

//Command must already be in mdat->buff.command.
int mce_send_command_now (int card)
{
  struct mce_control *mdat = mce_dat + card;
  int err = 0;
  u32 baddr = mdat->buff.command_busaddr;

  dsp_command cmd = {
    DSP_CON,
    { (baddr >> 16) & 0xffff, baddr & 0xffff, 0 }
  };

  PRINT_INFO(card, "Sending CON [%#08x %#04x %#04x]\n",
      mdat->buff.command->command,
      (int)mdat->buff.command->para_id,
      (int)mdat->buff.command->card_id);

  if ( (err=dsp_send_command( &cmd, mce_CON_dsp_callback, card, 1))) {
    PRINT_INFO(card, "dsp_send_command failed (%#x)\n",
        err);
    switch(-err) {
      case EAGAIN:
        return -MCE_ERR_INT_BUSY;
        //case EIO:
        //case ERESTARTSYS:
      default:
        return -MCE_ERR_INT_UNKNOWN;
    }
  }
  return 0;
}

void mce_send_command_timer(unsigned long data)
{
  unsigned long irqflags;
  struct mce_control *mdat = (struct mce_control *)data;
  int card = mdat - mce_dat;

  MDAT_LOCK;
  if (mdat->state == MDAT_IDLE) {
    PRINT_INFO(card, "timer ignored\n");
  } else {
    dsp_unreserve(card);
    PRINT_ERR(card, "mce reply timed out!\n");
    mce_command_do_callback( -MCE_ERR_TIMEOUT, NULL, card);
  }
  MDAT_UNLOCK;
}


int mce_send_command(mce_command *cmd, mce_callback callback, int non_block, int card)
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
    PRINT_INFO(card, "transaction in progress (state=%i)\n",
        mdat->state);
    ret_val = -MCE_ERR_ACTIVE;
    goto up_and_out;
  }

  // Copy the command and initiate
  memcpy(mdat->buff.command, cmd, sizeof(*cmd));
  if ( (ret_val = mce_send_command_now(card)) ) {
    PRINT_INFO(card, "send now failed [%i]!\n", ret_val);
    mdat->callback = NULL;
  } else {
    // Register callback, advance state, enable time-out.
    mod_timer(&mdat->timer, jiffies + MCE_DEFAULT_TIMEOUT);
    mdat->callback = callback;
    mdat->state = MDAT_CON;
  }

up_and_out:
  MDAT_UNLOCK;
  up(&mdat->sem);
  return ret_val;
}


/******************************************************************/

/*
   Upon receipt of NFY, dsp_int_handler calls mce_int_handler.  The
   mce_int_handler determines if packet is DA or RP, and calls the
   associated handler, either mce_da_hst or mce_rp_hst.  The *_hst
   functions are responsible for immediately
   */

int mce_da_hst_callback(int error, dsp_message *msg, int card)
{
  //FIX ME: "error" case should be natural and handled smoothly.
  // What will happen to this "data"?
  struct mce_control *mdat = mce_dat + card;

  if (error || msg==NULL) {

    if (!mce_error_register(card)) return -1;

    PRINT_ERR(card, "called with error = %i\n", error);
    if (msg==NULL) {
      PRINT_ERR(card, "dsp_message is NULL\n");
    } else {
      PRINT_INFO(card, "dsp_message=(%06x, %06x, %06x, %06x)\n",
          msg->type, msg->command,
          msg->reply, msg->data);
    }
    mdat->data_flags &= ~MDAT_HST;
    return -1;
  }

  if (mdat->data_flags != (MDAT_DHST)) {
    if (mce_error_register(card))
      PRINT_ERR(card, "unexpected flags state %#x\n",
          mdat->data_flags);
    mdat->data_flags = 0;
    return -1;
  }

  if (data_frame_increment(card) && mce_error_register(card)) {
    PRINT_ERR(card, "frame_increment error; packet lost\n");
  }

  //Only action is to increment tail pointer or whatever

  mdat->data_flags &= ~MDAT_DHST;
  return 0;
}

int mce_da_hst_now(int card)
{
  struct mce_control *mdat = mce_dat + card;
  int err = 0;
  u32 baddr;
  dsp_command cmd;

  PRINT_INFO(card, "NFY-DA accepted, sending HST\n");

  if ( (mdat->data_flags & MDAT_DHST) && mce_error_register(card) ) {
    PRINT_ERR(card, "NFY-DA interrupts outstanding HST!\n");
  }

  data_frame_address(&baddr, card);
  HST_FILL(cmd, baddr);

  if ((err = dsp_send_command(&cmd, mce_da_hst_callback, card, 0))) {
    PRINT_INFO(card, "dsp_send_command failed!\n");
    if (mce_error_register(card)) {
      PRINT_ERR(card, "dsp_send_command error %i; "
          "packet dropped\n",
          err);
    }
    return -1;
  }

  mdat->data_flags |= MDAT_DHST;

  return 0;
}


int mce_int_handler( dsp_message *msg, unsigned long data )
{
  struct mce_control *mdat = (struct mce_control *)data;
  dsp_notification *note = (dsp_notification*) msg;
  int packet_size = (note->size_lo | (note->size_hi << 16)) * 4;
  int card = mdat - mce_dat;
  frame_buffer_t *dframes = data_frames + card;

  if (note->type != DSP_NFY) {
    PRINT_ERR(card, "message is not NFY!\n");
    return -1;
  }

  switch(note->code) {

    case DSP_RP:

      PRINT_INFO(card, "NFY RP identified\n");
      mce_NFY_RP_handler(0, msg, card);

      break;

    case DSP_RPQ:

      PRINT_INFO(card, "NFY RPQ identified\n");
      mce_NFY_RPQ_handler(0, msg, card);

      break;

    case DSP_DA:
      if (packet_size != dframes->data_size) {
        if (mce_error_register(card))
          PRINT_ERR(card, "unexpected DA packet size"
              "%i bytes; dropping.\n",
              packet_size);
        return -1;
      } else
        mce_da_hst_now(card);
      break;

    default:
      PRINT_ERR(card, "unknown packet type, ignoring\n");
  }
  return 0;
}

//mce_send_command_wait (_callback) lived here once upon a time...

int mce_buffer_allocate(mce_comm_buffer *buffer, int card)
{
  unsigned long bus;

  // Create DMA-able area.  Use only one call since the two
  // buffers are so small.

  int offset = ( sizeof(mce_command) + (DMA_ADDR_ALIGN-1) ) &
    DMA_ADDR_MASK;

  int size = offset + sizeof(mce_reply);

  buffer->command = (mce_command*) dsp_allocate_dma(size, &bus);
  if (buffer->command==NULL)
    return -ENOMEM;
  if ((bus >> 16) >> 16 != 0) {
    PRINT_ERR(card, "dsp_allocate returned out of bounds address %lx\n", bus);
    return -ENOMEM;
  }
  buffer->command_busaddr = (u32)bus;

  buffer->reply = (mce_reply*) ((char*)buffer->command + offset);
  buffer->reply_busaddr = buffer->command_busaddr + offset;
  buffer->dma_size = size;

  PRINT_INFO(card, "cmd/rep[virt->bus]: [%lx->%lx]/[%lx->%lx]\n",
      (long unsigned int)buffer->command,
      (long unsigned int)virt_to_bus(buffer->command),
      (long unsigned int)buffer->reply,
      (long unsigned int)virt_to_bus(buffer->reply));

  return 0;
}

int mce_buffer_free(mce_comm_buffer *buffer)
{
  if (buffer->command!=NULL) {
    dsp_free_dma(buffer->command, buffer->dma_size,
        (unsigned long)buffer->command_busaddr);
  }

  buffer->command = NULL;
  buffer->reply   = NULL;

  return 0;
}


int mce_proc(char *buf, int count, int card)
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
      case MDAT_CONOK:
        strcpy(sstr, "command sent");
        break;
      case MDAT_NFY:
        strcpy(sstr, "reply notified");
        break;
      case MDAT_HST:
        strcpy(sstr, "reply queried");
        break;
      case MDAT_ERR:
        strcpy(sstr, "error");
        break;
    }
    len += sprintf(buf+len, "    %-15s %25s\n", "state:", sstr);
  }
  if (len < count) {
    len += sprintf(buf+len, "    %-15s %25s\n", "quiet_RP:",
        mdat->quiet_rp ? "on" : "off");
  }
  return len;
}


/* Block */

static void delay_func(unsigned long data)
{
  struct mce_control *mdat = (void*)data;
  mdat->dexpired = 1;
  wake_up_interruptible(&mdat->dqueue);
}

static int delay(struct mce_control *mdat, int ms)
{
  int j = (ms * HZ) / 1000;
  if (j<1) j=1;
  mdat->dexpired = 0;
  barrier();
  mod_timer(&mdat->dtimer, jiffies+j);
  return wait_event_interruptible(mdat->dqueue, mdat->dexpired);
}

/* Special DSP functionality */

int mce_hardware_reset(int card)
{
  dsp_command cmd = { DSP_RCO, {0,0,0} };
  dsp_message msg;
  return dsp_send_command_wait(&cmd, &msg, card);
}

int mce_interface_reset(int card)
{
  struct mce_control *mdat = mce_dat + card;
  dsp_command cmd = { DSP_RST, {0,0,0} };
  dsp_message msg;
  int err;
  err = dsp_send_command_wait(&cmd, &msg, card);
  if (err) return err;
  if (delay(mdat, 500)) return -1;
  return mce_quiet_RP_config(mdat->quiet_rp, card);
}


int mce_init()
{
  int err = 0;
  PRINT_INFO(NOCARD, "entry\n");

  err = mce_ops_init();
  if(err != 0) goto out;

  //FIX ME:: add error checking for data_ops
  data_ops_init();

  PRINT_INFO(NOCARD, "ok\n");
  return 0;
out:
  PRINT_ERR(NOCARD, "exiting with error\n");
  return err;
}


int mce_ready(int card) {
  struct mce_control *mdat = mce_dat + card;
  return mdat->initialized;
}


int mce_probe(int card, int dsp_version)
{
  struct mce_control *mdat = mce_dat + card;
  frame_buffer_t *dframes = data_frames + card;
  int err = 0;

  PRINT_INFO(card, "(%i, %i) entry\n", card, dsp_version);

  memset(mdat, 0, sizeof(*mdat));

  init_MUTEX(&mdat->sem);
  init_MUTEX(&mdat->local.sem);
  spin_lock_init(&mdat->state_lock);

  init_waitqueue_head(&mdat->local.queue);

  tasklet_init(&mdat->hst_tasklet,
      mce_do_HST_or_schedule, (unsigned long)mdat);

  init_timer(&mdat->timer);
  mdat->timer.function = mce_send_command_timer;
  mdat->timer.data = (unsigned long)mdat;

  init_waitqueue_head(&mdat->dqueue);
  init_timer(&mdat->dtimer);
  mdat->dtimer.function = delay_func;
  mdat->dtimer.data = (unsigned long)mdat;

  mdat->state = MDAT_IDLE;
  mdat->data_flags = 0;
  mdat->quiet_rp = 0;
  mdat->initialized = 1;

  err = data_probe(dsp_version, card, FRAME_BUFFER_SIZE, DEFAULT_DATA_SIZE);
  if (err !=0 ) goto out;

  err = mce_buffer_allocate(&mdat->buff, card);
  if (err != 0) goto out;

  err = mce_ops_probe(card);
  if (err != 0) goto out;

  // Set up command and quiet transfer handlers
  dsp_set_msg_handler(DSP_QTI, mce_qti_handler, (unsigned long)dframes, card);
  dsp_set_msg_handler(DSP_NFY, mce_int_handler, (unsigned long)mdat, card);

  if (dsp_version >= DSP_U0105) {
    mce_quiet_RP_config(1, card);
  }

  PRINT_INFO(card, "ok.\n");
  return 0;

out:
  PRINT_ERR(card, "error!\n");

  mce_remove(card);
  return err;
}

int mce_cleanup()
{
  PRINT_INFO(NOCARD, "entry\n");
  mce_ops_cleanup();
  data_ops_cleanup();

  PRINT_INFO(NOCARD, "ok\n");
  return 0;
}

int mce_remove(int card)
{
  struct mce_control *mdat = mce_dat + card;

  PRINT_INFO(card, "entry\n");

  if (!mdat->initialized) return 0;

  if (mdat->quiet_rp) {
    mce_quiet_RP_config(0, card);
  }

  del_timer_sync(&mdat->dtimer);
  del_timer_sync(&mdat->timer);
  tasklet_kill(&mdat->hst_tasklet);

  mce_buffer_free(&mdat->buff);

  data_remove(card);

  PRINT_INFO(card, "ok\n");
  return 0;
}
