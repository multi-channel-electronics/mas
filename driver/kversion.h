#ifndef _KVERSION_H_
#define _KVERSION_H_

#include <linux/version.h>

/*

 Redefine a bunch of 2.6 driver stuff for 2.4

 */


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

#else

#  define OLD_KERNEL

#endif


#ifdef OLD_KERNEL

#  define irqreturn_t void
#  define iowrite32(val, P) writel(val, P)
#  define ioread32(P) readl(P)
#  define __user
#  define IRQ_HANDLED 
#  define IRQ_NONE

#endif


#endif
