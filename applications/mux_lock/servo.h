#include <stdio.h>
#include <mce_library.h>

#define CMD_DEVICE "/dev/mce_cmd0"
#define DATA_DEVICE "/dev/mce_data0"
#define CONFIG_FILE "/etc/mce.cfg"

#define HEADER_OFFSET 43

#define MAXLINE 1024
#define MAXVOLTS 32
#define MAXCHANNELS 8
#define MAXROWS 41

#define SAFB_CARD "bc1"
#define SQ2FB_CARD "bc2"

#define SQ2BIAS_CARD "bc3"     //subrack v1 specific
#define SQ2BIAS_CMD "flux_fb"  //subrack v1 specific

#define SQ1BIAS_CARD "ac"
#define SQ1BIAS_CMD "on_bias"

typedef struct {
  int fcount;
  int row_num[MAXVOLTS];
  int row_data[MAXVOLTS];
  int which_rc;
  FILE *df;
}servo_t;

int flux_fb_set(int which_bc, int value);
int flux_fb_set_arr(int which_bc, int *arr);
int sq1fb_set(int which_rc, int value);
int sq1bias_set(int value);
int gengofile(char *datafile, char *workfile, int which_rc);
int acq(char *filename);
int error_action(char *msg, int errcode);

int genrunfile (
char *full_datafilename, /* datafilename including the path*/
char *datafile,          /* datafilename */
int  which_servo,        /* 1 for sq1servo, 2 for sq2servo*/
int  which_rc,
int  bias, int bstep, int nbias, int feed, int fstep, int nfeed,
char *initline1, char *initline2 /*init lines to be included in <servo_init> section*/
);

