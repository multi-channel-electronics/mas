#ifndef _INIT_H_
#define _INIT_H_

#include "data.h"

#define CONFIG_FILE "/etc/mas.cfg"

#define CONFIG_SERVER  "data_server"

#define CONFIG_ADDR    "address"
#define CONFIG_DAEMON  "daemon"
#define CONFIG_MCECMD  "mce_cmd"
#define CONFIG_MCEDATA "mce_data"
#define CONFIG_MCECFG  "mce_cfg"
#define CONFIG_MCESEQCARD "mce_sequence_card"
#define CONFIG_MCESEQCMD  "mce_sequence_command"
#define CONFIG_MCEGOCMD   "mce_go_command"
#define CONFIG_FORMAT  "format"
#define CONFIG_DIR     "working_directory"
#define CONFIG_DATAFILE "data_file_options"

#define CONFIG_HEADER  "header"
#define CONFIG_EXTRA   "extra"
#define CONFIG_ROWS    "rows"
#define CONFIG_COLUMNS "columns"
#define CONFIG_COLUMNSCARD "columns_per_card"
#define CONFIG_COLOPT  "column_options"
#define CONFIG_COLSEL  "select"
#define CONFIG_COLCRD  "cards"
#define CONFIG_COLCRDI "card_index"

#define CONFIG_FILEMODE "file_mode"
#define CONFIG_BASENAME "base_name"
#define CONFIG_FRAMEINT "frame_interval"


int init(params_t *p, int argc, char **argv);

int load_config(params_t *p, char *config_file);
int process_options(params_t *p, int argc, char **argv);
int get_config_file(char *filename, int argc, char **argv);
int check_usage(int argc, char **argv);
int mceids_config(params_t *p);


#endif
