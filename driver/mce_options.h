#ifndef _MCE_OPTIONS_H_
#define _MCE_OPTIONS_H_


#define DEVICE_NAME "mceds"


/*

  These parameters are passed in from the Makefile; instead of
  enabling them here, change the default value in Makefile or set a
  local override in Makefile.local; e.g.
          FAKEMCE ?= 1

  FAKEMCE - replace PCI card support with the fake_mce emulator
  REALTIME - use RTAI.  This only affects the PCI card interrupt.
  BIGPHYS - use bigphysarea.  This allows a larger frame buffer.
  OPT_VERBOSE - log, a lot
  OPT_QUIET - don't even log errors

*/


/*
  DEBUG MESSAGES
*/

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
  FRAME_BUFFER_SIZE - must be reduced if BIGPHYS is not used
*/

#ifndef BIGPHYS

#  define FRAME_BUFFER_SIZE (32*1024*1024)

#else

#  define FRAME_BUFFER_SIZE 10e6

#endif


/* DEFAULT_DATA_SIZE - doesn't affect very much, just the initial
 * frame size.  5424, as the current MCE maximum, is fine. */

#define DEFAULT_DATA_SIZE   5424


/*
  WATCHER - adds functionality for characterizing buffer usage.
  Adds a fast function call to data_frame_increment, and enables the
  WATCH options in mce_data IOCTL.

  Source file: data_watch.c, data_watch.h
*/

#define OPT_WATCHER


#endif
