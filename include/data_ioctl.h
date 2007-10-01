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

#endif
