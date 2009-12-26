#ifndef _MCE_DRIVER_H
#define _MCE_DRIVER_H

#include "kversion.h"
#include "mce/dsp.h"
#include "mce/types.h"


#define MCEDEV_NAME "mce_cmd"
#define MCEDATA_NAME "mce_data"

#define MCE_DEFAULT_TIMEOUT (HZ*100/100)

typedef int (*mce_callback)(int err, mce_reply* rep, int card);

/* Init and cleanup */

int mce_init(void);
int mce_cleanup(void);


/* Do we need these? */

int mce_error_register(int card);
void mce_error_reset(int card);


/* Function typedefs for MCE abstraction */

typedef struct {
	int (*remove)(int card);
	int (*proc)(char *buf, int count, int card);
	int (*send_command)(mce_command*, mce_callback, int, int);
	int (*hardware_reset)(int card);
	int (*interface_reset)(int card);
	int (*ready)(int card);
	int (*qt_command)(dsp_qt_code code, int arg1,
			  int arg2, int card);
} mce_interface_t;


/* Master list of MCE devices */

extern mce_interface_t *mce_interfaces[];


/* Creation of a MCE associated with an astrocam PCI card */

mce_interface_t *real_mce_create(int card, struct device *dev, int dsp_version);


/* Creation of a simple MCE emulator */

mce_interface_t *fake_mce_create(int card);
int mce_faker_init(int start, int count);

#endif
