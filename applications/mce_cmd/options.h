#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "cmd.h"

#define MAX_CMDLINE_CMD 32

typedef struct {

	int interactive;
	int nonzero_only;
	int no_prefix;
	int display;
	int echo;
	int use_readline;
	int acq_now;
	int fibre_card;

	int das_compatible; // horror

	char batch_file[LINE_LEN];
	int  batch_now;

	int version_only;

	char *cmd_set[MAX_CMDLINE_CMD];
	int  cmds_now;
	int  cmds_idx;

	char data_device[LINE_LEN];
	char cmd_device[LINE_LEN];
	char hardware_file[LINE_LEN];
	char masconfig_file[LINE_LEN];

	char acq_path[LINE_LEN];
	char acq_filename[LINE_LEN];

	int acq_cards;
	int acq_frames;
	int acq_interval;

	logger_t logger;

} options_t;

int process_options(options_t *options, int argc, char **argv);


#endif
