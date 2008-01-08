#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"servo_err.h"
#include"servo.h"

/******************************************************************
 * servo.c contains subroutines called by sq1servo.c and sq2servo.c
 * 
 * Revision History:
 * <date $Date> 
 * $Log: servo.c,v $
 * Revision 1.1  2007/10/03 23:03:15  mce
 * MA contains subroutines used by sq1servo and sq2servo, for now, only genrunfile
 *
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

