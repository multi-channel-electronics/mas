/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _DSP_STATE_H_
#define _DSP_STATE_H_

#include <kversion.h>
#include <mce_options.h>

#ifdef FOLD_KERNEL
#  include <linux/tqueue.h>
#else
#  include <linux/interrupt.h>
#endif

#include "mce/dsp.h"
#include "dsp_state.h"

/* Internal MCE clock rate */
#define MCE_CLOCK      50000000

/* You can set this to emulate 0x550102, 0x550103, 0x550104 */
#define FAKE_DSP_VERSION 0x550104

#define PSIZE 2048
#define XSIZE 2048
#define YSIZE 2048

#define CUE 10

typedef int (*fake_int_handler_t)(dsp_message *msg);

typedef struct dsp_state_struct {

    u32 p[PSIZE];
    u32 x[XSIZE];
    u32 y[YSIZE];

    fake_int_handler_t fake_int_handler;

    dsp_message msg_q[10];
    int msg_r;
    int msg_w;

    int trigger_timeout;

    void *mce_con;

    struct tasklet_struct msg_tasklet;

    struct {
        int enabled;
        int delta;
        int number;
        int inform;
        int period;
        int size;
        int tail;
        int head;
        int drops;
        void *base;
    } qt_data;

    int n_frames;
    int delta_jiffies;
    int delta_frames;

    struct timer_list timer;

} dsp_state_t;

extern dsp_state_t dsp_state;

int dsp_state_init( void );
int dsp_state_cleanup( void );

int dsp_state_set_handler(fake_int_handler_t fih);

int dsp_state_message(dsp_message *msg, int schedule);

int dsp_state_command(dsp_command *cmd);

int dsp_state_nfy_da(int size);
int dsp_state_nfy_rp(int size);

int dsp_retdat_callback(int frame_size, int ticks, int nframes);
void dsp_timer_function(unsigned long arg);

#endif
