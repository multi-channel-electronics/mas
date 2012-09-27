/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
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

	char batch_file[LINE_LEN];
	int  batch_now;

	int version_only;

	char *cmd_set[MAX_CMDLINE_CMD];
	int  cmds_now;
	int  cmds_idx;

	char *hardware_file;
	char *masconfig_file;

	char acq_path[LINE_LEN];
	char acq_filename[LINE_LEN];

	int acq_cards;
	int acq_frames;
	int acq_interval;

    char symlink[LINE_LEN];
    char dirfile_include[LINE_LEN];
    int dirfile_spf;
    int dirfile_version;

	maslog_t *logger;

} options_t;

int process_options(options_t *options, int argc, char **argv);


#endif
