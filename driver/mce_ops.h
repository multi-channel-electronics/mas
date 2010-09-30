/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#ifndef _MCE_OPS_H_
#define _MCE_OPS_H_


extern struct file_operations mce_fops;

/* Unnecessary prototypes */

ssize_t mce_read(struct file *filp, char __user *buf, size_t count,
    loff_t *f_pos);

ssize_t mce_write(struct file *filp, const char __user *buf, size_t count,
    loff_t *f_pos);

int mce_ioctl(struct inode *inode, struct file *filp,
    unsigned int iocmd, unsigned long arg);

int mce_open(struct inode *inode, struct file *filp);

int mce_release(struct inode *inode, struct file *filp);

int mce_ops_init(void);

int mce_ops_probe(int card);

int mce_ops_cleanup(void);

#endif
