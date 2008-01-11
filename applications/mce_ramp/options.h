#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "ramp.h"
#define LINE_LEN 1024

typedef struct {

	char batch_file[LINE_LEN];
	int  batch_now;

	char cmd_command[LINE_LEN];
	int  cmd_now;

	char data_device[LINE_LEN];
	char cmd_device[LINE_LEN];
	char config_file[LINE_LEN];

	char acq_path[LINE_LEN];

	int acq_frames;
	int acq_now;

	amble_t* ambles;
	loop_t* loops;
	value_t* values;
	operation_t* operations;

	amble_t* preambles;
	amble_t* postambles;
  
	int preamble_count;
	int postamble_count;

} options_t;

int process_options(options_t *options, int argc, char **argv);


#endif
