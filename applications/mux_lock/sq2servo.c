#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "servo_err.h"
#include "servo.h"

/***********************************************************
 *    sq2servo       : locks sq2 by calculating new sa_fb value and sweeping sq2_fb
 *    Author         : Mandana@ubc/Dennis@atc     4May2005
 *
 *  Revision history:
 *  <date $Date: 2007/10/22 23:38:35 $>    - <initials $Author: mce $>
 *  $Log: sq2servo.c,v $
 *  Revision 1.21  2007/10/22 23:38:35  mce
 *    * EB: MH fixed a bug
 *    * MA added <> to .bias header line
 *    * MA renamed .fb to .bias and creates a merger-friendly format of .bias file
 *
 ***********************************************************/

/***********************************************************
 * frame_callback: to store the frame to a file and fill row_data
 *
 *********************************************************/ 
int frame_callback(unsigned user_data, int frame_size, u32 *data){
  
  //Re-type 
  servo_t *myservo = (servo_t*)user_data;
  
  fwrite(data, frame_size, sizeof(u32), myservo->df);
  
  myservo->fcount ++;
  
  int i;
  u32 *lastrow_base;
  lastrow_base = data + HEADER_OFFSET + (myservo->row_num[0])*MAXCHANNELS;
  for (i=0; i<MAXCHANNELS; i++){
    myservo->row_data[i] = (int) *(lastrow_base + i);	
    //printf ("%d ", myservo->row_data[i]); 
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
   char safb_initfile[256];      /* filename for safb.init*/
   
   int i;
   int j;
   int ncol;
   int snum;                /* loop counter */
   
   FILE *fd;                /* pointer to output file*/
   FILE *tempf;             /* pointer to safb.init file*/
   char line[MAXLINE]; 
   char init_line[MAXLINE];    /* record a line of init values and pass it to genrunfile*/
   char tempbuf[30];
  
   double gain;             /* servo gain (=P=I) for the 0th calculation of the servo*/
   char *endptr;
   int nbias;
   int nfeed;
   u32 nrows_rep;
   char outfile[256];       /* output data file */
   int ssafb[MAXVOLTS];     /* series array feedback voltages */
   int sq2bias;             /* SQ2 bias voltage */
   int sq2bstep;            /* SQ2 bias voltage step */
   int sq2feed;             /* SQ2 feedback voltage */
   int sq2fstep;            /* SQ2 feedback voltage step */
   double z;                /* servo feedback offset */ 
   int  which_rc;
   int  skip_sq2bias = 0;

   int  error = 0;
   char errmsg_temp[256];
   
   time_t start, finish;
   
   time(&start);
   
/* check command-line arguments */
   if ( argc != 11 && argc != 12)
   {  
      printf ( "Rev. 2.1clover\n");
      printf ( "usage: sq2servo outfile sq2bias sq2bstep nbias\n" );
      printf ( "sq2feed sq2fstep nfeed N target gain skip_sq2bias\n" );
      printf ( "   outfile = name of file for output data\n" );
      printf ( "   sq2bias = starting SQ2 bias\n" );
      printf ( "   sq2bstep = step for SQ2 bias\n" );
      printf ( "   nbias = number of bias steps\n" );
      printf ( "   sq2feed = starting SQ2 feedback\n" );
      printf ( "   sq2fstep = step for SQ2 feedback\n" );
      printf ( "   nfeed = number of feedback steps\n" );
      printf ( "   N = readout-card number (1 to 4)\n" );
      printf ( "   target = lock target \n");
      printf ( "   gain = servo gain (double) \n");
      printf ( "   skip_sq2bias (optional) = if specified as 1, then no sq2_bias is applied.\n");
      ERRPRINT("wrong number of arguments");
      return ERR_NUM_ARGS;
   }
   /* Get range of values for second stage SQUIDs */
   strcpy(datafile, argv[1]);
   sq2bias = atoi ( argv[2] );
   sq2bstep = atoi ( argv[3] );
   nbias = atoi ( argv[4] );
   sq2feed = atoi ( argv[5] );
   sq2fstep = atoi ( argv[6] );
   nfeed = atoi ( argv[7] );
   which_rc = atoi ( argv[8]);
   z = atoi (argv[9]);
   gain = strtod(argv[10], &endptr);
   
   if (argc == 12){
      skip_sq2bias = atoi(argv[11]);
      if (nbias <1) nbias = 1;
   }
   else
      skip_sq2bias = 0;
   
   
   // Create MCE context
   mce_context_t *mce = mcelib_create();

   // Load MCE config information ("xml")
   if (mceconfig_open(mce, CONFIG_FILE, NULL) != 0) {
     sprintf(errmsg_temp, "Load MCE configuration file %s", CONFIG_FILE);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_LCFG;
   }

   // Connect to an mce_cmd device.
   if (mcecmd_open(mce, CMD_DEVICE)) {
     sprintf(errmsg_temp, "Failed to open %s.\n", CMD_DEVICE);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_OPEN;
   }

   // Open data device
   if ((error=mcedata_open(mce, DATA_DEVICE))!= 0) {
     sprintf(errmsg_temp, "could not open '%s'", DATA_DEVICE);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_DATA;
   }


   // Lookup "bc1 flux_fb"
   mce_param_t m_safb;
   if ((error=mcecmd_load_param(mce, &m_safb, SAFB_CARD, "flux_fb")) != 0) {
     sprintf(errmsg_temp, "lookup of %s flux_fb failed with %d", SAFB_CARD, error); 
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }     
   // Lookup "bc1 flux_fb"
   mce_param_t m_sq2fb;
   if ((error=mcecmd_load_param(mce, &m_sq2fb, SQ2FB_CARD, "flux_fb")) != 0) {
     sprintf(errmsg_temp, "lookup of %s flux_fb failed with %d", SQ2FB_CARD, error); 
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }     
   // Lookup "bc3 flux_fb"
   mce_param_t m_sq2bias;
   if ((error=mcecmd_load_param(mce, &m_sq2bias, SQ2BIAS_CARD, SQ2BIAS_CMD)) != 0) {
     sprintf(errmsg_temp, "lookup of %s %s failed with %d", SQ2BIAS_CARD, SQ2BIAS_CMD, error); 
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }     
   // now setup a single-frame acquisition 
   // Read back num_rows_reported
   mce_param_t m_nrows_rep;
   if ((error=mcecmd_load_param(mce, &m_nrows_rep, "cc", "num_rows_reported")) != 0) {
     sprintf(errmsg_temp, "lookup of cc num_rows_reported failed with %d code", error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_PARA;
   }     
   //int nrows_rep;
   if ((error=mcecmd_read_element(mce, &m_nrows_rep, 0, &nrows_rep)) != 0){
     sprintf(errmsg_temp, "rb cc num_rows_reported failed with %d", error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_RB;     
   }	   
   // Pick a card (won't work for rcs!!)
   int cards=(1<<(which_rc-1));
   printf("Card bits=%#x, num_rows_reported=%d\n", cards, (int)nrows_rep);
   mce_acq_t acq;
   mcedata_acq_setup(&acq, mce, 0, cards, (int) nrows_rep);
   // Our callback will update the counter in this structure
   servo_t sq2servo; 
   sq2servo.fcount = 0;
   sq2servo.which_rc = which_rc;
   sq2servo.row_num[0] = nrows_rep - 1;

   // setup a call back function
   mcedata_rambuff_create(&acq, frame_callback, (unsigned) &sq2servo);   
   
   if ( (datadir=getenv("MAS_DATA")) == NULL){
      ERRPRINT("Enviro var. $MAS_DATA not set, quit");
      return ERR_DATA_DIR;
   }
   sprintf (full_datafilename, "%s%s",datadir, datafile);

   // open a datafile 
   if( (sq2servo.df = fopen(full_datafilename, "w")) == NULL){
     sprintf(errmsg_temp, "openning data file: %s", full_datafilename);
     ERRPRINT(errmsg_temp);
     return ERR_DATA_FIL;
   }
   
/* Open output file to append modified data set */
   sprintf(outfile, "%s%s.bias", datadir, datafile);
   fd = fopen ( outfile, "a" );

/* Get starting SA feedback values  from a file called safb.init*/
   strcpy (safb_initfile, datadir);
   strcat (safb_initfile, "safb.init");
   if ((tempf = fopen (safb_initfile, "r")) == NULL){
      ERRPRINT("failed to open safb.init to read initial settings for safb");
      return ERR_SAFB_INI;
   }
   
   /* prepare a line of init values for runfile*/   
   sprintf(init_line, "<safb.init> ");
   for ( j=0; j<MAXVOLTS; j++ ){
     if ( fgets (line, MAXLINE, tempf) == NULL){
       ERRPRINT("reading safb.init quitting...."); 
       return ERR_INI_READ;
     }
     ssafb[j] = atoi (line );
     sprintf(tempbuf, "%d ", ssafb[j]);
     strcat(init_line, tempbuf);
   }
   fclose(tempf);

  
   /** generate a runfile **/
   error=genrunfile (full_datafilename, datafile, 2, which_rc, 
                      sq2bias, sq2bstep, nbias, sq2feed, sq2fstep, nfeed, 
                      init_line, NULL);
   if (error != 0){
     sprintf(errmsg_temp, "genrunfile %s.run failed with %d", full_datafilename, error);
     ERRPRINT(errmsg_temp);
     return ERR_RUN_FILE; 
   }

   /* generate the header line for the bias file*/
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
     fprintf ( fd, "  <error%02d> ", snum);  
         
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
     fprintf ( fd, "  <ssafb%02d> ", snum);  
   fprintf ( fd, "\n");
  
   /* write the initial ssafb values, do not apply any bias, 
      Execute a go command to take one frame of data to start the algorithm */
   if ((error = mcecmd_write_block(mce, &m_safb, MAXVOLTS, (u32 *)ssafb)) != 0) /*array*/
     error_action("mcecmd_write_block safb", error);

   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ ){
     if ((error = mcecmd_write_element(mce, &m_sq2fb, snum, sq2feed)) != 0)
       error_action("mcecmd_write_element sq2fb", error);
   }
   if ( (error=mcedata_acq_go(&acq, 1)) != 0) 
     error_action("data acquisition failed", error);

   // finally, the servo loop:
   for ( j=0; j<nbias; j++ ){
      sq2bias += sq2bstep;
      if (!skip_sq2bias){
        for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
          if ((error = mcecmd_write_element(mce, &m_sq2bias, snum, sq2bias)) != 0)
            error_action("mcecmd_write_element sq2bias", error);
      }

      for ( i=0; i<nfeed; i++ ){
         sq2feed += sq2fstep;

         /* now extract each column's reading*/
         for (ncol=0; ncol<MAXCHANNELS; ncol++)
	   fprintf ( fd, "%11d ", sq2servo.row_data[ncol] );

         for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ ){
           ssafb[snum] += gain * ((sq2servo.row_data[snum%8])-z );
           fprintf ( fd, "%11d ", ssafb[snum] );
         }
         fprintf ( fd, "\n" );
         
	 /* change voltages and trigguer more data acquisition */
         if ((error = mcecmd_write_block(mce, &m_safb, MAXVOLTS, (u32 *)ssafb)) != 0) /*array*/
           error_action("mcecmd_write_block safb", error);
         for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ ){
           //if ((error = mcecmd_write_element(mce, &m_safb, snum, ssafb[snum])) != 0) 
           //  error_action("mcecmd_write_element safb", error); 
		 
           if ((error = mcecmd_write_element(mce, &m_sq2fb, snum, sq2feed)) != 0)
             error_action("mcecmd_write_element sq2fb", error);
	 }
	 /* if this is the last iteration, skip_go */
         if ( (j != nbias-1) || (i != nfeed-1)) 
           if ( (error=mcedata_acq_go(&acq, 1)) != 0) 
             error_action("data acquisition failed", error);
	 //printf("acquired %d\n", sq2servo.fcount); 
      }
   }

   /* reset biases back to 0*/
   for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ ){
     ssafb[snum] = 0;
     if ((error = mcecmd_write_element(mce, &m_sq2fb, snum, 0)) != 0)
       error_action("mcecmd_write_element sq2fb", error);
   }
   if ((error = mcecmd_write_block(mce, &m_safb, MAXVOLTS, (u32 *)ssafb)) != 0) /*array*/
     error_action("mcecmd_write_block safb failed", error);

   if (!skip_sq2bias){
     for ( snum=(which_rc-1)*8; snum<which_rc*8; snum++ )
       if ((error = mcecmd_write_element(mce, &m_sq2bias, snum, sq2bias)) != 0)
         error_action("mcecmd_write_element sq2bias", error);
   }  
   else
     printf("This script did not apply SQ2 bias, you may need to turn biases off manually!\n");
   
   fclose(fd);
   fclose(sq2servo.df);
   mcelib_destroy(mce);

   time(&finish);
   //elapsed = ((double) (end - start))/CLOCKS_PER_SEC;
   printf ("sq2servo: elapsed time is %fs \n", difftime(finish,start));	   
   return SUCCESS;
}
