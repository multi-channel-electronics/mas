#ifndef __PROC_H__
#define __PROC_H__

int read_proc(char *page, char **start, off_t offset, int count, int *eof,
	      void *data);

typedef int (*proc_f)(char *buf, int count, int card);

typedef struct {
	proc_f dsp;
	proc_f data;
	proc_f mce;
} mceds_proc_t;

extern mceds_proc_t mceds_proc[MAX_CARDS];

#endif
