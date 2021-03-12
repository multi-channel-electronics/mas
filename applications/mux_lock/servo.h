#include <stdio.h>
#include <mce_library.h>

#include "options.h"

#define HEADER_OFFSET 43

#define MAXLINE 1024
#define MAXVOLTS 32     /* use 32 even on a small subrack! */
#define MAXCHANNELS 8
#define MAXROWS 41
#define MAXTEMP 1024

#define RO_CARD  "ac"
#define SA_CARD  "sa"
#define SQ2_CARD "sq2"
#define SQ1_CARD "sq1"

#define ROW_ORDER "row_order"
#define SQ2_BIAS "bias"
#define SQ1_BIAS "bias"
#define SA_FB    "fb"
#define SQ2_FB   "fb"
#define SQ1_FB   "fb_const"
#define SQ2_FB_COL   "fb_col"

typedef struct {
  int fcount;
  int row_num[MAXVOLTS];
  int row_data[MAXVOLTS];
  u32 last_header[HEADER_OFFSET];
  u32 last_frame[MAXVOLTS*MAXROWS];
  int which_rc;
  int num_rows;
    
  FILE *df;
}servo_t;

int flux_fb_set(int which_bc, int value);
int flux_fb_set_arr(int which_bc, int *arr);
int sq1fb_set(int which_rc, int value);
int sq1bias_set(int value);
int gengofile(char *datafile, char *workfile, int which_rc);
int acq(char *filename);
int error_action(char *msg, int errcode);

mce_context_t* connect_mce_or_exit(option_t* options);
int  load_param_or_exit(mce_context_t* mce, mce_param_t* p,
			const char *card, const char *para, int no_exit);
void write_range_or_exit(mce_context_t* mce, mce_param_t* p,
			 int start, u32 *data, int count,
			 const char *opmsg);
void duplicate_fill(u32 value, u32 *data, int count);


int genrunfile (
char *full_datafilename, /* datafilename including the path*/
char *datafile,          /* datafilename */
int  which_servo,        /* 1 for sq1servo, 2 for sq2servo*/
int  which_rc,
int  bias, int bstep, int nbias, int feed, int fstep, int nfeed,
char *initline1, char *initline2 /*init lines to be included in <servo_init> section*/
);

