#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#define OPTSTR 1024

typedef struct {

  char config_file[OPTSTR];
  char cmd_device[OPTSTR];
  char data_device[OPTSTR];
  char hardware_file[OPTSTR];
  int preservo;

} option_t;


int process_options(option_t *options, int argc, char **argv);


#endif
