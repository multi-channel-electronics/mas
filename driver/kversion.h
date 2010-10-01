/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#ifndef _KVERSION_H_
#define _KVERSION_H_

#include <linux/version.h>
#include <linux/interrupt.h>

/* This header file is used to control differences between kernel
   versions.  Currently we distinguish between 3 possibilities:
    - 2.4 series
    - 2.6 series up to 2.6.18
    - 2.6 series from 2.6.19

   Since the newer stuff is usually better, we should think of the
   driver as being written for the latest kernels, with special steps
   taken for backwards compatibility.
*/


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#  define OLD_KERNEL

#endif


#ifdef OLD_KERNEL

typedef void irqreturn_t;
#  define iowrite32(val, P) writel(val, P)
#  define ioread32(P) readl(P)
#  define __user
#  define IRQ_HANDLED 
#  define IRQ_NONE

#endif


/* 2.6.19 changed the arguments to interrupt handlers, and defined the
   type irq_handler_t (linux/interrupt.h) as
      
     typedef irqreturn_t (*irq_handler_t)(int, void *);

   Earlier kernels, including 2.4 series, expect handler functions like

     typedef irqreturn_t (*irq_handler_t)(int,void*,struct pt_regs*);
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)

typedef irqreturn_t (*irq_handler_t)(int,void*,struct pt_regs*);

#endif


#endif
