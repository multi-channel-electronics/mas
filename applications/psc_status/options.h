/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "../../defaults/config.h"

typedef struct {

    char *config_file;
    char *hardware_file;

    int fibre_card;
    int read_stdin;
} option_t;


int process_options(option_t *options, int argc, char **argv);


#endif
