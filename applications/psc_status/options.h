#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "../../defaults/config.h"

typedef struct {

  char *config_file;
  char *data_device;
  char *hardware_file;

  int fibre_card;
  int read_stdin;
} option_t;


int process_options(option_t *options, int argc, char **argv);


#endif
