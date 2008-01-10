#ifndef _CMD_H_
#define _CMD_H_

#define PROGRAM_NAME "mce_cmd"

#define LINE_LEN 1024
#define NARGS 256
#define DATA_SIZE 2048

#define FS_DIGITS 3 /* Number of digits in file sequencing file name */

#define DEFAULT_DEVICE "/dev/mce_cmd0"
#define DEFAULT_DATA "/dev/mce_data0"
#define DEFAULT_XML "/etc/mce.cfg"

enum { ERR_MEM=1,
       ERR_OPT=2,
       ERR_MCE=3 };

// This structure is used to cache data which eventually constructs acq.

struct my_acq_struct {
	char filename[LINE_LEN];
	int  cards;
	int  rows;
	int  n_frames;
	int  interval;
};

extern struct my_acq_struct my_acq;


#endif
