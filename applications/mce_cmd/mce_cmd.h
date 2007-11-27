#ifndef _MCE_CMD_H_
#define _MCE_CMD_H_

#include <mce.h>
#include <mcecmd.h>

#define LINE_LEN 1024
#define NARGS 64

#define FS_DIGITS 3 /* Number of digits in file sequencing file name */

#define DEFAULT_DEVICE "/dev/mce_cmd0"
#define DEFAULT_DATA "/dev/mce_data0"
#define DEFAULT_XML "/etc/mce.cfg"

#define FRAME_HEADER 43
#define FRAME_FOOTER 1
#define FRAME_COLUMNS 8

enum { ERR_MEM=1,
       ERR_OPT=2,
       ERR_MCE=3 };

extern struct option_struct options;

// This structure is used to cache data which eventually constructs acq.

struct my_acq_struct {
	char filename[LINE_LEN];
	int  frame_size;
	int  cards;
	int  rows;
	int  n_frames;
	int  interval;
};

extern struct my_acq_struct my_acq;


#endif
