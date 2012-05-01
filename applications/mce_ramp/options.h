/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "ramp.h"
#define LINE_LEN 1024

typedef struct {

	char output_file[LINE_LEN];
	int  output_file_now;

    char *mas_file;
    int fibre_card;

	int status_block;

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
