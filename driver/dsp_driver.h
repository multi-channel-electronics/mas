/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#ifndef _DSP_DRIVER_H_
#define _DSP_DRIVER_H_

#include <linux/interrupt.h>
#include "mce/new_dsp.h"
#include "mce/dsp_errors.h"

#define DSPDEV_NAME "mce_dsp"
#define DSP_DEFAULT_TIMEOUT (HZ*500/1000)

/* DSP code versions */

#define DSP_U0103          0x550103
#define DSP_U0104          0x550104
#define DSP_U0105          0x550105
#define DSP_U0106          0x550106


/* DSP PCI vendor/device id */

#define DSP_VENDORID 0x1057
#define DSP_DEVICEID 0x1801

/* DSP interrupt vectors */

#define HCVR_HC          0x0001               /* Do-this-now bit */
#define HCVR_HNMI        0x8000               /* Non-maskable HI-32 interrupt */

/* HCTR default */

#define DSP_PCI_MODE_BASE        0x000900    /* for 32->24 bit conversion */
#define DSP_PCI_MODE_HANDSHAKE   HCTR_HF1
#define DSP_PCI_MODE_NOIRQ       HCTR_HF2


#define PCI_MAX_FLUSH       256

/* Soft interrupt generation timer frequency */

#define DSP_POLL_FREQ       100
#define DSP_POLL_JIFFIES    (HZ / DSP_POLL_FREQ + 1)


#endif
