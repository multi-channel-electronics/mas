#ifndef _MCE_DRIVER_H
#define _MCE_DRIVER_H

#include "kversion.h"
#include "mce/dsp.h"
#include "mce/types.h"


#define MCEDEV_NAME "mce_cmd"
#define MCEDATA_NAME "mce_data"

#define MCE_DEFAULT_TIMEOUT (HZ*100/100)

typedef int (*mce_callback)(int err, mce_reply* rep, int card);

int mce_ready(int card);

int mce_init(void);

int mce_probe(int card, int dsp_version);

int mce_cleanup(void);

int mce_remove(int card);

int mce_proc(char *buf, int count, int card);

int mce_error_register(int card);

void mce_error_reset(int card);

int mce_send_command( mce_command *cmd,
		      mce_callback callback,
		      int nonblock, int card );

int mce_qt_command( dsp_qt_code code, int arg1, 
		    int arg2, int card);

int mce_get_reply( __user void* reply_user,
		   void* reply_kern, int count );

int mce_clear_commflags(void);

int mce_interface_reset(int card);

int mce_hardware_reset(int card);

#endif
