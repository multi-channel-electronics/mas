#ifndef _DATA_IOCTL_H_
#define _DATA_IOCTL_H_

/* IOCTL stuff */

#include <linux/ioctl.h>

#define DATADEV_IOC_MAGIC 'q'


#define DATADEV_IOCT_RESET      _IO(DATADEV_IOC_MAGIC,  0)

#define DATADEV_IOCT_QUERY      _IO(DATADEV_IOC_MAGIC,  1)
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

  
/* for use by data_watcher routine */
#define DATADEV_IOCT_WATCH      _IO(DATADEV_IOC_MAGIC,  2)
#define      WATCH_START     0
#define      WATCH_STOP      1
#define      WATCH_RESET     2
#define      WATCH_SIZE      3

#define DATADEV_IOCT_WATCH_DL   _IO(DATADEV_IOC_MAGIC,  3)

#define DATADEV_IOCT_SET_DATASIZE _IO(DATADEV_IOC_MAGIC, 4)

#define DATADEV_IOCT_FAKE_STOPFRAME _IO(DATADEV_IOC_MAGIC, 5)

#define DATADEV_IOCT_EMPTY      _IO(DATADEV_IOC_MAGIC, 6)

#define DATADEV_IOCT_QT_CONFIG  _IO(DATADEV_IOC_MAGIC, 7)

#define DATADEV_IOCT_QT_ENABLE  _IO(DATADEV_IOC_MAGIC, 8)

#define DATADEV_IOCT_FRAME_POLL _IO(DATADEV_IOC_MAGIC, 9)

#define DATADEV_IOCT_FRAME_CONSUME _IO(DATADEV_IOC_MAGIC, 10)

#define DATADEV_IOCT_LOCK       _IO(DATADEV_IOC_MAGIC, 11)
#define      LOCK_QUERY      0
#define      LOCK_DOWN       1
#define      LOCK_UP         2
#define      LOCK_RESET      3

/* Behaviours */
#define DATADEV_IOCT_GET           _IO(DATADEV_IOC_MAGIC, 12)
#define DATADEV_IOCT_SET           _IO(DATADEV_IOC_MAGIC, 13)
#define      DATA_LEECH      (1 << 0)  /* Secondary data reader */

/* timestamping */
#define DATADEV_IOCT_TIMESTAMP_ENABLE _IO(DATADEV_IOC_MAGIC, 14)

#endif
