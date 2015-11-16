#ifndef _DSPIOCTL_H_
#define _DSPIOCTL_H_

#include <linux/ioctl.h>
//#include <linux/types.h>

/* IOCTL stuff */

#define DSPIOCT_MAGIC 't'

/* DSP and MCE commands and replies */
#define DSPIOCT_COMMAND         _IOW(DSPIOCT_MAGIC,  1, int)
#define DSPIOCT_MCE_COMMAND     _IOW(DSPIOCT_MAGIC,  4, int)
#define DSPIOCT_GET_DSP_REPLY   _IOR(DSPIOCT_MAGIC,  2, int)
#define DSPIOCT_GET_MCE_REPLY   _IOR(DSPIOCT_MAGIC,  3, int)

/* Resets */
#define DSPIOCT_RESET_SOFT      _IO(DSPIOCT_MAGIC,  20)
#define DSPIOCT_RESET_DSP       _IO(DSPIOCT_MAGIC,  21)
#define DSPIOCT_RESET_MCE       _IO(DSPIOCT_MAGIC,  22)
#define DSPIOCT_GET_VERSION     _IO(DSPIOCT_MAGIC,  23)

/* Data frame stuff */
#define DSPIOCT_QUERY           _IO(DSPIOCT_MAGIC,  40)
#define      QUERY_HEAD      0
#define      QUERY_TAIL      1
#define      QUERY_MAX       2
#define      QUERY_DATASIZE  4
#define      QUERY_FRAMESIZE 5
#define      QUERY_BUFSIZE   6
/* #define      QUERY_LTAIL     7 */
/* #define      QUERY_LPARTIAL  8 */
/* #define      QUERY_LVALID    9 */
#define DSPIOCT_SET_DATASIZE    _IO(DSPIOCT_MAGIC, 41)
#define DSPIOCT_FAKE_STOPFRAME  _IO(DSPIOCT_MAGIC, 42)
#define DSPIOCT_EMPTY      	_IO(DSPIOCT_MAGIC, 43)
#define DSPIOCT_SET_NFRAMES     _IO(DSPIOCT_MAGIC, 44)
#define DSPIOCT_FRAME_POLL 	_IO(DSPIOCT_MAGIC, 46)
#define DSPIOCT_FRAME_CONSUME   _IO(DSPIOCT_MAGIC, 47)
#define DSPIOCT_DATA_LOCK       _IO(DSPIOCT_MAGIC, 48)
#define      LOCK_QUERY      0
#define      LOCK_DOWN       1
#define      LOCK_UP         2
#define      LOCK_RESET      3


/* Low level read / write of DSP PCI registers */
#define DSPIOCT_R_HRXS   _IOR(DSPIOCT_MAGIC,  128, int)
#define DSPIOCT_R_HSTR   _IOR(DSPIOCT_MAGIC,  129, int)
#define DSPIOCT_R_HCVR   _IOR(DSPIOCT_MAGIC,  130, int)
#define DSPIOCT_R_HCTR   _IOR(DSPIOCT_MAGIC,  131, int)
#define DSPIOCT_W_HTXR   _IOW(DSPIOCT_MAGIC,  132, int)
#define DSPIOCT_W_HCVR   _IOW(DSPIOCT_MAGIC,  133, int)
#define DSPIOCT_W_HCTR   _IOW(DSPIOCT_MAGIC,  134, int)





#endif /* _DSPIOCTL_H_ */
