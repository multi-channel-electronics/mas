/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <mce_library.h>
#include <mce/defaults.h>

#include "options.h"

#define HEADER_OFFSET 43

#define MAXLINE 1024
#define MAXCHANNELS 8
#define MAXCOLS 32
#define MAXROWS 41
#define MAXTEMP 1024

#define SA_DAC      65536
#define SQ2_DAC     65536
#define SQ2_BAC_DAC 16386

#define RO_CARD  "ac"
#define SA_CARD  "sa"
#define SQ2_CARD "sq2"
#define SQ1_CARD "sq1"
#define ROWSEL_CARD "row"

#define ROW_ORDER "row_order"
#define SQ2_BIAS "bias"
#define SQ1_BIAS "bias"
#define SA_FB    "fb"
#define SQ2_FB   "fb"
#define SQ1_FB   "fb_const"
#define SA_FB_COL    "fb_col"
#define SQ2_FB_COL   "fb_col"
#define SQ2_FB_MUX   "enbl_mux"
#define ROWSEL_FB "select"

typedef struct {
  int fcount;
  u32 last_header[HEADER_OFFSET];
  u32 last_frame[MAXCOLS*MAXROWS];
  FILE *df;
}servo_t;

int flux_fb_set(int which_bc, int value);
int flux_fb_set_arr(int which_bc, int *arr);
int sq1fb_set(int which_rc, int value);
int sq1bias_set(int value);
int gengofile(char *datafile, char *workfile, int which_rc);
int acq(char *filename);
int error_action(char *msg, int errcode);

int load_initfile(const char *datadir, const char *filename, int start, int count, int *dest);

mce_context_t* connect_mce_or_exit(option_t* options);
int  load_param_or_exit(mce_context_t* mce, mce_param_t* p,
			const char *card, const char *para, int no_exit);
void write_range_or_exit(mce_context_t* mce, mce_param_t* p,
			 int start, i32 *data, int count,
			 const char *opmsg);
int check_fast_sq2(mce_context_t* mce, mce_param_t* sq2fb,
		   mce_param_t* sq2fb_col, int col0, int n_col);
int check_mux11d(mce_context_t* mce, mce_param_t* safb,
		   mce_param_t* safb_col, int col0, int n_col);
void duplicate_fill(i32 value, i32 *data, int count);

void rerange(i32 *dest, i32 *src, int n_data,
	     int *quanta, int n_quanta);

int genrunfile (
char *full_datafilename, /* datafilename including the path*/
char *datafile,          /* datafilename */
int  which_servo,        /* 1 for sq1servo, 2 for sq2servo*/
int  which_rc,
int  bias, int bstep, int nbias, int bias_active,
int feed, int fstep, int nfeed,
char *initline1, char *initline2, /*init lines to be included in <servo_init> section*/
int n_cols,
int servo,
double *gains,
int *quanta
);


/* experiment.cfg assist */
config_setting_t* load_config(const char *filename);
int load_int_array(config_setting_t *cfg, char *name, int start, int count, int *data);
int load_double_array(config_setting_t *cfg, char *name, int start, int count, double *data);
int load_double(config_setting_t *cfg, char *name, double *dest);
int load_int(config_setting_t *cfg, char *name, int *dest);

