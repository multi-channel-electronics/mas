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
 *    * MA+MFH: repair multi-bias bug
 *
 ***********************************************************/


int write_sq2fb(mce_context_t *mce, mce_param_t *m_sq2fb,
		mce_param_t *m_sq2fb_col, int biasing_ac,
		u32 *data, int offset, int count);

int servo_step(u32* dest, int* src, int count, double gain, double target);


/***********************************************************
 * frame_callback: to store the frame to a file and fill row_data
 *
 *********************************************************/ 
int frame_callback(unsigned long user_data, int frame_size, u32 *data) {
  
  //Re-type 
  servo_t *myservo = (servo_t*)user_data;
  
  fwrite(data, sizeof(u32), frame_size, myservo->df);
  
  myservo->fcount ++;
  
  int i;
  u32 *lastrow_base;
  lastrow_base = data + HEADER_OFFSET + (myservo->row_num[0])*MAXCHANNELS;
  for (i=0; i<MAXCHANNELS; i++) {
    myservo->row_data[i] = (int) *(lastrow_base + i);	
    //printf("%d ", myservo->row_data[i]); 
  } 
  return 0; 
}	
/************************************************************
 *          M A I N
 ************************************************************/
int main (int argc, char **argv)
{
   char datafile[MAXLINE];          /* datafile being written by DAS */
   char full_datafilename[MAXLINE]; /*full path for datafile*/
   char *datadir;
   char safb_initfile[MAXLINE];     /* filename for safb.init*/
   
   u32 temparr[MAXTEMP];    /* This must have at least rows, channels elements */
  
   int i;
   int j;
   int snum;                /* loop counter */
   
   FILE *fd;                /* pointer to output file*/
   FILE *tempf;             /* pointer to safb.init file*/
   char outfile[MAXLINE];   /* output data file */
   char line[MAXLINE]; 
   char init_line[MAXLINE];    /* record a line of init values and pass it to genrunfile*/
   char tempbuf[MAXLINE];
  
   double gain;             /* servo gain (=P=I) for the 0th calculation of the servo*/
   char *endptr;
   int nbias;
   int nfeed;
   u32 nrows_rep;
   u32 ssafb_init[MAXVOLTS]; /* starting values for feedback */
   u32 ssafb[MAXVOLTS];     /* series array feedback voltages */
   u32 ssafb_r[MAXVOLTS];   /* re-range SA fb */
   int sq2bias;             /* SQ2 bias voltage */
   int sq2bstep;            /* SQ2 bias voltage step */
   int sq2feed;             /* SQ2 feedback voltage */
   int sq2fstep;            /* SQ2 feedback voltage step */
   double z;                /* servo feedback offset */ 
   int  which_rc;
   int  soffset;
   int  skip_sq2bias = 0;
   int  biasing_ac = 0;     /* does MCE have a biasing address card? */

   config_setting_t *cfg;   /* experiment.cfg data */
   u32 *sa_quanta;          /* Series array phi0 in feedback units */

   int  error = 0;
   char errmsg_temp[MAXLINE];
   
   time_t start, finish;
   
   /* Define default MAS options */
   option_t options = {
     config_file:   DEFAULT_MASFILE,
     cmd_device:    DEFAULT_CMDFILE,
     data_device:   DEFAULT_DATAFILE,
     hardware_file: DEFAULT_HARDWAREFILE,
     preservo:      0,
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
   if (argc != 11 && argc != 12)
   {  
      printf("Rev. 2.1\n");
      printf("usage: sq2servo outfile sq2bias sq2bstep nbias\n");
      printf("sq2feed sq2fstep nfeed N target gain skip_sq2bias\n");
      printf("   outfile = name of file for output data\n");
      printf("   sq2bias = starting SQ2 bias\n");
      printf("   sq2bstep = step for SQ2 bias\n");
      printf("   nbias = number of bias steps\n");
      printf("   sq2feed = starting SQ2 feedback\n");
      printf("   sq2fstep = step for SQ2 feedback\n");
      printf("   nfeed = number of feedback steps\n");
      printf("   N = readout-card number (1 to 4)\n");
      printf("   target = lock target \n");
      printf("   gain = servo gain (double) \n");
      printf("   skip_sq2bias (optional) = if specified as 1, then no sq2_bias is applied.\n");
      ERRPRINT("wrong number of arguments");
      return ERR_NUM_ARGS;
   }
   /* Get range of values for second stage SQUIDs */
   strcpy(datafile, argv[1]);
   sq2bias = atoi(argv[2]);
   sq2bstep = atoi(argv[3]);
   nbias = atoi(argv[4]);
   sq2feed = atoi(argv[5]);
   sq2fstep = atoi(argv[6]);
   nfeed = atoi(argv[7]);
   which_rc = atoi(argv[8]);
   z = atoi(argv[9]);
   gain = strtod(argv[10], &endptr);
   
   if (argc == 12) {
      skip_sq2bias = atoi(argv[11]);
      if (nbias <1) nbias = 1;
   }
   else
      skip_sq2bias = 0;
   
   // All of our per-column parameters must be written at the right index
   soffset = (which_rc-1)*MAXCHANNELS;

   // Try to load SA quanta
   cfg = load_config("/data/cryo/current_data/experiment.cfg");
   sa_quanta = (u32*)load_int_array(cfg, "sa_flux_quanta", soffset+MAXCHANNELS);
   if (sa_quanta==NULL) {
     ERRPRINT("Could not load 'sa_flux_quanta' from config file.");
     exit(ERR_MCE_ECFG);
   } else {
     //printf("SA QUANTA:\n");
     for (i=0; i<MAXCHANNELS; i++) {
       u32 q = sa_quanta[i+soffset];
       sa_quanta[i+soffset] = q * (SA_DAC / q);
       //printf(" %i", q);
     }
     //printf("\n");
   }

   // Create MCE context
   mce_context_t *mce = connect_mce_or_exit(&options);

   // Lookup MCE parameters, or exit with error message
   mce_param_t m_safb, m_sq2fb, m_sq2bias, m_nrows_rep, m_retdat,
     m_sq2fb_col[MAXVOLTS];

   load_param_or_exit(mce, &m_safb,    SA_CARD,  SA_FB, 0);
   load_param_or_exit(mce, &m_sq2bias, SQ2_CARD, SQ2_BIAS, 0);
   load_param_or_exit(mce, &m_nrows_rep, "cc", "num_rows_reported", 0);

   // Check for biasing address card
   error = load_param_or_exit(mce, &m_sq2fb,   SQ2_CARD, SQ2_FB, 1);
   if (error != 0) {
     error = load_param_or_exit(mce, &m_sq2fb, SQ2_CARD, SQ2_FB_COL "0", 1);
     if (error) {
       sprintf(errmsg_temp, "Neither %s %s nor %s %s could be loaded!",
	       SQ2_CARD, SQ2_FB, SQ2_CARD, SQ2_FB_COL "0"); 
       ERRPRINT(errmsg_temp);
       exit(ERR_MCE_PARA);
     }
     biasing_ac = 1;
     
     // Load params for all columns
     for (i=0; i<MAXCHANNELS; i++) {
       sprintf(tempbuf, "%s%i", SQ2_FB_COL, i+soffset);
       load_param_or_exit(mce, m_sq2fb_col+i, SQ2_CARD, tempbuf, 0);
     }
   }     

   // Make sure this card can go!
   sprintf(tempbuf, "rc%i", which_rc);
   load_param_or_exit(mce, &m_retdat, tempbuf, "ret_dat", 0);

   if ((error=mcecmd_read_element(mce, &m_nrows_rep, 0, &nrows_rep)) != 0) {
     sprintf(errmsg_temp, "rb cc num_rows_reported failed with %d", error);
     ERRPRINT(errmsg_temp);
     return ERR_MCE_RB;     
   }	   

   // Our callback will update the counter in this structure
   servo_t sq2servo; 
   sq2servo.fcount = 0;
   sq2servo.which_rc = which_rc;
   sq2servo.row_num[0] = nrows_rep - 1;

   // setup a call back function
   mcedata_storage_t* ramb;
   ramb = mcedata_rambuff_create(frame_callback, (unsigned long) &sq2servo);
   
   // Pick a card (won't work for rcs!!)
   int cards=(1<<(which_rc-1));
   printf("Card bits=%#x, num_rows_reported=%d\n", cards, (int)nrows_rep);
   mce_acq_t acq;
   mcedata_acq_create(&acq, mce, 0, cards, (int) nrows_rep, ramb);

   if ((datadir=getenv("MAS_DATA")) == NULL) {
      ERRPRINT("Enviro var. $MAS_DATA not set, quit");
      return ERR_DATA_DIR;
   }
   sprintf(full_datafilename, "%s%s",datadir, datafile);

   // open a datafile 
   if ((sq2servo.df = fopen(full_datafilename, "w")) == NULL) {
     sprintf(errmsg_temp, "openning data file: %s", full_datafilename);
     ERRPRINT(errmsg_temp);
     return ERR_DATA_FIL;
   }
   
/* Open output file to append modified data set */
   sprintf(outfile, "%s%s.bias", datadir, datafile);
   fd = fopen (outfile, "a");

/* Get starting SA feedback values  from a file called safb.init*/
   strcpy (safb_initfile, datadir);
   strcat (safb_initfile, "safb.init");
   if ((tempf = fopen (safb_initfile, "r")) == NULL) {
      ERRPRINT("failed to open safb.init to read initial settings for safb");
      return ERR_SAFB_INI;
   }
   
   /* prepare a line of init values for runfile*/   
   sprintf(init_line, "<safb.init> ");
   for (j=0; j<(which_rc)*MAXCHANNELS; j++) {
     if (fgets (line, MAXLINE, tempf) == NULL) {
       ERRPRINT("reading safb.init quitting...."); 
       return ERR_INI_READ;
     }
     ssafb_init[j] = atoi(line);
     sprintf(tempbuf, "%d ", ssafb_init[j]);
     strcat(init_line, tempbuf);
   }
   fclose(tempf);

   /** generate a runfile **/
   error=genrunfile (full_datafilename, datafile, 2, which_rc, 
                      sq2bias, sq2bstep, nbias, sq2feed, sq2fstep, nfeed, 
                      init_line, NULL);
   if (error != 0) {
     sprintf(errmsg_temp, "genrunfile %s.run failed with %d", full_datafilename, error);
     ERRPRINT(errmsg_temp);
     return ERR_RUN_FILE;
   }

   /* generate the header line for the bias file*/
   for (snum=0; snum<MAXCHANNELS; snum++)
     fprintf(fd, "  <error%02d> ", snum + soffset);  
         
   for (snum=0; snum<MAXCHANNELS; snum++)
     fprintf(fd, "  <ssafb%02d> ", snum + soffset);
   fprintf(fd, "\n");
  

   // finally, the servo loop:
   for (j=0; j<nbias; j++) {

      if (!skip_sq2bias) {
	duplicate_fill((u32)sq2bias + j*sq2bstep, temparr, MAXCHANNELS);
	write_range_or_exit(mce, &m_sq2bias, soffset, temparr, MAXCHANNELS, "sq2bias");
      }

      // Load starting fb values
      for (snum=0; snum<MAXVOLTS; snum++)
	      ssafb[snum] = ssafb_init[snum];

      // Set starting sa fb and sq2 fb
      duplicate_fill(sq2feed, temparr, MAXCHANNELS);
      write_sq2fb(mce, &m_sq2fb, m_sq2fb_col, biasing_ac, temparr, soffset, MAXCHANNELS);
	      
      for (i=0; i<options.preservo; i++) {
	      write_range_or_exit(mce, &m_safb, soffset, ssafb + soffset, MAXCHANNELS, "safb");

	      if ((error=mcedata_acq_go(&acq, 1)) != 0) 
		      error_action("data acquisition failed", error);

	      servo_step(ssafb+soffset, sq2servo.row_data, MAXCHANNELS, gain, z);
      }

      for (i=0; i<nfeed; i++) {

	 rerange(ssafb_r + soffset, ssafb + soffset, MAXCHANNELS, sa_quanta, MAXCHANNELS);
	 write_range_or_exit(mce, &m_safb, soffset, ssafb_r + soffset, MAXCHANNELS, "safb");

	 if (biasing_ac) {
	   // Must write all rows here, since row_order may bring any
	   // real row into the smaller subset we're tuning.
	   duplicate_fill(sq2feed + i*sq2fstep, temparr, MAXROWS);
	   for (snum=0; snum<MAXCHANNELS; snum++) {
	     write_range_or_exit(mce, m_sq2fb_col+snum, 0, temparr, MAXROWS, "sq2fb_col");
	   }
	 } else {
	   duplicate_fill(sq2feed + i*sq2fstep, temparr, MAXCHANNELS);
	   write_range_or_exit(mce, &m_sq2fb, soffset, temparr, MAXCHANNELS, "sq2fb");
	 }

	 if ((error=mcedata_acq_go(&acq, 1)) != 0) 
	   error_action("data acquisition failed", error);

         /* now extract each column's reading*/
         for (snum=0; snum<MAXCHANNELS; snum++)
	   fprintf(fd, "%11d ", sq2servo.row_data[snum]);

         for (snum=0; snum<MAXCHANNELS; snum++) {
           ssafb[snum+soffset] += gain * ((sq2servo.row_data[snum])-z);
           fprintf(fd, "%11d ", ssafb[snum+soffset]);
         }
         fprintf(fd, "\n");
         
      }
   }

   /* reset biases back to 0*/
   duplicate_fill(0, temparr, MAXCHANNELS);
   write_range_or_exit(mce, &m_sq2fb, soffset, temparr, MAXCHANNELS, "sq2fb");
   
   duplicate_fill(0, ssafb+soffset, MAXCHANNELS);
   write_range_or_exit(mce, &m_safb, soffset, ssafb + soffset, MAXCHANNELS, "safb");
   
   if (!skip_sq2bias) {
     duplicate_fill(sq2bias, temparr, MAXCHANNELS);
     write_range_or_exit(mce, &m_sq2bias, soffset, temparr, MAXCHANNELS, "sq2bias");
   }  
   else
     printf("This script did not apply SQ2 bias, you may need to turn biases off manually!\n");
   
   fclose(fd);
   fclose(sq2servo.df);
   mcelib_destroy(mce);

   time(&finish);
   //elapsed = ((double) (end - start))/CLOCKS_PER_SEC;
   printf("sq2servo: elapsed time is %fs \n", difftime(finish,start));	   
   return SUCCESS;
}

/* Writes values to sq2 feedback columns offset through offset+count.
 * Even with biasing_ac, the same value is applied for the whole
 * column.  In that case, m_sq2fb_col should point to the desired
 * columns only (i.e. m_sq2fb_col[0] through mm_sq2fb_col[count-1] are
 * used, *not* m_sq2fb_col[offset] through [offset+count-1]. */

int write_sq2fb(mce_context_t *mce, mce_param_t *m_sq2fb,
		 mce_param_t *m_sq2fb_col, int biasing_ac,
		 u32 *data, int offset, int count)
{
	if (biasing_ac) {
		int i;
		u32 temparr[MAXROWS];
		for (i=0; i<count; i++) {
			duplicate_fill(data[i], temparr, MAXROWS);
			write_range_or_exit(mce, m_sq2fb_col+i, 0, temparr, MAXROWS, "sq2fb_col");
		}
	} else {
		write_range_or_exit(mce, m_sq2fb, offset, data, MAXCHANNELS, "sq2fb");
	}
	return 0;
}

int servo_step(u32* dest, int* src, int count, double gain, double target)
{
	int i;
	for (i=0; i<count; i++) {
		dest[i] += gain * (src[i] - target);
	}
	return 0;
}
