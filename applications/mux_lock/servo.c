/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <mce/defaults.h>
#include "servo_err.h"
#include "servo.h"
#include "options.h"

/******************************************************************
 * servo.c contains subroutines called by sq1servo.c and sq2servo.c
 * 
 * Contains subroutines used by sq1servo and sq2servo, especially
 * generic utility calls for generating runfiles and connecting to
 * the MCE library.
 *
 ******************************************************************/

int error_action(char *msg, int code){
    char temp[1024];
  
    sprintf (temp, "%s with code %d", msg, code);
    ERRPRINT(temp);
    exit (code);  
}  

char *bias_codes[] = {
    "",         /* 0 */
    "sq1bias",  /* 1 - sq1servo */
    "sq2bias",  /* 2 - sq2servo */
    "sq1bias",  /* 3 - sq1servo_sa */
    "sq1bias",  /* 4 - rs_servo */
};
char *flux_codes[] = {
    "",        /* 0 */
    "sq1fb",   /* 1 - sq1servo */
    "sq2fb",   /* 2 - sq2servo */
    "sq1fb",   /* 3 - sq1servo_sa */
    "rowsel",  /* 4 - rs_servo */
};

/***********************************************************
 * genrunfile - creates a runfile
 ***********************************************************/
int genrunfile (
        char *full_datafilename, /* datafilename including the path*/
        char *datafile,          /* datafilename */
        int  which_servo,        /* 1 for sq1servo, 2 for sq2servo*/
        int  which_rc,
        int bias, int bstep, int nbias, int bias_active,
        int feed, int fstep, int nfeed,
        char *servo_init1,       /* a line of servo_init var_name and values to be included in <servo_init>*/     
        char *servo_init2        /* a line of servo_init var_name and values to be included in <servo_init>*/     
)
{
    /* Method: spawns mcestatus to create <header> section
     *         generates <par_ramp> section
     *         generates <servo_init> section
     *         spawns frameacq_stamp to create <frameacq> section
     */
    char command[512];
    char myerrmsg[100];
    char runfilename[256];
    int sysret;
    FILE *runfile;

    sprintf(command, "mce_status -f %s.run", full_datafilename);
    if ((sysret =system (command)) != 0){
        sprintf(myerrmsg, "generating runfile %s.run failed when inserting mce_status header",datafile);
        ERRPRINT(myerrmsg);
        return sysret;
    }
    sprintf(runfilename, "%s.run",full_datafilename);
    if ((runfile = fopen (runfilename, "a")) == NULL){
        sprintf(myerrmsg, "generating runfile %s failed when appending loop pars", runfilename);
        return 1;
    }
    /*<servo_init section*/
    if (servo_init1 != NULL){
        fprintf (runfile,"<servo_init>\n  %s\n", servo_init1);
        if (servo_init2 != NULL)
            fprintf (runfile,"  %s\n", servo_init2);
        fprintf (runfile, "</servo_init>\n\n");    
    }
    /*<par_ramp> section*/  
    fprintf (runfile,"<par_ramp>\n  <loop_list> loop1 loop2\n");
    fprintf (runfile,
            "    <par_list loop1> par1\n"
            "      <par_title loop1 par1> %s\n"
            "      <par_step loop1 par1> %d %d %d\n"
            "      <par_active loop1 par1> %d\n",
            bias_codes[which_servo], bias, bstep, nbias, bias_active);
    fprintf (runfile,
            "    <par_list loop2> par1\n"
            "      <par_title loop2 par1> %s\n"
            "      <par_step loop2 par1> %d %d %d\n",
            bias_codes[which_servo], feed, fstep, nfeed);
    fprintf (runfile, "</par_ramp>\n\n");
    fclose(runfile);

    /* frameacq_stamp */
    char rc_code = (which_rc > 0) ? '0'+which_rc : 's';
    sprintf(command, "frameacq_stamp %c %s %d >> %s.run", rc_code, datafile, nbias*nfeed, full_datafilename);
    if ( (sysret =system (command)) != 0){
        sprintf(myerrmsg, "generating runfile %s.run failed when inserting frameacq_stamp",datafile);
        ERRPRINT(myerrmsg);
        return sysret;
    }
    return 0;
}


mce_context_t* connect_mce_or_exit(option_t* options)
{
    char errmsg[1024];

    mce_context_t* mce = mcelib_create(options->fibre_card,
            options->config_file, 0);

    /* get defaults */
    if (options->experiment_file == NULL) {
        options->experiment_file = mcelib_default_experimentfile(mce);
        if (options->experiment_file == NULL) {
            sprintf(errmsg,
                    "Unable to obtain path to default experiment.cfg!\n");
            ERRPRINT(errmsg);
            exit(ERR_MCE_LCFG);
        }
    }

    // Load MCE hardware information ("xml")
    if (mceconfig_open(mce, options->hardware_file, NULL) != 0) {
        sprintf(errmsg, "Failed to open %s as hardware configuration file.",
                options->hardware_file);
        ERRPRINT(errmsg);
        exit(ERR_MCE_LCFG);
    }

    // Connect to an mce_cmd device.
    if (mcecmd_open(mce)) {
        sprintf(errmsg, "Failed to open CMD device.\n");
        ERRPRINT(errmsg);
        exit(ERR_MCE_OPEN);
    }

    // Open data device
    if (mcedata_open(mce) != 0) {
        sprintf(errmsg, "Failed to open DATA device.\n");
        ERRPRINT(errmsg);
        exit(ERR_MCE_DATA);
    }

    return mce;
}


int load_param_or_exit(mce_context_t* mce, mce_param_t* p,
        const char *card, const char *para, int no_exit)
{
    char errmsg[1024];
    int error = mcecmd_load_param(mce, p, card, para);
    if (error != 0) {
        if (no_exit)
            return error;

        sprintf(errmsg, "lookup of %s %s failed with %d (%s)",
                card, para, error, mcelib_error_string(error)); 
        ERRPRINT(errmsg);
        exit(ERR_MCE_PARA);
    }
    return 0;
}


void write_range_or_exit(mce_context_t* mce, mce_param_t* p,
        int start, int32_t* data, int count,
        const char *opmsg)
{
    char temp[1024];
    int error = mcecmd_write_range(mce, p, start, (uint32_t*)data, count);
    if (error != 0) {

        sprintf (temp, "mcecmd_write_range %s with code %d", opmsg, error);
        ERRPRINT(temp);
        exit (error);  
    }
}

/* check_fast_sq2 -- perform the rather involved steps necessary
   to determine whether fast SQ2 switching is active or not.

   Initializes sq2fb and/or sq2fb_col for use and returns 1 or 0
   depending on whether fast_sq1 is active or not.
   */

int check_fast_sq2(mce_context_t* mce, mce_param_t* sq2fb,
        mce_param_t* sq2fb_col, int col0, int n_col)
{
    char errmsg[MAXLINE];
    int fast_sq2 = 0;

    /* Check for both sq2 fb and sq2 fb_col.  When both are present,
       check the current muxing state of the SQ2. */

    int fb0_err = load_param_or_exit(mce, sq2fb, SQ2_CARD, SQ2_FB_COL "0", 1);
    int fb_err = load_param_or_exit(mce, sq2fb, SQ2_CARD, SQ2_FB, 1);

    if (fb_err != 0 && fb0_err != 0) {
        sprintf(errmsg, "Neither %s %s nor %s %s could be loaded!",
                SQ2_CARD, SQ2_FB, SQ2_CARD, SQ2_FB_COL "0"); 
        ERRPRINT(errmsg);
        exit(ERR_MCE_PARA);
    } else if (fb0_err == 0 && fb_err != 0) {
        printf("Identified biasing address card.\n");
        // Biasing address card.
        fast_sq2 = 1;
    } else if (fb0_err == 0 && fb_err == 0) {
        printf("Identified bias card with fast-SQ2 support.\n");
        // New bias card with dual support, so check value of enbl_mux
        mce_param_t mux;
        if (load_param_or_exit(mce, &mux, SQ2_CARD, SQ2_FB_MUX, 1)) {
            ERRPRINT("Could not load " SQ2_CARD " " SQ2_FB_MUX " to check fast-SQ2 setting.");
            exit(ERR_MCE_PARA);
        }
        uint32_t mux_mode[MAXCOLS];
        if (mcecmd_read_range(mce, &mux, col0, mux_mode, 1)) {
            ERRPRINT("Failed to read " SQ2_CARD " " SQ2_FB_MUX " to check fast-SQ2 setting.");
            exit(ERR_MCE_PARA);
        }
        // Just use first value to determine whether SQ2 is muxing.
        if (mux_mode[0] != 0)
            fast_sq2 = 1;
    } else {
        printf("Identified bias card with non-muxing SQ2 FB.\n");
    }

    if (fast_sq2) {
        char tempbuf[MAXLINE];
        for (int i=0; i<n_col; i++) {
            sprintf(tempbuf, "%s%i", SQ2_FB_COL, i+col0);
            load_param_or_exit(mce, sq2fb_col+i, SQ2_CARD, tempbuf, 0);
        }
    }

    return fast_sq2;
}

/* check_mux11d -- check for mux11d support.

   Initializes safb and/or safb_col for use and returns 1 or 0
   depending on whether mux11d appears to be enabled or not.
*/

int check_mux11d(mce_context_t* mce, mce_param_t* safb,
        mce_param_t* safb_col, int col0, int n_col)
{
    /* Just check for "sa fb_col0". */
    int fb0_err = load_param_or_exit(mce, safb, SA_CARD, SA_FB_COL "0", 1);
    if (fb0_err != 0) {
        printf("Card does not appear to support fast-switching SA FB.\n");
        return 0;
    }

    char tempbuf[MAXLINE];
    for (int i=0; i<n_col; i++) {
        sprintf(tempbuf, "%s%i", SA_FB_COL, i+col0);
        load_param_or_exit(mce, safb_col+i, SA_CARD, tempbuf, 0);
    }

    return 1;
}

void duplicate_fill(int32_t value, int32_t *data, int count)
{
    int i;
    for (i=0; i<count; i++) data[i] = value;
}

void rerange(int32_t *dest, int32_t *src, int n_data,
        int *quanta, int n_quanta)
{
    for (int i=0; i<n_data; i++) {
        int q = quanta[i % n_quanta];
        dest[i] = (q==0) ? src[i] : (src[i] % q + q) % q;
    }
}

int load_initfile(const char *datadir, const char *filename, int start, int count, int *dest)
{
    FILE *fd;
    char fullname[MAXLINE];
    char line[MAXLINE];
    if (datadir == NULL)
        strcpy(fullname, filename);
    else
        snprintf(fullname, MAXLINE, "%s/%s", datadir, filename);

    if ( (fd=fopen(fullname, "r")) == NULL) {
        ERRPRINT("failed to open init values file:\n");
        ERRPRINT(fullname);
        exit(ERR_DATA_ROW);
    }

    for (int j=-start; j<count; j++) {
        if (fgets(line, MAXLINE, fd) == NULL) {
            ERRPRINT("not enough numbers in init values file:\n");
            ERRPRINT(fullname);
            exit(ERR_INI_READ);
        }
        if (j<0)
            continue;
        dest[j] = atoi(line);
    }
    return 0;
}


config_setting_t* load_config(const char *filename)
{
    config_t* cfg = malloc(sizeof(config_t));
    config_init(cfg);
    if (config_read_file(cfg, filename) != CONFIG_TRUE) {
        free(cfg);
        return NULL;
    }

    return config_root_setting(cfg);
}

int load_int_array(config_setting_t *cfg, char *name, int start, int count, int* data)
{
    config_setting_t *el = config_setting_get_member(cfg, name);
    if (el == NULL) {
        ERRPRINT("Failed to load experiment.cfg parameter:");
        ERRPRINT(name);
        exit(ERR_MCE_ECFG);
    }
    memset(data, 0, count*sizeof(*data));
    for (int i=0; i<count; i++)
        data[i] = config_setting_get_int_elem(el, start+i);
    return 0;
}

int load_double_array(config_setting_t *cfg, char *name, int start, int count, double *data)
{
    config_setting_t *el = config_setting_get_member(cfg, name);
    if (el == NULL) {
        ERRPRINT("Failed to load experiment.cfg parameter:");
        ERRPRINT(name);
        exit(ERR_MCE_ECFG);
    }
    memset(data, 0, count*sizeof(*data));
    for (int i=0; i<count; i++)
        data[i] = config_setting_get_float_elem(el, start+i);
    return 0;
}


/* These currently don't have type checking and will just return 0s if
   the values are formatted incorrectly.  Newer libconfig (1.4)
   provides a better interface. */

int load_double(config_setting_t *cfg, char *name, double *dest)
{
    config_setting_t *n = config_setting_get_member(cfg, name);
    if (n==NULL) {
        ERRPRINT("Failed to load experiment.cfg parameter:");
        ERRPRINT(name);
        exit(ERR_MCE_ECFG);
    }
    *dest = config_setting_get_float(n);
    return 0;
}

int load_int(config_setting_t *cfg, char *name, int *dest)
{
    config_setting_t *n = config_setting_get_member(cfg, name);
    if (n==NULL) {
        ERRPRINT("Failed to load experiment.cfg parameter:");
        ERRPRINT(name);
        exit(ERR_MCE_ECFG);
    }
    *dest = config_setting_get_int(n);
    return 0;
}
