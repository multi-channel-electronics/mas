#ifndef _DSPIOCTL_H_
#define _DSPIOCTL_H_

#include <linux/ioctl.h>
//#include <linux/types.h>

/* IOCTL stuff */

#define DSPIOCT_MAGIC 't'

/* Generic codes for debugging */

#define DSPIOCT_CMD0   _IO(DSPIOCT_MAGIC,  0)
#define DSPIOCT_CMD1   _IO(DSPIOCT_MAGIC,  1)
#define DSPIOCT_CMD2   _IO(DSPIOCT_MAGIC,  2)
#define DSPIOCT_CMD3   _IO(DSPIOCT_MAGIC,  3)
#define DSPIOCT_CMD4   _IO(DSPIOCT_MAGIC,  4)

/* Low level read / write of DSP PCI registers */

#define DSPIOCT_R_HRXS   _IOR(DSPIOCT_MAGIC,  128, int)
#define DSPIOCT_R_HSTR   _IOR(DSPIOCT_MAGIC,  129, int)
#define DSPIOCT_R_HCVR   _IOR(DSPIOCT_MAGIC,  130, int)
#define DSPIOCT_R_HCTR   _IOR(DSPIOCT_MAGIC,  131, int)
#define DSPIOCT_W_HTXR   _IOW(DSPIOCT_MAGIC,  132, int)
#define DSPIOCT_W_HCVR   _IOW(DSPIOCT_MAGIC,  133, int)
#define DSPIOCT_W_HCTR   _IOW(DSPIOCT_MAGIC,  134, int)

/* DSP and MCE commands and replies */

#define DSPIOCT_COMMAND         _IOW(DSPIOCT_MAGIC,  140, int)
#define DSPIOCT_GET_DSP_REPLY   _IOR(DSPIOCT_MAGIC,  141, int)
#define DSPIOCT_GET_MCE_REPLY   _IOR(DSPIOCT_MAGIC,  142, int)

/* Low-level function triggers */

//#define DSPIOCT_SET_REP_BUF     _IO(DSPIOCT_MAGIC,  150)
//#define DSPIOCT_SET_DATA_BUF    _IO(DSPIOCT_MAGIC,  151)
#define DSPIOCT_DUMP_BUF        _IO(DSPIOCT_MAGIC,  152)
//#define DSPIOCT_TRIGGER_FAKE    _IO(DSPIOCT_MAGIC,  153)

/* Special functions */

#define DSPIOCT_RESET_SOFT      _IO(DSPIOCT_MAGIC,  60)
#define DSPIOCT_RESET_DSP       _IO(DSPIOCT_MAGIC,  61)
#define DSPIOCT_RESET_MCE       _IO(DSPIOCT_MAGIC,  62)
#define DSPIOCT_GET_VERSION     _IO(DSPIOCT_MAGIC,  63)

#define DSPIOCT_DATA_LOCK       _IO(DSPIOCT_MAGIC, 64)
#define      LOCK_QUERY      0
#define      LOCK_DOWN       1
#define      LOCK_UP         2
#define      LOCK_RESET      3

/* Data frame stuff */

#define DSPIOCT_QUERY           _IO(DSPIOCT_MAGIC,  70)
#define      QUERY_HEAD      0
#define      QUERY_TAIL      1
#define      QUERY_MAX       2
#define      QUERY_PARTIAL   3
#define      QUERY_DATASIZE  4
#define      QUERY_FRAMESIZE 5
#define      QUERY_BUFSIZE   6
#define      QUERY_LTAIL     7
#define      QUERY_LPARTIAL  8
#define      QUERY_LVALID    9
#define DSPIOCT_SET_DATASIZE    _IO(DSPIOCT_MAGIC, 71)
#define DSPIOCT_FAKE_STOPFRAME  _IO(DSPIOCT_MAGIC, 72)
#define DSPIOCT_EMPTY      	_IO(DSPIOCT_MAGIC, 73)
#define DSPIOCT_QT_CONFIG  	_IO(DSPIOCT_MAGIC, 74)
#define DSPIOCT_QT_ENABLE  	_IO(DSPIOCT_MAGIC, 75)
#define DSPIOCT_FRAME_POLL 	_IO(DSPIOCT_MAGIC, 76)
#define DSPIOCT_FRAME_CONSUME   _IO(DSPIOCT_MAGIC, 77)

/* Lock */




#endif /* _DSPIOCTL_H_ */
