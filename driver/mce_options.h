/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#ifndef _MCE_OPTIONS_H_
#define _MCE_OPTIONS_H_

#include "../defaults/config.h"

#define MAX_CARDS MAX_FIBRE_CARD

#define DEVICE_NAME "mceds"


/*

   The following parameters may be set in ../defaults/config.h (via
   the ./configure script).

   FAKEMCE - replace PCI card support with the fake_mce emulator
   REALTIME - use RTAI.  This only affects the PCI card interrupt.
   BIGPHYS - use bigphysarea.  This allows a larger frame buffer.
   DRIVER_VERBOSE - log, a lot
   DRIVER_QUIET - don't even log errors

*/


/*
   DEBUG MESSAGES
   */

#define NOCARD -1
#define PRINT_MCE(level, card, fmt, ...) \
  do { \
    if (card == NOCARD) \
    printk(level DEVICE_NAME "[-](%s): " fmt, __func__, ##__VA_ARGS__); \
    else \
    printk(level DEVICE_NAME "[%i](%s): " fmt, card, __func__, ##__VA_ARGS__); \
  } while (0)

#ifdef DRIVER_QUIET
#  define PRINT_ERR(A...) //shh(A)
#else
#  define PRINT_ERR(...) PRINT_MCE(KERN_WARNING, __VA_ARGS__)
#endif

#ifdef DRIVER_VERBOSE
#  define PRINT_INFO(...) PRINT_MCE(KERN_INFO, __VA_ARGS__)
#else
#  define PRINT_INFO(...) // shh(A)
#endif

#define PRINT_IOCT(...) PRINT_MCE(KERN_INFO, __VA_ARGS__)


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
