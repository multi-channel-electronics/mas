#ifndef _DATA_QT_H_
#define _DATA_QT_H_

#include "dsp.h"

#define DSP_INFORM_RATE    10 /* Hz */
#define DSP_INFORM_COUNTS  (50000000 / DSP_INFORM_RATE) 

int  mce_qti_handler ( dsp_message *msg, unsigned long data );

void data_grant_task(unsigned long data);

int  data_qt_cmd( dsp_qt_code code, int arg1, int arg2);

int  data_qt_enable(int on);

int  data_qt_configure( int qt_interval );

#endif
