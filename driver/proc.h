#ifndef __PROC_H__
#define __PROC_H__

int read_proc(char *page, char **start, off_t offset, int count, int *eof,
	      void *data);


#endif
