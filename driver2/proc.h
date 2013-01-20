/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#ifndef __PROC_H__
#define __PROC_H__

int read_proc(char *page, char **start, off_t offset, int count, int *eof,
	      void *data);


#endif
