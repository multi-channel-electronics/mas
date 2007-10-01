#ifndef _MCE_OPTIONS_H_
#define _MCE_OPTIONS_H_


#define DEVICE_NAME "mceds"


/*
  DEBUG MESSAGES
*/

//#define OPT_VERBOSE
//#define OPT_QUIET

#ifdef OPT_QUIET
#  define PRINT_ERR(A...) //shh(A)
#else
#  define PRINT_ERR(A...) printk(KERN_WARNING DEVICE_NAME ": " A)
#endif

#ifdef OPT_VERBOSE
#  define PRINT_INFO(A...) printk(KERN_INFO DEVICE_NAME ": " A)
#else
#  define PRINT_INFO(A...) // shh(A)
#endif

#define PRINT_IOCT(A...) printk(KERN_INFO A)


/*
  FAKEMCE - replace PCI card support with the fake_mce emulator
*/

//#define FAKEMCE


/*
  REALTIME - use RTAI.  This only affects the PCI card interrupt.
*/

//#define REALTIME


/*
  BIGPHYS - use bigphysarea.  This allows a larger frame buffer.
*/

//#define BIGPHYS


/*
  FRAME_BUFFER_SIZE - must be reduced if BIGPHYS is not used
*/

#ifndef BIGPHYS

# define FRAME_BUFFER_SIZE 128000

#else

# define FRAME_BUFFER_SIZE 10e6

#endif


/*
  WATCHER - adds functionality for characterizing buffer usage.
  Adds a fast function call to data_frame_increment, and enables the
  WATCH options in mce_data IOCTL.

  Source file: data_watch.c, data_watch.h
*/

#define OPT_WATCHER


#endif
