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
 *   * MA+MFH: repair multi-bias bug
 *
 ***********************************************************/


struct {

  char *filename; /* Basename for output data */

  int rc;         /* [1,2,3,4] or 0 for RCS */
  int column_0;   /* First column to servo */
  int column_n;   /* Number of columns to servo (and number in readout frame) */

  int rows;       /* Row count */

  int bias_active;
  int bias;
  int dbias;
  int nbias;
  
  int fb;
  int dfb;
  int nfb;
  
  int sfb_init[MAXCOLS];

  double target[MAXCOLS];
  double gain[MAXCOLS];
  int quanta[MAXCOLS];

  int row_choice[MAXCOLS];
	
} control;


int write_sq2fb(mce_context_t *mce, mce_param_t *m_sq2fb,
		mce_param_t *m_sq2fb_col, int biasing_ac,
		i32 *data, int offset, int count);


/***********************************************************
 * frame_callback: to store the frame to a file and fill row_data
 * 
 **********************************************************/
int frame_callback(unsigned long user_data, int frame_size, u32 *data)
{
  //Re-type 
  servo_t *myservo = (servo_t*)user_data;

  // Write frame to our data file
  fwrite(data, sizeof(u32), frame_size, myservo->df);

  // Copy header and data into myservo struct
  memcpy(myservo->last_header, data, HEADER_OFFSET*sizeof(*data));
  memcpy(myservo->last_frame, data + HEADER_OFFSET,
	 control.rows * control.column_n * sizeof(*data));

  myservo->fcount ++;
  return 0;
}


/* Parameters for management of the servo.  Filled in from experiment.cfg
   or command line options. */

int load_exp_config(const char *filename)
{
  config_setting_t* cfg = load_config(filename);

  if (cfg == NULL) {
    fprintf(stderr, "Error loading config file: %s\n", filename);
    exit(1);
  }

  load_int(cfg, "sq1_servo_flux_start", &control.fb);
  load_int(cfg, "sq1_servo_flux_count", &control.nfb);
  load_int(cfg, "sq1_servo_flux_step", &control.dfb);

  load_int(cfg, "sq1_servo_bias_ramp", &control.bias_active);
  load_int(cfg, "sq1_servo_bias_start", &control.bias);
  load_int(cfg, "sq1_servo_bias_count", &control.nbias);
  load_int(cfg, "sq1_servo_bias_step", &control.dbias);

  load_double_array(cfg, "sq1_servo_gain",
		    control.column_0, control.column_n, control.gain);

  load_int_array(cfg, "sq2_fb",
		 control.column_0, control.column_n, control.sfb_init);

  load_int_array(cfg, "sq2_flux_quanta",
		 control.column_0, control.column_n, control.quanta);

  load_int_array(cfg, "sq2_rows",
		 control.column_0, control.column_n, control.row_choice);

  return 0;
}

/************************************************************
 *          M A I N
 ************************************************************/
int main ( int argc, char **argv )
{
   char full_datafilename[MAXLINE]; /*full path for datafile*/
   char *datadir;
   
   i32 temparr[MAXTEMP];
  
   int i, j, snum;          /* loop counters */
 
   FILE *fd;                /* pointer to output file*/
   char outfile[MAXLINE];   /* output data file */
   char init_line1[MAXLINE];    /* record a line of init values and pass it to genrunfile*/
   char init_line2[MAXLINE];    /* record a line of init values and pass it to genrunfile*/

   char *endptr;
   i32 sq2fb[MAXCOLS];      /* sq2 feedback voltages */
   
   int error=0;
   char errmsg_temp[MAXLINE];
   
   time_t start, finish;

   /* Define default MAS options */
   option_t options = {
	   .fibre_card = -1,
	   .config_file = NULL,
     .hardware_file = NULL,
	   .experiment_file = NULL,
	   .argument_opts = 0,
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

   int args_ok = ((options.argument_opts && (argc == 12 || argc == 13)) ||
		  (!options.argument_opts && (argc == 3)));
   if (!args_ok) {
      printf("Rev. 3.0\n");
      printf("  (pass -E0 to get command line control of ramp parameters)\n");
   }
   if (!args_ok && options.argument_opts) {
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
   if (!args_ok && !options.argument_opts) {
     printf("When ramp parameters are loaded from experiment.cfg, arguments are:\n"
	    "    sq1servo [options] <rc> <filename>\n\n"
	    "where\n"
	    "    rc           readout card number (1-4)\n"
	    "    filename     output file basename ($MAS_DATA will be prepended)\n");
     return ERR_NUM_ARGS;
   }

   memset(&control, 0, sizeof(control));

   if (options.argument_opts) {
     control.filename = argv[1];

     control.bias = atoi(argv[2]);
     control.dbias = atoi(argv[3]);
     control.nbias = atoi(argv[4]);
     
     control.fb = atoi(argv[5]);
     control.dfb = atoi(argv[6]);
     control.nfb = atoi(argv[7]);

     // This does not support RCS
     control.rc = atoi(argv[8]);
     control.column_n = 8;
     control.column_0 = (control.rc - 1)*8;
     control.rows = atoi(argv[10]);

     for (i=0; i<control.column_n; i++) {
       control.target[i] = atoi(argv[9]);
       control.gain[i] = strtod(argv[11], &endptr);
     }

     control.bias_active = 1;
     if (argc == 13)
       control.bias_active = !(atoi(argv[12]));
   } else {
     if (argv[1][0] == 's') {
       control.rc = 0;
       control.column_0 = 0;
     } else {
       control.rc = atoi(argv[1]);
       control.column_0 = (control.rc - 1)*8;
     }
     // Fill in control.column_n later when we create the acq.
     control.filename = argv[2];
   }

   // Setup a call back function for frame data
   servo_t sq1servo;
   mcedata_storage_t* ramb;
   ramb = mcedata_rambuff_create(frame_callback, (unsigned long) &sq1servo);
   
   // Create MCE context
   mce_context_t *mce = connect_mce_or_exit(&options);

   // Configure acquisition (and use it to load important frame params...)
   int cards = 0; // By default, this will cause RCS
   if (control.rc != 0)
     cards = 1<<(control.rc-1);
   mce_acq_t acq;
   error = mcedata_acq_create(&acq, mce, 0, cards, control.rows, ramb);
   if (error != 0) {
     sprintf(errmsg_temp, "acq_setup failed [%i]: %s\n", error, mcelib_error_string(error));
     ERRPRINT(errmsg_temp);
     return ERR_MCE_GO;
   }
   control.rows = acq.rows;
   control.column_n = acq.cols * acq.n_cards;
   if (!options.argument_opts)
     load_exp_config(options.experiment_file);

   printf("Card bits=%#x, column count=%i num_rows_reported=%d\n",
	  acq.cards, control.column_n, control.rows);

   // Make sure we servo exactly once when bias is supressed.
   if (!control.bias_active)
     control.nbias = 1;

   // Lookup MCE parameters, or exit with error message.
   mce_param_t m_sq2fb, m_sq1fb, m_sq1bias, m_sq2fb_col[MAXCOLS];

   load_param_or_exit(mce, &m_sq1fb,   SQ1_CARD, SQ1_FB, 0);
   load_param_or_exit(mce, &m_sq1bias, SQ1_CARD, SQ1_BIAS, 0);

   // Check for biasing address card
   int fast_sq2 = check_fast_sq2(mce, &m_sq2fb, m_sq2fb_col,
				 control.column_0, control.column_n);

   if ((datadir=getenv("MAS_DATA")) == NULL) {
     ERRPRINT("Enviro var. $MAS_DATA not set, quit");
     return ERR_DATA_DIR;
   }
   sprintf(full_datafilename, "%s%s",datadir, control.filename);
   
   // open a datafile 
   if ((sq1servo.df = fopen(full_datafilename, "w")) == NULL) {
     sprintf(errmsg_temp, "openning data file: %s", full_datafilename);
     ERRPRINT(errmsg_temp);
     return ERR_DATA_FIL;
   }
                          
   /* Open output file to append modified data set */
   sprintf(outfile, "%s%s.bias", datadir, control.filename);
   fd = fopen ( outfile, "a" );

   if (options.argument_opts) {
     /* Get starting SQ2 feedback values  from a file called sq2fb.init*/
     load_initfile(datadir, "sq2fb.init", control.column_0, control.column_n, sq2fb);
     load_initfile(datadir, "row.init", control.column_0, control.column_n,
		   control.row_choice);
   } else {
     // Initialize servo output
     for (j=0; j<control.column_n; j++)
	 sq2fb[j] = control.sfb_init[j];
   }

   /* prepare lines of init values for runfile */
   sprintf(init_line1, "<sq2fb.init> ");
   sprintf(init_line2, "<row.init> ");
   for (j=0; j<control.column_n; j++) {
     sprintf(init_line1+strlen(init_line1), "%d ", sq2fb[j]);
     sprintf(init_line2+strlen(init_line2), "%d ", control.row_choice[j]);
   }

   /** generate a runfile **/
   error=genrunfile (full_datafilename, control.filename, 1, control.rc,
		     control.bias, control.dbias, control.nbias,
		     control.fb, control.dfb, control.nfb, control.bias_active,
		     init_line1, init_line2);
   if (error != 0) {
     sprintf(errmsg_temp, "genrunfile %s.run failed with %d", control.filename, error);
     ERRPRINT(errmsg_temp);
     return ERR_RUN_FILE;
   }

   /* generate the header line for the bias file*/
   for (snum=0; snum<control.column_n; snum++)
     fprintf ( fd, "  <error%02d> ", snum + control.column_0);
         
   for (snum=0; snum<control.column_n; snum++)
      fprintf ( fd, "  <sq2fb%02d> ", snum + control.column_0); 
   fprintf ( fd, "\n");

   /* start the servo*/
   for (j=0; j<control.nbias; j++ ){

      if (control.bias_active) {
	// Write *all* sq1bias here, or row-order destroys you.
	duplicate_fill(control.bias + j*control.dbias, temparr, MAXROWS);
        if ((error = mcecmd_write_block(mce, &m_sq1bias, MAXROWS, (u32*)temparr)) != 0) 
          error_action("mcecmd_write_block sq1bias", error);
      }

      // Initialize SQ1 FB
      duplicate_fill(control.fb, temparr, control.column_n);
      write_range_or_exit(mce, &m_sq1fb, control.column_0, temparr, control.column_n, "sq1fb");

      // Preservo and run the FB ramp.
      for (i=-options.preservo; i<control.nfb; i++ ){

	// Write SQ2 FB
	rerange(temparr, sq2fb, control.column_n, control.quanta, control.column_n);
	write_sq2fb(mce, &m_sq2fb, m_sq2fb_col, fast_sq2, temparr, control.column_0, control.column_n);

	if (i > 0) {
	  // Next sq1 fb ramp value.
	  duplicate_fill(control.fb + i*control.dfb, temparr, control.column_n);
	  write_range_or_exit(mce, &m_sq1fb, control.column_0, temparr, control.column_n, "sq1fb");
	}
	
	// Get a frame
	if ((error=mcedata_acq_go(&acq, 1)) != 0)
	  error_action("data acquisition failed", error);

	// Update output
	for (snum=0; snum<control.column_n; snum++) {
	  int idx = snum + control.column_n * control.row_choice[snum];
	  sq1servo.last_frame[snum] = sq1servo.last_frame[idx]; // For printing, below.
	  sq2fb[snum] += control.gain[snum] * ((int)sq1servo.last_frame[snum] - control.target[snum]);
	}

	if (i >= 0) {
	  // Record
	  for (snum=0; snum<control.column_n; snum++)	 
	    fprintf(fd, "%11d ", sq1servo.last_frame[snum]);
	  for (snum=0; snum<control.column_n; snum++)
	    fprintf ( fd, "%11d ", sq2fb[snum]);
	  fprintf ( fd, "\n" );
	}
      }
   }

   /* reset values back to 0 */
   duplicate_fill(0, temparr, control.column_n);
   write_range_or_exit(mce, &m_sq2fb, control.column_0, temparr, control.column_n, "sq2fb");
 
   duplicate_fill(-8192, temparr, control.column_n);
   write_range_or_exit(mce, &m_sq1fb, control.column_0, temparr, control.column_n, "sq1fb");	
   
   if (control.bias_active) {
     duplicate_fill(0, temparr, MAXROWS);
     if ((error = mcecmd_write_block(mce, &m_sq1bias, MAXROWS, (u32*)temparr)) != 0) 
       error_action("mcecmd_write_block sq1bias", error);
   }
   else
      printf("No SQ1 bias is applied!\n"); 

   fclose(sq1servo.df);
   fclose(fd);
   mcelib_destroy(mce);
   
   time(&finish);
   printf("sq1servo: elapsed time is %fs \n", difftime(finish, start));
      
   return SUCCESS;
}


/* Writes values to sq2 feedback columns offset through offset+count.
 * Even with biasing_ac, the same value is applied for the whole
 * column.  In that case, m_sq2fb_col should point to the desired
 * columns only (i.e. m_sq2fb_col[0] through mm_sq2fb_col[count-1] are
 * used, *not* m_sq2fb_col[offset] through [offset+count-1]. */

int write_sq2fb(mce_context_t *mce, mce_param_t *m_sq2fb,
		 mce_param_t *m_sq2fb_col, int biasing_ac,
		 i32 *data, int offset, int count)
{
	if (biasing_ac) {
		int i;
		i32 temparr[MAXROWS];
		for (i=0; i<count; i++) {
		        duplicate_fill(data[i], temparr, MAXROWS);
			write_range_or_exit(mce, m_sq2fb_col+i, 0, temparr, MAXROWS, "sq2fb_col");
		}
	} else {
		write_range_or_exit(mce, m_sq2fb, offset, data, control.column_n, "sq2fb");
	}
	return 0;
}

