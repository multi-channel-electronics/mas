/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#include <linux/kernel.h>
#include <asm/uaccess.h>

#include "mce/data_ioctl.h"
#include "data_watcher.h"

struct buffer_watcher watcher;

void watcher_reset() {
  memset(&watcher, 0, sizeof(watcher));
}

void watcher_file(int idx) {
  if (idx>=0 || idx<MAX_WATCH) watcher.freq[idx]++;
  else watcher.freq[MAX_WATCH-1]++;
  if (idx>watcher.max) watcher.max = idx;

  if (idx==MAX_WATCH/4) 
    printk("data_watcher: 1/4 watch\n");
  if (idx==MAX_WATCH/2) 
    printk("data_watcher: 2/4 watch\n");
  if (idx==3*MAX_WATCH/4) 
    printk("data_watcher: 3/4 watch\n");
}

void watcher_start() {
  watcher.on = 1;
  if (1) {

  }
}
void watcher_stop() {
  watcher.on = 0;
}
int watcher_size() {
  return MAX_WATCH;
}

int watcher_dump(int *dest, int user) {
  int n = 0;

  if (user)
    n = copy_to_user(dest, watcher.freq,
        MAX_WATCH * sizeof(int));
  else			     
    memcpy(dest, watcher.freq,
        MAX_WATCH * sizeof(int));

  return watcher.max;
}

int watcher_ioctl(int iocmd, int arg)
{
  switch (iocmd)	{

    case DATADEV_IOCT_WATCH:

      switch (arg) {

        case WATCH_START:
          watcher_start();
          break;
        case WATCH_STOP:
          watcher_stop();
          break;
        case WATCH_RESET:
          watcher_reset();
          break;
        case WATCH_SIZE:
          return watcher_size();
        default:
          return -1;
      }
      return 0;

    case DATADEV_IOCT_WATCH_DL:
      // Probably not x64 safe!
      //return watcher_dump((int*)arg, 1 /*user*/);
      return -1;
  }
  return -1;
}
