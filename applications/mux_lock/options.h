/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _OPTIONS_H_
#define _OPTIONS_H_

typedef struct {

    char *config_file;
    char *hardware_file;
    char *experiment_file;

    int fibre_card;
    int argument_opts;
    int preservo;

    int unwrap_sa_quanta;

} option_t;


int process_options(option_t *options, int argc, char **argv);

void usage();

#endif
