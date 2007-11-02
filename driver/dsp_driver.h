#ifndef _DSP_DRIVER_H_
#define _DSP_DRIVER_H_

#include "dsp.h"
#include "dsp_errors.h"

#define DSPDEV_NAME "mce_dsp"

#define DSP_DEFAULT_TIMEOUT (HZ*50/100)

typedef int (*dsp_callback)(int, dsp_message*);


/* Prototypes */

int dsp_send_command(dsp_command *cmd,
		     dsp_callback callback);

int dsp_send_command_wait(dsp_command *cmd,
			  dsp_message *msg);

int dsp_driver_ioctl(unsigned int iocmd, unsigned long arg);


int dsp_int_handler(dsp_message *msg);

#endif
