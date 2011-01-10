#ifndef _MAS_PARAM_H_
#define _MAS_PARAM_H_

#include "../../defaults/config.h"
#include <libconfig.h>
#include <mce/defaults.h>
#include <libmaslog.h>

#define PROGRAM_NAME           "mas_param"

#define MCE_LONG 1024

#define TEXT_SIZE 16384 /* max data output size */

/* Modes */
enum { MODE_IDLE, MODE_CRAWL, MODE_GET, MODE_SET, MODE_INFO };

/* Output formats */
enum { FORMAT_BASH = 0,
       FORMAT_CSH,
       FORMAT_IDLTEMPLATE,
       FORMAT_INFO,
       FORMAT_FULL };


/* options structure and processor */

typedef struct {
	char *config_file; // MAS config file

	char *source_file; // target file for processing
	char output_path[MCE_LONG];
	char output_file[MCE_LONG];

	int  output_on;

	char output_word[MCE_LONG]; // mode-dependent

	int mode;
	int format;
	int fibre_card;
	
	char *param_name;           // For get and set operations
	char **data_source;         // For set operations
	int data_count;             //  "
	
	config_setting_t *root;

} options_t;

int process_options(options_t *options, int argc, char **argv);

enum { CFG_INT, CFG_STR, CFG_DBL };


/* Translation of config parameter data */

typedef struct {
	int array;
	int count;
	int type;

	int        *data_i;
	double     *data_d;
	const char *data_s;
	const char *data_name;
	char *name;
	
	config_setting_t* cfg;
} mas_param_t;

#include "get.h"
#include "crawl.h"
#include "bash.h"
#include "csh.h"
#include "idl.h"
#include "info.h"

#endif

