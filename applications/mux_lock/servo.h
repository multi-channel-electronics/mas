/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdint.h>
#include <stdio.h>
#include <mce_library.h>
#include <mce/defaults.h>

#include "options.h"

#define HEADER_OFFSET 43

#define MAXLINE 1024
#define MAXCHANNELS 8
#define MAXCOLS 32
#define MAXROWS 55
#define MAXTEMP 1024
//added for mux11d hybrid RS schemes
#define MAXCARDS 8
#define MAXBCROWS 32
#define MAXACROWS 41
#define NUMBCS 3

// Servo type
enum servo_type_t {
    CLASSIC_SQ2_SERVO = 1,
    CLASSIC_SQ1_SERVO_SINGLE_ROW,
    CLASSIC_SQ1_SERVO_ALL_ROWS,
    CLASSIC_SQ1_SERVO_SA_FB,
    MUX11D_SQ1_SERVO_SA,
    MUX11D_RS_SERVO
};

/* For looking up names of bias, ramp_fb, servo_fb. */
#define STR_TABLE_NCOL 4
#define SV_DESCRIPTION 0
#define SV_BIAS  1
#define SV_FLUX  2
#define SV_SERVO 3
struct string_table {
    int id;
    const char *values[STR_TABLE_NCOL];
};

extern struct string_table servo_var_codes[];
const char *get_string(struct string_table *table, int id, int column,
                       char *def);


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
  uint32_t last_header[HEADER_OFFSET];
  uint32_t last_frame[MAXCOLS*MAXROWS];
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
        int start, int32_t *data, int count, const char *opmsg);
int check_fast_sq2(mce_context_t* mce, mce_param_t* sq2fb,
		   mce_param_t* sq2fb_col, int col0, int n_col);
int check_mux11d(mce_context_t* mce, mce_param_t* safb,
		   mce_param_t* safb_col, int col0, int n_col);
void duplicate_fill(int32_t value, int32_t *data, int count);

void rerange(int32_t *dest, int32_t *src, int n_data,
	     int *quanta, int n_quanta);

int genrunfile (
char *full_datafilename, /* datafilename including the path*/
char *datafile,          /* datafilename */
enum servo_type_t servo_type,
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
int load_int_if_present(config_setting_t *cfg, char *name, int *dest);

// hybrid mux11d
int load_hybrid_rs_cards(config_setting_t *cfg, char *data[MAXCARDS]);//, int start, int count, int *data);
int validate_hybrid_mux11d_mux_order(int nhybrid_rs_cards,
                                     char *mux11d_row_select_cards[MAXCARDS],
                                     int *mux11d_row_select_cards_row0);
