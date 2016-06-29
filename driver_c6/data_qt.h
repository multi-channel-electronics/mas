/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#ifndef _DATA_QT_H_
#define _DATA_QT_H_

#include "autoversion.h"
#include "mce/dsp.h"

#define DSP_INTERNAL_FREQ  50e6
#define DSP_INFORM_RATE    10 /* Hz */
#define DSP_INFORM_COUNTS  (DSP_INTERNAL_FREQ / DSP_INFORM_RATE) 

int  mce_qti_handler (dsp_message *msg, unsigned long data);

void data_grant_task(unsigned long data);

int  data_qt_enable(int on, int card);

int  data_qt_configure(int qt_interval, int card);

#endif
