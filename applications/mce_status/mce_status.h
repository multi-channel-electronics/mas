#ifndef _MCE_STATUS_H_
#define _MCE_STATUS_H_

#include <mce_library.h>

#define PROGRAM_NAME           "mce_status"


enum { CRAWLER_DAS = 0,
       CRAWLER_MAS,
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

	mce_context_t* context;

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
