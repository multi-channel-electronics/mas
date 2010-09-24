#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "../../defaults/config.h"

#define OPTSTR 1024

typedef struct {

  char config_file[OPTSTR];
  char cmd_device[OPTSTR];
  char data_device[OPTSTR];
  char hardware_file[OPTSTR];
  char experiment_file[OPTSTR];

  int fibre_card;
  int argument_opts;
  int preservo;

  int unwrap_sa_quanta;

} option_t;


int process_options(option_t *options, int argc, char **argv);

void usage();

#endif
