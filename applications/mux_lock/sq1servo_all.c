#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "servo_err.h"
#include "servo.h"

/***********************************************************
 *    sq1servo_all   : locks sq1 by calculating new sq2_fb for each row while sweeping sq1_bias
 *    Author         : Mandana@ubc/Dennis@atc     5May2005
 *    Original Source: ssaservo
 * Revision history:
 * <date $Date: 2007/10/22 23:38:35 $>    - <initials $Author: mce $>
 * $Log: sq1servo.c,v $
 * Revision 1.27  2007/10/22 23:38:35  mce
 *   * EB: MH fixed a bug
 *   * MA added <> to .bias header line
 *   * MA renamed .fb to .bias and creates a merger-friendly format of .bias file
 *   * MA+MFH: repair multi-bias bug
 *
 ***********************************************************/
/***********************************************************
 * frame_callback: to store the frame to a file and fill row_data
 * 
 **********************************************************/
int frame_callback(unsigned long user_data, int frame_size, u32 *data)
{
  //Re-type 
  servo_t *myservo = (servo_t*)user_data;
  int col_offset = (myservo->which_rc - 1)*8;
  u32 *lastrow_base;
  int i;
  
  fwrite(data, sizeof(u32), frame_size, myservo->df);

  memcpy(myservo->last_header, data, HEADER_OFFSET*sizeof(*data));

  // This assumes single card read, but maps the data into full frame structure.
  for (i=0; i<myservo->num_rows; i++) {
    memcpy(myservo->last_frame + MAXVOLTS*i + col_offset,
	   data + HEADER_OFFSET + MAXCHANNELS*i, MAXCHANNELS*sizeof(*data));
  }
  myservo->fcount ++;

  //printf("\nframe %d: ",myservo->fcount);
  for (i=0; i<MAXCHANNELS; i++) {
    lastrow_base = data + HEADER_OFFSET +
	(myservo->row_num[i + col_offset])*MAXCHANNELS;
    myservo->row_data[i+col_offset] = (int) *(lastrow_base + i);
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
   
   u32 temparr[MAXROWS];
  
   int i, j, r, snum;       /* loop counters */
 
   FILE *bias_fd[MAXROWS];  /* Output files for error and bias entries; one per row */
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
   u32 sq2fb[MAXVOLTS][MAXROWS];     /* sq2 feedback voltages */
   int sq1bias;             /* SQ2 bias voltage */
   int sq1bstep;            /* SQ2 bias voltage step */
   int sq1feed;             /* SQ2 feedback voltage */
   int sq1fstep;            /* SQ2 feedback voltage step */
   double z;                /* servo feedback offset */
   int which_rc;
   int soffset;
   int total_row;
   int skip_sq1bias = 0;
   
   int error=0;
   char errmsg_temp[256];
   
   time_t start, finish;

   /* Define default MAS options */
   option_t options = {
     config_file:   DEFAULT_MASFILE,
     cmd_device:    DEFAULT_CMDFILE,
     data_device:   DEFAULT_DATAFILE,
     hardware_file: DEFAULT_HARDWAREFILE,
   };
   int arg_offset = 0;
   
   time(&start);

   /* Process MAS config file options first (-x style) */
   arg_offset = process_options(&options, argc, argv) - 1;
   if (arg_offset < 0)
     exit(ERR_NUM_ARGS);
   
   // Correct argc and argv for the remaining options (hack!)
   argc -= arg_offset;
   argv += arg_offset;

   /* check command-line arguments */
   if ( argc != 12 && argc != 13){  
      printf ( "Rev. 2.0\n");
      printf ( "usage:- sq1servo_all outfile sq1bias sq1bstep nbias " );
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
      printf ( "*NOTE*: Make sure sq2fb.init (32 single-entry lines) is present in the data directory\n"); 
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
   
   // All of our per-column parameters must be written at the right index
   soffset = (which_rc-1)*MAXCHANNELS;

   // Create MCE context
   mce_context_t *mce = connect_mce_or_exit(&options);

   // Lookup MCE parameters, or exit with error message.
   mce_param_t m_sq2fb, m_sq1fb, m_sq1bias, m_nrows_rep, m_retdat,
     m_sq2fb_col[MAXVOLTS];

   load_param_or_exit(mce, &m_sq1fb,   SQ1_CARD, SQ1_FB, 0);
   load_param_or_exit(mce, &m_sq1bias, SQ1_CARD, SQ1_BIAS, 0);
   load_param_or_exit(mce, &m_nrows_rep, "cc", "num_rows_reported", 0);

   // Check for biasing address card
   error = load_param_or_exit(mce, &m_sq2fb, SQ2_CARD, SQ2_FB_COL "0", 1);
   if (error) {
       sprintf(errmsg_temp,
	       "Biasing address card (bac) required to run all-row servo!"); 
       ERRPRINT(errmsg_temp);
       exit(ERR_MCE_PARA);
   }
     
   // Load params for all columns
   for (i=0; i<MAXCHANNELS; i++) {
       sprintf(tempbuf, "%s%i", SQ2_FB_COL, i+soffset);
       load_param_or_exit(mce, m_sq2fb_col+i, SQ2_CARD, tempbuf, 0);
   }

   // Make sure this card can go!
   sprintf(tempbuf, "rc%i", which_rc);
   load_param_or_exit(mce, &m_retdat, tempbuf, "ret_dat", 0);

   u32 nrows_rep;
   if ((error=mcecmd_read_element(mce, &m_nrows_rep, 0, &nrows_rep)) != 0){
     sprintf(errmsg_temp, "rb cc num_rows_reported failed with %d", error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_RB;
   }

   // Our callback will update the counter in this structure
   servo_t sq1servo;
   sq1servo.fcount = 0;
   sq1servo.which_rc=which_rc;
   sq1servo.num_rows = nrows_rep;

   // setup a call back function
   mcedata_storage_t* ramb;
   ramb = mcedata_rambuff_create(frame_callback, (unsigned long) &sq1servo);
   
   // Pick a card (won't work for rcs!!)
   int cards=(1<<(which_rc-1));
   printf("Card bits=%#x, num_rows_reported=%d\n", cards, (int)nrows_rep);
   mce_acq_t acq;
   error = mcedata_acq_create(&acq, mce, 0, cards, (int) nrows_rep, ramb);
   if (error != 0) {
     sprintf(errmsg_temp, "acq_setup failed [%i]: %s\n", error, mcelib_error_string(error));
     ERRPRINT(errmsg_temp);
     return ERR_MCE_GO;
   }

   if ( (datadir=getenv("MAS_DATA")) == NULL){
     ERRPRINT("Enviro var. $MAS_DATA not set, quit");
     return ERR_DATA_DIR;
   }
   sprintf(full_datafilename, "%s%s",datadir, datafile);
   
   // open a datafile 
   if( (sq1servo.df = fopen(full_datafilename, "w")) == NULL){
     sprintf(errmsg_temp, "opening data file: %s", full_datafilename);
     ERRPRINT(errmsg_temp);
     return ERR_DATA_FIL;
   }
                          
   /* For now we'll just have one .bias file for each row */
   
   /* Open output file to append modified data set */
   for (i=0; i<nrows_rep; i++) {
       sprintf(outfile, "%s%s.r%02i.bias", datadir, datafile, i);
       bias_fd[i] = fopen ( outfile, "a" );
   }

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
   for ( j=0; j< which_rc*MAXCHANNELS; j++ ) {
     if (fgets (line, MAXLINE, tempf) == NULL) {
       ERRPRINT ("reading sq2fb.init quitting....");
       return ERR_INI_READ;
     }
     sq2fb[j][0] = atoi(line);
     for ( i=1; i<nrows_rep; i++)
       sq2fb[j][i] = sq2fb[j][0];
   
     sprintf(tempbuf, "%d ", sq2fb[j][0]);
     strcat(init_line1, tempbuf);
   }
   fclose(tempf);

   /** generate a runfile **/
   sprintf(init_line2, "# All rows servoed simultaneously");
   error=genrunfile (full_datafilename, datafile, 1, which_rc, 
                      sq1bias, sq1bstep, nbias, sq1feed, sq1fstep, nfeed, 
                      init_line1, init_line2);
   if (error != 0){
     sprintf(errmsg_temp, "genrunfile %s.run failed with %d", datafile, error);
     ERRPRINT(errmsg_temp);
     return ERR_RUN_FILE;
   }

   /* generate the header line for the bias files*/
   for (i=0; i<nrows_rep; i++) {
       for ( snum=0; snum<MAXCHANNELS; snum++)
	   fprintf ( bias_fd[i], "<error%02d_r%02i> ", snum + soffset, i);
       for ( snum=0; snum<MAXCHANNELS; snum++)
	   fprintf ( bias_fd[i], "<sq2fb%02d_r%02i> ", snum + soffset, i); 
       fprintf ( bias_fd[i], "\n");
   }

   /* start the servo*/
   for ( j=0; j<nbias; j++ ){

      if (!skip_sq1bias){
	duplicate_fill(sq1bias + j*sq1bstep, temparr, MAXROWS);
        if ((error = mcecmd_write_block(mce, &m_sq1bias, nrows_rep, temparr)) != 0) 
          error_action("mcecmd_write_block sq1bias", error);
      }

      for ( i=0; i<nfeed; i++ ){

	// Write all rows fb to each squid 2
	for (snum=0; snum<MAXCHANNELS; snum++) {
	  write_range_or_exit(mce, m_sq2fb_col+snum, 0, sq2fb[snum+soffset],
			      nrows_rep, "sq2fb_col");
	}

	// Next sq1 fb ramp value.
	duplicate_fill(sq1feed + i*sq1fstep, temparr, MAXCHANNELS);
	write_range_or_exit(mce, &m_sq1fb, soffset, temparr, MAXCHANNELS, "sq1fb");	
	
	// Get a frame
	if ((error=mcedata_acq_go(&acq, 1)) != 0)
	  error_action("data acquisition failed", error);

	// Compute new feedback for each column, row
	for ( snum=0; snum<MAXCHANNELS; snum++) {
	  for (r=0; r<nrows_rep; r++) {
	    sq2fb[snum + soffset][r] += gain *
		( (int)sq1servo.last_frame[r*MAXVOLTS + snum + soffset] - z );
	  }
	}

	// Write errors and computed feedbacks to .bias files.
	for (r=0; r<nrows_rep; r++) {
	    for (snum=0; snum<MAXCHANNELS; snum++)	 
		fprintf(bias_fd[r], "%13d ", sq1servo.last_frame[r*MAXVOLTS + snum+soffset]);
	    for (snum=0; snum<MAXCHANNELS; snum++)	 
		fprintf(bias_fd[r], "%13d ", sq2fb[snum+soffset][r]);
	    fprintf ( bias_fd[r], "\n" );
	}

      }
   }

   /* reset values back to 0 */
   duplicate_fill(0, temparr, MAXCHANNELS);
   write_range_or_exit(mce, &m_sq2fb, soffset, temparr, MAXCHANNELS, "sq2fb");
 
   duplicate_fill((u32)(-8192), temparr, MAXCHANNELS);
   write_range_or_exit(mce, &m_sq1fb, soffset, temparr, MAXCHANNELS, "sq1fb");	
   
   if (!skip_sq1bias){
     duplicate_fill((u32)(-8192), temparr, MAXROWS);
     if ((error = mcecmd_write_block(mce, &m_sq1bias, nrows_rep, temparr)) != 0) 
       error_action("mcecmd_write_block sq1bias", error);
   }
   else
      printf("No SQ1 bias is applied!\n"); 

   fclose(sq1servo.df);
   for (i=0; i<nrows_rep; i++)
       fclose(bias_fd[i]);

   mcelib_destroy(mce);
   
   time(&finish);
   printf("sq1servo: elapsed time is %fs \n", difftime(finish, start));
      
   return SUCCESS;
}
