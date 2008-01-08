#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "servo_err.h"
#include "servo.h"

/***********************************************************
 *    sq1servo       : locks sq1 by calculating new sq2_fb value and sweeping sq1_bias
 *    Author         : Mandana@ubc/Dennis@atc     5May2005
 *    Original Source: ssaservo
 * Revision history:
 * <date $Date: 2007/10/22 23:38:35 $>    - <initials $Author: mce $>
 * $Log: sq1servo.c,v $
 * Revision 1.27  2007/10/22 23:38:35  mce
 *   * EB: MH fixed a bug
 *   * MA added <> to .bias header line
 *   * MA renamed .fb to .bias and creates a merger-friendly format of .bias file
 *
 ***********************************************************/
/***********************************************************
 * frame_callback: to store the frame to a file and fill row_data
 * 
 **********************************************************/
int frame_callback(unsigned user_data, int frame_size, u32 *data){

  //Re-type 
  servo_t *myservo = (servo_t*)user_data;
  
  fwrite(data, frame_size, sizeof(u32), myservo->df);
   
  myservo->fcount ++;

  int i;
  u32 *lastrow_base;
  //printf("\nframe %d: ",myservo->fcount);
  for (i=(myservo->which_rc - 1)*8; i<(myservo->which_rc)*8; i++){
    lastrow_base = data + HEADER_OFFSET + (myservo->row_num[i])*MAXCHANNELS;
    myservo->row_data[i] = (int) *(lastrow_base + i%8);
    //printf ("c%dr%d %d\t", i,myservo->row_num[i], myservo->row_data[i]); 
  }
  return 0;
}
/************************************************************
 *          M A I N
 ************************************************************/
int main ( int argc, char **argv )
{
   char datafile[256];     /* datafile being written by DAS */
   char full_datafilename[256]; /*full path for datafile*/
   char *datadir;
   char sq2fb_initfile[256];      /* filename for sq2fb.init*/
   char row_initfile[256];
   
   u32 temparr[MAXROWS];
  
   int i;
   int j;
   int snum;                /* loop counter */
 
   FILE *fd;                /* pointer to output file*/
   FILE *tempf;             /* pointer to sq2fb.init file*/
   char outfile[256];       /* output data file */
   char line[MAXLINE];
   char init_line1[MAXLINE];    /* record a line of init values and pass it to genrunfile*/
   char init_line2[MAXLINE];    /* record a line of init values and pass it to genrunfile*/
   char tempbuf[30];

   double gain;             /* servo gain (=P=I) */
   char *endptr;
   int nbias;
   int nfeed;
   int sq2fb[MAXVOLTS];     /* sq2 feedback voltages */
   int sq1bias;             /* SQ2 bias voltage */
   int sq1bstep;            /* SQ2 bias voltage step */
   int sq1feed;             /* SQ2 feedback voltage */
   int sq1fstep;            /* SQ2 feedback voltage step */
   double z;                /* servo feedback offset */
   int  which_rc;
   char which_rc_name[5];
   int total_row;
   int skip_sq1bias = 0;
   
   int error=0;
   char errmsg_temp[256];
   
   time_t start, finish;
   
   time(&start);

   /* check command-line arguments */
   if ( argc != 12 && argc != 13){  
      printf ( "Rev. 2.0\n");
      printf ( "usage:- sq1servo outfile sq1bias sq1bstep nbias " );
      printf ( "sq1fb sq1fstep nfb N target total_row gain skip_sq1bias\n" );
      printf ( "   outfile = filename for output data\n" );
      printf ( "   sq1bias = starting SQ1 bias\n" );
      printf ( "   sq1bstep = step size for SQ1 bias\n" );
      printf ( "   nbias = number of bias steps\n" );
      printf ( "   sq1fb = starting SQ1 feedback\n" );
      printf ( "   sq1fstep = step size for SQ1 feedback\n" );
      printf ( "   nfb = number of feedback steps\n" );
      printf ( "   N = readout-card number (1 to 4)\n");
      printf ( "   target = lock target \n");
      printf ( "   total_row = total number of rows in the system \n");
      printf ( "   gain = gain of the servo (double)\n");
      printf ( "   skip_sq1bias (optional) = if specified as 1, no SQ1 bias is applied\n");
      printf ( "*NOTE*: Make sure sq2fb.init (32 single-entry lines) and row.init (32 single-entry lines between 0 to 40) are present in the data directory\n"); 
      ERRPRINT("no argument specified");
      return ERR_NUM_ARGS;
   }

   /* Get range of values for second stage SQUIDs */
   strcpy(datafile, argv[1]);
   sq1bias = atoi ( argv[2] );
   sq1bstep = atoi ( argv[3] );
   nbias = atoi ( argv[4] );
   sq1feed = atoi ( argv[5] );
   sq1fstep = atoi ( argv[6] );
   nfeed = atoi ( argv[7] );
   which_rc = atoi (argv[8]);
   z = atoi (argv[9]);
   total_row = atoi(argv[10]);
   gain = strtod (argv[11], &endptr);
   if (argc == 13){
     skip_sq1bias = atoi(argv[12]);
     if (nbias <1 ) nbias = 1;
     //printf("No SQ1 bias is applied!\n");
   }
   else 
     skip_sq1bias = 0;
   
   // Load MCE config information ("xml")
   mceconfig_t *conf;
   if (mceconfig_load(CONFIG_FILE, &conf) != 0) {
     sprintf(errmsg_temp, "Load MCE configuration file %s", CONFIG_FILE);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_LCFG;
   }

   // Connect to an mce_cmd device.
   int handle = mce_open(CMD_DEVICE);
   if (handle < 0) {
     sprintf(errmsg_temp, "Failed to open %s.\n", CMD_DEVICE);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_OPEN;
   }

   // Share the config information with the mce_cmd device
   mce_set_config(handle, conf);

   // Open data device
   mcedata_t mcedata;
   if ((error=mcedata_init(&mcedata, handle, DATA_DEVICE))!= 0) {
     sprintf(errmsg_temp, "could not open '%s'", DATA_DEVICE);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_DATA;
   }

   // Lookup "bc1 flux_fb"
   mce_param_t m_sq2fb;
   if ((error=mce_load_param(handle, &m_sq2fb, SQ2FB_CARD, "flux_fb")) != 0) {
     sprintf(errmsg_temp, "lookup of %s flux_fb failed with %d", SQ2FB_CARD, error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }
   // Lookup "rc1 fb_const"
   mce_param_t m_sq1fb;
   sprintf(which_rc_name,"rc%d",which_rc);
   if ((error=mce_load_param(handle, &m_sq1fb, which_rc_name, "fb_const")) != 0) {
     sprintf(errmsg_temp, "lookup of %s fb_const failed with %d", which_rc_name, error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }
   // Lookup "ac on_bias"
   mce_param_t m_sq1bias;
   if ((error=mce_load_param(handle, &m_sq1bias, SQ1BIAS_CARD, SQ1BIAS_CMD)) != 0) {
     sprintf(errmsg_temp, "lookup of %s %s failed with %d", SQ1BIAS_CARD, SQ1BIAS_CMD, error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   } 
   // now setup a single-frame acquisition 
   // Read back num_rows_reported
   mce_param_t m_nrows_rep;
   if ((error=mce_load_param(handle, &m_nrows_rep, "cc", "num_rows_reported")) != 0) {
     sprintf(errmsg_temp, "lookup of cc num_rows_reported failed with %d code", error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }
   u32 nrows_rep;
   if ((error=mce_read_element(handle, &m_nrows_rep, 0, &nrows_rep)) != 0){
     sprintf(errmsg_temp, "rb cc num_rows_reported failed with %d", error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_RB;
   }
   // Pick a card (won't work for rcs!!)
   int cards=(1<<(which_rc-1));
   printf("Card bits=%#x, num_rows_reported=%d\n", cards, (int)nrows_rep);
   mce_acq_t acq;
   mcedata_acq_setup(&acq, &mcedata, 0, cards, (int) nrows_rep);
   // Our callback will update the counter in this structure
   servo_t sq1servo;
   sq1servo.fcount = 0;
   sq1servo.which_rc=which_rc;

   // setup a call back function
   mcedata_rambuff_create(&acq, frame_callback, (unsigned) &sq1servo);
   
   if ( (datadir=getenv("MAS_DATA")) == NULL){
     ERRPRINT("Enviro var. $MAS_DATA not set, quit");
     return ERR_DATA_DIR;
   }
   sprintf(full_datafilename, "%s%s",datadir, datafile);
   
   // open a datafile 
   if( (sq1servo.df = fopen(full_datafilename, "w")) == NULL){
     sprintf(errmsg_temp, "openning data file: %s", full_datafilename);
     ERRPRINT(errmsg_temp);
     return ERR_DATA_FIL;
   }
                          
   /* Open output file to append modified data set */
   sprintf(outfile, "%s%s.bias", datadir, datafile);
   fd = fopen ( outfile, "a" );

   /* Get starting SQ2 feedback values  from a file called sq2fb.init*/
   strcpy (sq2fb_initfile, datadir);
   strcat (sq2fb_initfile, "sq2fb.init");
   if ((tempf = fopen (sq2fb_initfile, "r")) == NULL){
      sprintf (errmsg_temp,"failed to open sq2fb.init (%s) to read initial settings for sq2fb", sq2fb_initfile);
      ERRPRINT(errmsg_temp);
      return ERR_S2FB_INI;
   }
   /*prepare a line of init values for runfile*/
   sprintf(init_line1, "<sq2fb.init> ");
   for ( j=0; j<MAXVOLTS; j++ ){
     if (fgets (line, MAXLINE, tempf) == NULL){
       ERRPRINT ("reading sq2fb.init quitting....");
       return ERR_INI_READ;
     }
     sq2fb[j] = atoi (line );
     sprintf(tempbuf, "%d ", sq2fb[j]);
     strcat(init_line1, tempbuf);
   }
   fclose(tempf);

   /* Get row number for each column to servo on*/
   strcpy (row_initfile, datadir);
   strcat (row_initfile, "row.init");
   if ((tempf = fopen (row_initfile, "r")) == NULL){
      sprintf (errmsg_temp, "failed to open row.init (%s) to read row numbers to servo on", row_initfile);
      ERRPRINT(errmsg_temp);
      return ERR_DATA_ROW;
   }
   /*prepare a line of init values for runfile*/
   sprintf(init_line2, "<row.init> ");
   for ( j=0; j<MAXVOLTS; j++ ) {
     if (fgets (line, MAXLINE, tempf) == NULL){
       ERRPRINT("reading row.init quitting....");
       return ERR_INI_READ;
     }
     sq1servo.row_num[j] = atoi (line );
     sprintf(tempbuf, "%d ", sq1servo.row_num[j]);
     strcat(init_line2, tempbuf);
     if (sq1servo.row_num[j] < 0 || sq1servo.row_num[j] > nrows_rep){ //formerly compared with 40!
       sprintf (errmsg_temp, "out-of-range entry in row.init: %d\n Valid range is: 0<row_num<%d (num_rows_reported)", 
		                                            sq1servo.row_num[j], nrows_rep); 
       ERRPRINT(errmsg_temp);
       return ERR_INI_READ;
     } 
   }
   fclose(tempf);

   /** generate a runfile **/
   error=genrunfile (full_datafilename, datafile, 1, which_rc, 
                      sq1bias, sq1bstep, nbias, sq1feed, sq1fstep, nfeed, 
                      init_line1, init_line2);
   if (error != 0){
     sprintf(errmsg_temp, "genrunfile %s.run failed with %d", datafile, error);
     ERRPRINT(errmsg_temp);
     return ERR_RUN_FILE;
   }

   /* generate the header line for the bias file*/
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
      fprintf ( fd, "  <error%02d> ", snum);
         
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
      fprintf ( fd, "  <sq2fb%02d> ", snum); 
   fprintf ( fd, "\n");

   /* apply the sq2fb starting values (as specified in sq2fb.init) and sq1bias starting value
      and trigger one-frame data acqusition*/
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
     if ((error = mce_write_element(handle, &m_sq2fb, snum, sq2fb[snum])) != 0)
       error_action("mce_write_element sq2fb", error);

   for ( snum=0; snum<MAXCHANNELS; snum++ )
     temparr[snum] = (u32) sq1feed;	   
   if ((error = mce_write_block(handle, &m_sq1fb, MAXCHANNELS, temparr)) != 0)
     error_action("mce_write_block sq1fb", error);	
		   
   if (!skip_sq1bias){
     for (snum=0; snum<MAXROWS; snum++)
       temparr[snum] = (u32) sq1bias;	     
     if ((error = mce_write_block(handle, &m_sq1bias, MAXROWS, temparr)) != 0) 
       error_action("mce_write_block sq1bias", error);
   }	   
   
   if ((error=mcedata_acq_go(&acq, 1)) != 0)
     error_action("data acquisition failed", error);
   
   /* start the servo*/
   for ( j=0; j<nbias; j++ ){
      sq1bias += sq1bstep;
      if (!skip_sq1bias){
        for (snum=0; snum<MAXROWS; snum++)
          temparr[snum] = (u32) sq1bias;	     
        if ((error = mce_write_block(handle, &m_sq1bias, MAXROWS, temparr)) != 0) 
          error_action("mce_write_block sq1bias", error);
      }	   
      for ( i=0; i<nfeed; i++ ){
         sq1feed += sq1fstep;

         //for (col=0; col<MAXCHANNELS; col++) //incompatible with rcs!
         for (snum=(which_rc-1)*8; snum<which_rc*8; snum++)	 
           fprintf(fd, "%11d ", sq1servo.row_data[snum]);
         
         /* get the right columns, change voltages and trigger more data acquisition*/
         for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ ){
           sq2fb[snum] += gain * ( sq1servo.row_data [snum] - z );
           fprintf ( fd, "%11d ", sq2fb[snum]);
           if ((error = mce_write_element(handle, &m_sq2fb, snum, (u32) sq2fb[snum])) != 0) 
             error_action("mce_write_element sq2fb", error);
         }
         fprintf ( fd, "\n" );
         fflush (fd);

         for ( snum=0; snum<MAXCHANNELS; snum++ )
           temparr[snum] = (u32) sq1feed;	   
         if ((error = mce_write_block(handle, &m_sq1fb, MAXCHANNELS, temparr)) != 0)
           error_action("mce_write_block sq1fb", error);	
	 
         /* if this is the last iteration, skip_go */
         if ((j!=nbias-1) || (i!=nfeed-1))
           if ((error=mcedata_acq_go(&acq, 1)) != 0)
             error_action("data acquisition failed", error);
      }
   }
   /* reset values back to 0*/
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
     if ((error = mce_write_element(handle, &m_sq2fb, snum, 0)) != 0) 
       error_action("mce_write_element sq2fb", error);
   
   for ( snum=0; snum<MAXCHANNELS; snum++ )
     temparr[snum] = (u32) (-8192);	   
   if ((error = mce_write_block(handle, &m_sq1fb, MAXCHANNELS, temparr)) != 0)
     error_action("mce_write_block sq1fb", error);	
   
   if (!skip_sq1bias){
     for (snum=0; snum<MAXROWS; snum++)
       temparr[snum] = 0;	     
     if ((error = mce_write_block(handle, &m_sq1bias, MAXROWS, temparr)) != 0) 
       error_action("mce_write_block sq1bias", error);
   }  
   else
      printf("No SQ1 bias is applied!\n"); 

   fclose(sq1servo.df);
   fclose(fd);
   mceconfig_destroy(conf);
   mce_close(handle);
   
   time(&finish);
   printf("sq1servo: elapsed time is %fs \n", difftime(finish, start));
      
   return SUCCESS;
}
