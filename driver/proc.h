/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#ifndef __PROC_H__
#define __PROC_H__

int read_proc(char *page, char **start, off_t offset, int count, int *eof,
    void *data);


#endif
