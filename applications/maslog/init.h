#ifndef _INIT_H_
#define _INIT_H_

#include "logger.h"
#include "mce/defaults.h"

#define CONFIG_SERVER  "log_server"
#define CONFIG_LOGFILE "log_file"
#define CONFIG_LOGADDR "log_address"
#define CONFIG_DAEMON  "daemon"
#define CONFIG_LEVEL   "level"

int init(params_t *p, int argc, char **argv);

int process_options(params_t *p, int argc, char **argv);
int check_usage(int argc, char **argv);


#endif
