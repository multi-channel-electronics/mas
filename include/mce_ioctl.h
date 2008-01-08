#ifndef _MCE_IOCTL_H_
#define _MCE_IOCTL_H_

/* IOCTL stuff */

#include <linux/ioctl.h>

#define MCEDEV_IOC_MAGIC 'r'

#define MCEDEV_IOCT_RESET             _IO(MCEDEV_IOC_MAGIC,  0)

#define MCEDEV_IOCT_QUERY             _IO(MCEDEV_IOC_MAGIC,  1)
  
#define MCEDEV_IOCT_HARDWARE_RESET    _IO(MCEDEV_IOC_MAGIC,  2)

#define MCEDEV_IOCT_INTERFACE_RESET   _IO(MCEDEV_IOC_MAGIC,  3)

#define MCEDEV_IOCT_LAST_ERROR        _IO(MCEDEV_IOC_MAGIC,  4)

#endif
