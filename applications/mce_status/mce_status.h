#ifndef _MCE_STATUS_H_
#define _MCE_STATUS_H_

#include <mcecmd.h>
#include <mceconfig.h>
#include <libmaslog.h>

#include "mce.h"

enum { CRAWLER_DAS = 0,
       CRAWLER_CFG };


/* options structure and processor */

typedef struct {
	char device_file[MCE_LONG];
	char config_file[MCE_LONG];
	char hardware_file[MCE_LONG];
	char output_path[MCE_LONG];

	char output_file[MCE_LONG];
	int  output_on;

	int mode;

	int handle;
	mceconfig_t *mce;

} options_t;

int process_options(options_t *options, int argc, char **argv);


/* config crawler actions */

typedef struct {

	int (*init)(unsigned user_data, const options_t* options);
	int (*item)(unsigned user_data, const mce_param_t *m);
	int (*cleanup)(unsigned user_data);

	unsigned user_data;

} crawler_t;


#endif
