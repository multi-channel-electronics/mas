#ifndef _INIT_H_
#define _INIT_H_

#include "logger.h"

#define CONFIG_FILE "/etc/mce/mas.cfg"

#define CONFIG_SERVER  "log_server"
#define CONFIG_LOGFILE "log_file"
#define CONFIG_LOGADDR "log_address"
#define CONFIG_DAEMON  "daemon"
#define CONFIG_LEVEL   "level"

int init(params_t *p, int argc, char **argv);

int load_config(params_t *p, char *config_file);
int process_options(params_t *p, int argc, char **argv);
int get_config_file(char *filename, int argc, char **argv);
int check_usage(int argc, char **argv);


#endif
