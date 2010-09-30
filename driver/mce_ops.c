/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "kversion.h"
#include "mce_options.h"

#include "mce_ops.h"
#include "mce_driver.h"
#include "mce/mce_ioctl.h"
#include "mce/mce_errors.h"

struct file_operations mce_fops = 
{
  .owner=   THIS_MODULE,
  .open=    mce_open, 
  .read=    mce_read,
  .release= mce_release,
  .write=   mce_write,
  .ioctl=   mce_ioctl,
};

typedef enum {
  OPS_IDLE = 0,
  OPS_CMD,
  OPS_REP,
  OPS_ERR
} mce_ops_state_t;


#define MAX_CMD_RETRY 100

/* Each agent that opens the device file is allocated a filp_pdata.
   This is where reader-writer specific information is stored. */

struct filp_pdata {
  int minor;      /* Minor (card) number. */
  int error;      /* Error code of last operation. */
  int properties; /* accessed through MCEDEV_IOCT_GET / SET */
};

struct mce_ops_t {

  int major;

  struct semaphore sem;
  wait_queue_head_t queue;

  mce_ops_state_t state;
  struct filp_pdata *commander;
  int cmd_error;

  mce_reply   rep;
  mce_command cmd;

} mce_ops[MAX_CARDS];


/* The operations states are defined as follows:

   OPS_IDLE indicates the device is available for a new write operation.
   In this state, writing data to the device will initiate a command,
   and reading from the device will return 0.  Successors to this state
   are CMD and ERR

   OPS_CMD indicates that a command has been initiated.  If a write to
   the device succeeds (i.e. does not return 0 or an error code) then
   the driver will be but into this state.

   OPS_REP indicates that a reply to a command has been received
   successfully and is waiting to be read.

   OPS_ERR indicates that the command / reply sequence failed at some
   step.  The error state is cleared by reading from the device, which
   will return 0 bytes.

   The behaviour of the device files in each of the states is as follows:

   Method State          Blocking                 Non-blocking
   ------------------------------------------------------------------
   read   IDLE           return 0                 return 0
   read   CMD            wait for state!=CMD      return -EAGAIN
   read   REP            [copy reply to the user, returning number
   of bytes in the reply]
   read   ERR            return 0                 return 0

   write  IDLE           [copy command from user and initiate it;
   return size of command or 0 on failure]
   write  CMD            return 0                 return 0
   write  REP            return 0                 return 0
   write  ERR            return 0                 return 0

   In non-blocking mode, both methods immediately return -EAGAIN if they
   cannot obtain the operations semaphore.


   The state update rules are as follows:

   IDLE -> CMD   a command was initiated successfully.
   CMD  -> REP   the reply to the command was received
   CMD  -> ERR   the driver indicated an error in receiving the command
   REP  -> IDLE  the reply has been copied to the user
   ERR  -> IDLE  the error has been communicated to the user


   Note that the state ERR does not represent a problem with driver use;
   it represents a failure of the hardware for some reason.  ERR state is
   different from the errors when device operations return 0.  The cause
   of device operations returning 0 can be obtained by calling ioctl
   operation MCEDEV_IOCT_LAST_ERROR.  This error is cleared on the next
   read/write operation.

   So anyway, we're going to get rid of the ERR state and instead just
   return an error packet as the reply.  That way mce_ops doesn't need to
   know if the MCE works or not.

*/

ssize_t mce_read(struct file *filp, char __user *buf, size_t count,
    loff_t *f_pos)
{
  struct filp_pdata *fpdata = filp->private_data;
  struct mce_ops_t *mops = mce_ops + fpdata->minor;
  int card = fpdata->minor;
  int err = 0;
  int ret_val = 0;

  PRINT_INFO(card, "state=%#x\n", mops->state);

  // Loop until we get the semaphore and there is a reply available.
  while (1) {
    if (filp->f_flags & O_NONBLOCK) {
      if (down_trylock(&mops->sem))
        return -EAGAIN;
    } else {
      if (down_interruptible(&mops->sem))
        return -ERESTARTSYS;
    }
    /* In many cases we will return 0 to communicate that
       reader should not expect to ever receive a reply. */
    ret_val = 0;
    if (mops->state == OPS_IDLE) {
      fpdata->error = -MCE_ERR_NOT_ACTIVE;
      goto out;
    }
    /* For closed channels, a busy channel looks idle to
       all openers except the original commander. */
    if ((mops->commander->properties & MCEDEV_CLOSED_CHANNEL) &&
        (fpdata!=mops->commander)) {
      fpdata->error = -MCE_ERR_NOT_ACTIVE;
      goto out;
    }

    /* If REP/ERR is here, go process. */
    if (mops->state != OPS_CMD)
      break;
    /* If we're awaiting REP/ERR then perhaps we may block */
    up(&mops->sem);
    if (filp->f_flags & O_NONBLOCK)
      ret_val = -EAGAIN;
    PRINT_INFO(card, "read is waiting for state change.\n");
    if (wait_event_interruptible(mops->queue,
          mops->state != OPS_CMD))
      return -ERESTARTSYS;
  }
  /* Now sem is held and state is either REP or ERR. */
  PRINT_INFO(card, "clear, getting reply.\n");
  if (mops->state == OPS_REP) {
    // Fix me: partial packet reads not supported
    ret_val = sizeof(mops->rep);
    if (ret_val > count) ret_val = count;
    err = copy_to_user(buf, (void*)&mops->rep, ret_val);
    ret_val -= err;
    if (err) {
      PRINT_ERR(card, "could not copy %#x bytes to user\n", err );
      fpdata->error = -MCE_ERR_KERNEL;
    } else {
      fpdata->error = 0;
    }
  } else {
    /* Return 0 bytes and make sure reader can access error code. */
    ret_val = 0;
    fpdata->error = mops->cmd_error;
  }
  mops->state = OPS_IDLE;
  wake_up_interruptible(&mops->queue);

out:
  PRINT_INFO(card, "exiting (%i)\n", ret_val);
  up(&mops->sem);
  return ret_val;
}


int mce_write_callback( int error, mce_reply* rep, int card)
{
  struct mce_ops_t *mops = mce_ops + card;

  // Reject unexpected interrupts
  if (mops->state != OPS_CMD) {
    PRINT_ERR(card, "state is %#x, expected %#x\n",
        mops->state, OPS_CMD);
    return -1;
  }			  

  // Change state to REP or ERR, awaken readers, exit with success.
  if (error || rep==NULL) {
    PRINT_ERR(card, "called with error=-%#x, rep=%lx\n",
        -error, (unsigned long)rep);
    memset(&mops->rep, 0, sizeof(mops->rep));
    mops->state = OPS_ERR;
    mops->cmd_error = error ? error : -MCE_ERR_INT_UNKNOWN;
  } else {
    PRINT_INFO(card, "type=%#x\n", rep->ok_er);
    memcpy(&mops->rep, rep, sizeof(mops->rep));
    mops->state = OPS_REP;
    mops->cmd_error = 0;
  }

  wake_up_interruptible(&mops->queue);
  return 0;
}

ssize_t mce_write(struct file *filp, const char __user *buf, size_t count,
    loff_t *f_pos)
{
  struct filp_pdata *fpdata = filp->private_data;
  struct mce_ops_t *mops = mce_ops + fpdata->minor;
  int card = fpdata->minor;
  int err = 0;
  int ret_val = 0;

  PRINT_INFO(card, "state=%#x\n", mops->state);

  /* Don't sleep with the semaphore because this will prevent a
     reader from clearing the state. */

  while (1) {
    // Get semaphore
    if (filp->f_flags & O_NONBLOCK) {
      if (down_trylock(&mops->sem))
        return -EAGAIN;
    } else {
      if (down_interruptible(&mops->sem))
        return -ERESTARTSYS;
    }
    // Check for idle state
    if (mops->state == OPS_IDLE) {
      break;
    } 
    // Release semaphore and maybe sleep.
    up(&mops->sem);
    if (filp->f_flags & O_NONBLOCK)
      return -EAGAIN;
    if (wait_event_interruptible(mops->queue,
          mops->state == OPS_IDLE))
      return -ERESTARTSYS;
  }

  // At this point we know the state is IDLE and we hold the sem.

  // Command size check
  if (count != sizeof(mops->cmd)) {
    PRINT_ERR(card, "count != sizeof(mce_command)\n");
    ret_val = -EPROTO;
    goto out;
  }

  // Copy command to local buffer
  if ( (err=copy_from_user(&mops->cmd, buf,
          sizeof(mops->cmd)))!=0) {
    PRINT_ERR(card, "copy_from_user incomplete\n");
    ret_val = count - err;
    goto out;
  }

  // Set CMD, then go back to IDLE on failure.
  //  - leaves us vulnerable to unexpected interrupts
  //  - the alternative risks losing expected interrupts

  mops->state = OPS_CMD;
  mops->commander = fpdata;
  while (1) {
    err = mce_send_command(&mops->cmd, mce_write_callback,
        filp->f_flags & O_NONBLOCK, fpdata->minor);
    if (err != -MCE_ERR_INT_BUSY && !(filp->f_flags & O_NONBLOCK))
      break;
  }
  switch(err) {
    case 0:
      ret_val = count;
      break;
    default:
      PRINT_ERR(card, "mce_send_command failed (%i), returning %i\n",
          err, ret_val);
      mops->state = OPS_IDLE;
      fpdata->error = err;
      break;
  }

out:
  up(&mops->sem);

  PRINT_INFO(card, "exiting [%#x]\n", ret_val);
  return ret_val;
}


int mce_ioctl(struct inode *inode, struct file *filp,
    unsigned int iocmd, unsigned long arg)
{
  struct filp_pdata *fpdata = filp->private_data;
  int card = fpdata->minor;
  int x;

  switch(iocmd) {

    case MCEDEV_IOCT_RESET:
    case MCEDEV_IOCT_QUERY:
      PRINT_ERR(card, "not yet implemented!\n");
      return -1;

    case MCEDEV_IOCT_HARDWARE_RESET:
      return mce_hardware_reset(card);

    case MCEDEV_IOCT_INTERFACE_RESET:
      return mce_interface_reset(card);

    case MCEDEV_IOCT_LAST_ERROR:
      x = fpdata->error;
      fpdata->error = 0;
      return x;

    case MCEDEV_IOCT_GET:
      return fpdata->properties;

    case MCEDEV_IOCT_SET:
      fpdata->properties = (int)arg;
      return 0;

    default:
      PRINT_ERR(card, "unknown command (%#x)\n", iocmd );
  }

  return -1;
}

int mce_open(struct inode *inode, struct file *filp)
{
  struct filp_pdata *fpdata;
  int minor = iminor(inode);

  if (!mce_ready(minor)) {
    PRINT_ERR(minor, "card %i not enabled.\n", minor);
    return -ENODEV;
  }

  fpdata = kmalloc(sizeof(struct filp_pdata), GFP_KERNEL);
  fpdata->minor = iminor(inode);
  filp->private_data = fpdata;

  PRINT_INFO(minor, "mce_open %p\n", fpdata);
  return 0;
}


int mce_release(struct inode *inode, struct file *filp)
{
  struct filp_pdata *fpdata = filp->private_data;
  struct mce_ops_t *mops = mce_ops + fpdata->minor;
  int card = fpdata->minor;

  PRINT_INFO(card, "entry %p\n", fpdata);

  if(fpdata != NULL) {
    kfree(fpdata);
  } else
    PRINT_ERR(card, "called with NULL private_data\n");

  /* If a command is outstanding on this connection, re-idle the
     state so subsequent commands don't (necessarily) fail. */
  if ((fpdata->properties & MCEDEV_CLOSE_CLEANLY) && mops->state != OPS_IDLE &&
      mops->commander==fpdata) {
    PRINT_ERR(card, "closure forced, setting state to idle.\n");
    mops->state = OPS_IDLE;
    wake_up_interruptible(&mops->queue);
  }

  return 0;
}

int mce_ops_init(void)
{
  int err = 0;
  int i = 0;
  PRINT_INFO(NOCARD, "entry\n");

  err = register_chrdev(0, MCEDEV_NAME, &mce_fops);
  if (err<0) {
    PRINT_ERR(NOCARD, "mce_ops_init: could not register_chrdev, err=%#x\n",
        -err);
  } else {
    for(i=0; i<MAX_CARDS; i++) {
      struct mce_ops_t *mops = mce_ops + i;		
      mops->major = err;
    }
    err = 0;
  }

  PRINT_INFO(NOCARD, "ok\n");
  return err;
}

int mce_ops_probe(int card)
{
  struct mce_ops_t *mops = mce_ops + card;

  PRINT_INFO(card, "entry\n");

  init_waitqueue_head(&mops->queue);
  init_MUTEX(&mops->sem);

  mops->state = OPS_IDLE;

  PRINT_INFO(card, "ok\n");
  return 0;
}

int mce_ops_cleanup(void)
{
  PRINT_INFO(NOCARD, "entry\n");

  if (mce_ops->major != 0) 
    unregister_chrdev(mce_ops->major, MCEDEV_NAME);

  PRINT_INFO(NOCARD, "ok\n");
  return 0;
}
