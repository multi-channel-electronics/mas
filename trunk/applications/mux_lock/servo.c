#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
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
/***********************************************************
 * genrunfile - creates a runfile
 ***********************************************************/
int genrunfile (
char *full_datafilename, /* datafilename including the path*/
char *datafile,          /* datafilename */
int  which_servo,        /* 1 for sq1servo, 2 for sq2servo*/
int  which_rc,
int  bias, int bstep, int nbias, int feed, int fstep, int nfeed,
char *servo_init1,       /* a line of servo_init var_name and values to be included in <servo_init>*/     
char *servo_init2        /* a line of servo_init var_name and values to be included in <servo_init>*/     
){
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
  fprintf (runfile,"    <par_list loop1> par1\n      <par_title loop1 par1> sq%dbias\n      <par_step loop1 par1> %d %d %d\n",
                        which_servo, bias, bstep, nbias);
  fprintf (runfile,"    <par_list loop2> par1\n      <par_title loop2 par1> sq%dfb\n      <par_step loop2 par1> %d %d %d\n",
                        which_servo, feed, fstep, nfeed);
  fprintf (runfile, "</par_ramp>\n\n");
  fclose(runfile);
  
  /* frameacq_stamp */
  sprintf(command, "frameacq_stamp %d %s %d >> %s.run", which_rc, datafile, nbias*nfeed, full_datafilename);
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

  mce_context_t* mce = mcelib_create();
  
  // Load MCE hardware information ("xml")
  if (mceconfig_open(mce, options->hardware_file, NULL) != 0) {
    sprintf(errmsg, "Failed to open %s as hardware configuration file.", options->config_file);
    ERRPRINT(errmsg);
    exit(ERR_MCE_LCFG);
  }
  
  // Connect to an mce_cmd device.
  if (mcecmd_open(mce, options->cmd_device)) {
    sprintf(errmsg, "Failed to open %s as command device.", options->cmd_device);
    ERRPRINT(errmsg);
    exit(ERR_MCE_OPEN);
  }

   // Open data device
   if (mcedata_open(mce, options->data_device) != 0) {
     sprintf(errmsg, "Failed to open %s as data device.", options->data_device);
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
			 int start, u32 *data, int count,
			 const char *opmsg)
{
  char temp[1024];
  int error = mcecmd_write_range(mce, p, start, data, count);
  if (error != 0) {

    sprintf (temp, "mcecmd_write_range %s with code %d", opmsg, error);
    ERRPRINT(temp);
    exit (error);  
  }
}

void duplicate_fill(u32 value, u32 *data, int count)
{
  int i;
  for (i=0; i<count; i++) data[i] = value;
}

void rerange(u32 *dest, u32 *src, int n_data,
	     u32 *quanta, int n_quanta)
{
  int i;
  u32 q;
  for (i=0; i<n_data; i++) {
    q = quanta[i % n_quanta];
    dest[i] = (q==0) ? src[i] : (src[i] % q);
  }
}


config_setting_t* load_config(char *filename)
{
  config_t* cfg = malloc(sizeof(config_t));
  config_init(cfg);
  if (config_read_file(cfg, filename) != CONFIG_TRUE) {
    free(cfg);
    return NULL;
  }

  return config_root_setting(cfg);
}

int* load_int_array(config_setting_t *cfg, char *name, int n) {
  int* data;
  int i;
  config_setting_t *el = config_setting_get_member(cfg, name);
  if (el == NULL) return NULL;
  
  data = malloc(n * sizeof(int));
  memset(data, 0, n*sizeof(int));
  for (i=0; i<n; i++) {
    data[i] = config_setting_get_int_elem(el, i);
  }
  return data;
}
