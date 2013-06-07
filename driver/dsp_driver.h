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

typedef struct {
	volatile
        void*     base;
	unsigned long base_busaddr;
        size_t    size;

        int start_frame;
        int end_frame;
} dsp_memblock_t;

/* This is a DSP firmware max; do not increase beyond 20. */
#define DSP_MAX_MEM_BLOCKS 20

typedef struct {

	/* volatile */
        /* void*     base; */
	/* unsigned long base_busaddr; */
        /* size_t    size; */

        int n_blocks;
        dsp_memblock_t blocks[DSP_MAX_MEM_BLOCKS];

        int n_frames;
        int frame_size;
        int data_size;
        int total_size;

        // New data is written at head, consumer data is read at tail.
	volatile
	int       head_index;
	volatile
	int       tail_index;
        
        int last_grant;
        int update_interval;
        int qt_configs;

	// Semaphore should be held when modifying structure, but
	// interrupt routines may modify head_index at any time.

        spinlock_t lock;
	wait_queue_head_t queue;

	// Device lock - controls read access on data device and
	// provides a system for checking whether the system is
	// mid-acquisition.  Should be NULL (for idle) or pointer to
        // reader's filp (for locking).

	void *data_lock;	

} frame_buffer_t;

#define DSP_INTERNAL_FREQ  50e6
#define DSP_INFORM_RATE    10 /* Hz */
#define DSP_INFORM_COUNTS  (DSP_INTERNAL_FREQ / DSP_INFORM_RATE) 

#endif
