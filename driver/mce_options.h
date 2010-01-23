#ifndef _MCE_OPTIONS_H_
#define _MCE_OPTIONS_H_

#define MAX_CARDS 2

#define DEVICE_NAME "mceds"


/*

  The following parameters may be set in the Makefile to control
  certain behaviours. Some defaults are set through the ./configure
  script.

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

#  define FRAME_BUFFER_SIZE (1024*1024*4)

#else

#  define FRAME_BUFFER_SIZE 10e6

#endif


/*
  FAKE_MCE_COUNT, FAKE_MCE_MINOR - number and minor offset of any
  emulators.
*/

#define FAKE_MCE_COUNT 1
#define FAKE_MCE_MINOR 0

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
