/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#ifndef __PROC_H__
#define __PROC_H__

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

int mce_proc(struct seq_file *sfile, void *data);
int dsp_proc(struct seq_file *sfile, void *data);
int data_proc(struct seq_file *sfile, void *data);

extern const struct file_operations mcedsp_proc_ops;


#endif
