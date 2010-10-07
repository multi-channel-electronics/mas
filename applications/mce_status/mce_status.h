#ifndef _MCE_STATUS_H_
#define _MCE_STATUS_H_

#include <mce_library.h>
#include <mce/defaults.h>

#define PROGRAM_NAME           "mce_status"


enum { CRAWLER_DAS = 0,
       CRAWLER_MAS,
       CRAWLER_CFG };


/* options structure and processor */

typedef struct {
	int fibre_card;
	char *device_file;
	char *config_file;
	char *hardware_file;
	char output_path[MCE_LONG];

	char output_file[MCE_LONG];
	int  output_on;

	int mode;

	mce_context_t* context;

} options_t;

int process_options(options_t *options, int argc, char **argv);


/* config crawler actions */

typedef struct {

	int (*init)(unsigned long user_data, const options_t* options);
	int (*item)(unsigned long user_data, const mce_param_t *m);
	int (*cleanup)(unsigned long user_data);

	unsigned long user_data;

} crawler_t;


#endif
