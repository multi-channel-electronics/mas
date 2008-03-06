#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "cmd.h"

typedef struct {

	int interactive;
	int nonzero_only;
	int no_prefix;
	int display;
	int echo;
	int use_readline;
	int acq_now;

	int das_compatible; // horror

	char batch_file[LINE_LEN];
	int  batch_now;

	char cmd_command[LINE_LEN];
	int  cmd_now;

	char data_device[LINE_LEN];
	char cmd_device[LINE_LEN];
	char config_file[LINE_LEN];

	char acq_path[LINE_LEN];
	char acq_filename[LINE_LEN];

	int acq_cards;
	int acq_frames;
	int acq_interval;
} options_t;

int process_options(options_t *options, int argc, char **argv);


#endif
