#ifndef _DATA_WATCHER_H_
#define _DATA_WATCHER_H_

#define MAX_WATCH 1024

struct buffer_watcher {
	int freq[MAX_WATCH];
	int max;  
	int on;
};

extern struct buffer_watcher watcher;

void watcher_file(int idx);

void watcher_reset(void);

void watcher_start(void);

void watcher_stop(void);

int watcher_size(void);

int watcher_dump(int *dest, int user);

int watcher_ioctl(int iocmd, int arg);

#endif
