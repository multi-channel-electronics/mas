#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "mce_cmd.h"

struct option_struct {

	int interactive;
	int nonzero_only;
	int no_prefix;
	int display;
	int echo;

	int das_compatible; // horror

	char batch_file[LINE_LEN];
	int  batch_now;

	char cmd_command[LINE_LEN];
	int  cmd_now;

	char device_file[LINE_LEN];
	char config_file[LINE_LEN];

	char acq_path[LINE_LEN];

	int acq_frames;
	int acq_now;
};

int process_options(int argc, char **argv);


#endif
