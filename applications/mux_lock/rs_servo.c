/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "servo_err.h"
#include "servo.h"

/**************************************************************************
 *    rs_servo
 *
 *    Based on sq1servo_all.c.  See "svn log" for revision history.
 *
 *    For mux11d operation.  Locks the SQ1 by computing a new sa_fb
 *    for every row and column while sweeping the "row select" signals
 *    on all rows..  Optionally this may be done for a series of
 *    sq1_bias values.
 *
 *    As is typical for these programs, a lot of prep has to be done
 *    already; in particular you need the SQ1 FB (and maybe bias) to
 *    be not fast-switching.
 *
 *    See the function "load_exp_config" to see what experiment.cfg
 *    parameters are used to control the servo ramps.
 **************************************************************************/


struct {

  char *filename;

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
  
  int row_desel[MAXROWS];

  int sfb_init[MAXCOLS];

  double target[MAXCOLS];
  double gain[MAXCOLS];
  int quanta[MAXCOLS];
  int columns_off[MAXCOLS];

  /* For single row mode, on slow SA... */
  int super_servo;
  int row_choice[MAXCOLS];

  /* For mux11d hybrid muxing */
  int hybrid_rs_active;
  int hybrid_ac_idle_row;

  int nhybrid_rs_cards;
  char* hybrid_rs_cards[MAXCARDS];
  double hybrid_rs_multipliers[MAXCARDS];
  int hybrid_rs_cards_row0[MAXCARDS];
  int hybrid_mux_order[MAXROWS];

} control;

/* Info for turning hybrid mux11d addresses into BC DAC targets. */
typedef struct {
    int is_bc;                   /* If True, row is managed by some BC, not AC. */
    mce_param_t mce_param;       /* The mce_param of the relevant "bc? fb_col?" */
    char param_name[16];         /* The name to pass to write_range_or_exit. */
    int param_offset;            /* The index within bc? fb_col? to which the on
                                    value should go... i.e. the row number. */
    double multiplier;           /* The rescaling to apply to feedback values 
                                    before writing them. */
} row_info_t;

row_info_t row_infos[MAXROWS];
    
void init_row_info(mce_context_t *mce)
{
    for (int row=0; row<control.rows; row++) {
        /* Init row info to 0... which includes is_bc = False. */
        row_info_t *info = &row_infos[row];
        memset(info, 0, sizeof(*info));

        /* If not hybrid, short-circuit. */
        if (!control.hybrid_rs_active)
            continue;

        /* Take this row's mux_order element and figure out what
           rs_card it belongs to.  Assumes row0 vector is strictly
           increasing. */
        int rs_card = 0;
        while ((rs_card < control.nhybrid_rs_cards) && 
               (control.hybrid_mux_order[row] >= control.hybrid_rs_cards_row0[rs_card]))
            rs_card++;
        rs_card--; // It's the one before that.
        int param_offset = (control.hybrid_mux_order[row] - 
                            control.hybrid_rs_cards_row0[rs_card]);
        char *card_name = control.hybrid_rs_cards[rs_card];

        /* Copy information into the row_info. */
        info->is_bc = (strcmp(card_name, "ac") != 0);
        info->multiplier = control.hybrid_rs_multipliers[rs_card];

        if (info->is_bc) {
            /* For bias cards, offset into the BC DAC register is
               simply the row index.  Use the mux_order to get the right DAC register. */
            char param_name[16];
            sprintf(param_name, "fb_col%d", param_offset);
            sprintf(info->param_name, "%s_%s", card_name, param_name);
            load_param_or_exit(mce, &info->mce_param, card_name, param_name, 0);
            info->param_offset = row;
            printf("init_row: %2i  maps to %s %s\n", row, card_name, param_name);
        }
    }
}

/* update_bc_row_select -- writes the appropriate "bc? fb_col?" vector
   with the off_value, except for at index row, where is written on_value. */
void update_bc_row_select(mce_context_t *mce, int row, int on_value, int off_value)
{
    int32_t temparr[MAXTEMP];
    row_info_t *info = &row_infos[row];
    if (!info->is_bc)
        return;

    off_value = (info->multiplier * off_value);
    on_value = (info->multiplier * on_value);
    duplicate_fill(off_value, temparr, MAXROWS);
    temparr[info->param_offset] = on_value;
    write_range_or_exit(mce, &info->mce_param, 0, temparr, MAXROWS, info->param_name);
}


/***********************************************************
 * frame_callback: to store the frame to a file and fill row_data
 * 
 **********************************************************/
int frame_callback(unsigned long user_data, int frame_size, uint32_t *data)
{
  //Re-type 
  servo_t *myservo = (servo_t*)user_data;

  // Write frame to our data file?
  if (myservo->df != NULL)
      fwrite(data, sizeof(uint32_t), frame_size, myservo->df);

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

  load_int(cfg, "default_num_rows", &control.rows);

  load_int(cfg, "rowsel_servo_flux_start", &control.fb);
  load_int(cfg, "rowsel_servo_flux_count", &control.nfb);
  load_int(cfg, "rowsel_servo_flux_step", &control.dfb);

  load_int(cfg, "rowsel_servo_bias_ramp", &control.bias_active);
  load_int(cfg, "rowsel_servo_bias_start", &control.bias);
  load_int(cfg, "rowsel_servo_bias_count", &control.nbias);
  load_int(cfg, "rowsel_servo_bias_step", &control.dbias);

  load_int_array(cfg, "row_deselect",
		 0, MAXROWS, control.row_desel);

  load_double_array(cfg, "rowsel_servo_gain",
		    control.column_0, control.column_n, control.gain);
  load_int_array(cfg, "sa_fb",
		 control.column_0, control.column_n, control.sfb_init);
  load_int_array(cfg, "sa_flux_quanta",
		 control.column_0, control.column_n, control.quanta);
  load_int_array(cfg, "columns_off",
		 control.column_0, control.column_n, control.columns_off);

  load_int(cfg, "config_mux11d_all_rows", &control.super_servo);
  load_int_array(cfg, "mux11d_row_choice",
		 control.column_0, control.column_n, control.row_choice);

  // hybrid mux11d rs
  if(config_setting_get_member(cfg, "mux11d_hybrid_row_select")!=NULL){
      load_int(cfg, "mux11d_hybrid_row_select", &control.hybrid_rs_active);
      // only load these parameters if hybrid RS has been requested
      if(control.hybrid_rs_active){
          control.nhybrid_rs_cards=load_hybrid_rs_cards(cfg, control.hybrid_rs_cards);
          
          load_int_array(cfg,"mux11d_mux_order",
                         0,MAXROWS,control.hybrid_mux_order);
          load_int_array(cfg,"mux11d_row_select_cards_row0",
                         0,control.nhybrid_rs_cards,control.hybrid_rs_cards_row0);
          
          // some basic validation of the hybrid inputs ; more extensive
          // checks in auto_setup/mux11d.py:do_init_mux11d() ; could
          // probably eliminate this and do it all there
          validate_hybrid_mux11d_mux_order(control.nhybrid_rs_cards,control.hybrid_rs_cards,
                                           control.hybrid_rs_cards_row0);
          
          load_double_array(cfg,"mux11d_row_select_multipliers",
                            0,control.nhybrid_rs_cards,control.hybrid_rs_multipliers);
          
          // probably don't need this...delete later if not used
          load_int(cfg, "mux11d_ac_idle_row", &control.hybrid_ac_idle_row);
      }
  } else control.hybrid_rs_active=0;
  
  return 0;
}

/************************************************************
 *          M A I N
 ************************************************************/
int main(int argc, char **argv)
{
   char full_datafilename[256]; /*full path for datafile*/
   const char *datadir;
   
   int32_t temparr[MAXTEMP];
  
   int i, j, r, snum;       /* loop counters */
 
   FILE *bias_out;              /* Output files for error and bias entries; one per row */
   char outfile[MAXLINE];       /* output data file */
   char init_line1[MAXLINE];    /* record a line of init values and pass it to genrunfile*/
   char init_line2[MAXLINE];    /* record a line of init values and pass it to genrunfile*/

   char *endptr;
   int32_t safb[MAXCOLS][MAXROWS];     /* sq2 feedback voltages */
   
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
      printf("usage:- rs_servo outfile sq1bias sq1bstep nbias " );
      printf("rowsel rowselstep nrowsel N target total_row gain skip_sq1bias\n" );
      printf("   outfile = filename for output data\n" );
      printf("   sq1bias = starting SQ1 bias\n" );
      printf("   sq1bstep = step size for SQ1 bias\n" );
      printf("   nbias = number of bias steps\n" );
      printf("   rowsel = starting row_selecty flux\n" );
      printf("   rowselstep = step size for row_select flux\n" );
      printf("   nrowsel = number of row_select flux steps\n" );
      printf("   N = readout-card number (1 to 4)\n");
      printf("   target = lock target \n");
      printf("   total_row = total number of rows in the system \n");
      printf("   gain = gain of the servo (double)\n");
      printf("   skip_sq1bias (optional) = if specified as 1, no SQ1 bias is applied\n");
      ERRPRINT("no argument specified");
      return ERR_NUM_ARGS;
   }
   if (!args_ok && !options.argument_opts) {
     printf("When ramp parameters are loaded from experiment.cfg, arguments are:\n"
	    "    rs_servo [options] <rc> <filename>\n\n"
	    "where\n"
	    "    rc           readout card number (1-4)\n"
	    "    filename     output file basename ($$MAS_DATA will be prepended)\n");
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
       control.sfb_init[i] = 0;
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
   mce_param_t m_safb, m_rowsel, m_sq1bias, m_safb_col[MAXCOLS];

   // Hybrid MUX setup
   if (control.hybrid_rs_active)
       init_row_info(mce);

   load_param_or_exit(mce, &m_rowsel,  ROWSEL_CARD, ROWSEL_FB, 0);
   load_param_or_exit(mce, &m_sq1bias, SQ1_CARD, SQ1_BIAS, 0);

   // Make sure this is the right kind of system (and init m_safb_col_)
   int mux11d = check_mux11d(mce, &m_safb, m_safb_col,
			     control.column_0, control.column_n);
   if (mux11d != 1) {
       sprintf(errmsg_temp,
	       "This system doesn't appear to be set up for mux11d operation."); 
       ERRPRINT(errmsg_temp);
       exit(ERR_MCE_PARA);
   }
     
   if ((datadir = mcelib_lookup_dir(mce, MAS_DIR_DATA)) == NULL) {
       ERRPRINT("Error determining $MAS_DATA, quit");
       return ERR_DATA_DIR;
   }
   sprintf(full_datafilename, "%s/%s", datadir, control.filename);
   
   // open a datafile?  Nahh...
   if (0) {
     if ((sq1servo.df = fopen(full_datafilename, "w")) == NULL){
       sprintf(errmsg_temp, "opening data file: %s", full_datafilename);
       ERRPRINT(errmsg_temp);
       return ERR_DATA_FIL;
     }
   } else {
     sq1servo.df = NULL;
   }
                          
   /* Bias file holds error and bias readings for all rows and columns */
   sprintf(outfile, "%s/%s.bias", datadir, control.filename);
   bias_out = fopen(outfile, "a");

   /* Initialize servo output */
   for (snum=0; snum<control.column_n; snum++)
     for (r=0; r<control.rows; r++) 
       safb[snum][r] = control.sfb_init[snum];

   /*prepare a line of init values for runfile*/
   sprintf(init_line1, "<safb.init>");
   for (j=0; j<control.column_n; j++ )
     sprintf(init_line1 + strlen(init_line1), " %d", safb[j][0]);

   /** generate a runfile **/
   sprintf(init_line2, "<super_servo> %i", control.super_servo);
   error=genrunfile (full_datafilename, control.filename, 4, control.rc,
		     control.bias, control.dbias, control.nbias, control.bias_active,
		     control.fb, control.dfb, control.nfb, 
                     init_line1, init_line2,
                     control.column_n, control.super_servo, 
                     control.gain, control.quanta);
   if (error != 0){
     sprintf(errmsg_temp, "genrunfile %s.run failed with %d", control.filename, error);
     ERRPRINT(errmsg_temp);
     return ERR_RUN_FILE;
   }

   /* generate the header line for the bias file*/
   fprintf (bias_out, "<bias> <flux> <row> ");
   for (snum=0; snum<control.column_n; snum++)
     fprintf (bias_out, "<error%02d> ", snum + control.column_0);
   for (snum=0; snum<control.column_n; snum++)
     fprintf (bias_out, "<safb%02d> ", snum + control.column_0); 
   fprintf (bias_out, "\n");

   /* start the servo*/
   for (j=0; j<control.nbias; j++ ){

      if (control.bias_active) {
       // Update the SQ1 bias
       duplicate_fill(control.bias + j*control.dbias, temparr, control.column_n);
       write_range_or_exit(mce, &m_sq1bias, control.column_0, temparr, control.column_n, "sq1bias");
      }
       
      // Initialize ROW SELECT -- write all rows so row_order doesn't get you
      duplicate_fill(control.fb, temparr, MAXROWS);
      write_range_or_exit(mce, &m_rowsel, 0, temparr, MAXROWS, "rowsel");
      
      // Zero this bc DAC for all row visits if it's being used as a hybrid rs
      if (control.hybrid_rs_active) {
          /* This will only update the BC DACs. */
          for (int row=0; row<control.rows; row++)
              update_bc_row_select(mce, row, 0, 0);
      }
      
      // Preservo and run the FB ramp.
      for (i=-options.preservo; i<control.nfb; i++ ){
          
          // Write all rows fb to each series array
          for (snum=0; snum<control.column_n; snum++) {
              rerange(temparr, safb[snum], MAXROWS, control.quanta+snum, 1);
              if (!control.super_servo) {
                  //Actually write them as all the same.
                  for (r=0; r<MAXROWS; r++)
                      temparr[r] = temparr[control.row_choice[snum]];
              }
              write_range_or_exit(mce, m_safb_col+snum, 0, temparr, MAXROWS, "safb_col");
          }
          
          if (i > 0) {
              // Next rowsel value.
              duplicate_fill(control.fb + i*control.dfb, temparr, MAXROWS);
              write_range_or_exit(mce, &m_rowsel, 0, temparr, MAXROWS, "rowsel");
              
              if (control.hybrid_rs_active)
                  for (int row=0; row<control.rows; row++)
                      update_bc_row_select(mce, row, control.fb + i*control.dfb, 0);
          }
          
          // Get a frame
          if ((error=mcedata_acq_go(&acq, 1)) != 0)
              error_action("data acquisition failed", error);
          
          // Compute new feedback for each column, row
          for (snum=0; snum<control.column_n; snum++) {
              for (r=0; r<control.rows; r++) {
                  safb[snum][r] += control.gain[snum] *
                      ((int)sq1servo.last_frame[r*control.column_n + snum] - control.target[snum] );
              }
          }
          
          // If ramping, write to file
          if (i >= 0) {
              if (control.super_servo) {
                  // Write errors and computed feedbacks to .bias file.
                  for (r=0; r<control.rows; r++) {
                      fprintf(bias_out, "%2i %4i %2i ", j, i, r);
                      for (snum=0; snum<control.column_n; snum++)	 
                          fprintf(bias_out, "%13d ", sq1servo.last_frame[r*control.column_n + snum]);
                      for (snum=0; snum<control.column_n; snum++)	 
                          fprintf(bias_out, "%13d ", safb[snum][r]);
                      fprintf (bias_out, "\n" );
                  }
              } else {
                  // Chosen rows only.
                  fprintf(bias_out, "%2i %4i %2i ", j, i, 0);
                  for (snum=0; snum<control.column_n; snum++) {
                      r = control.row_choice[snum];
                      fprintf(bias_out, "%13d ", sq1servo.last_frame[
                                                                     r*control.column_n + snum]);
                  }
                  for (snum=0; snum<control.column_n; snum++) {
                      r = control.row_choice[snum];
                      fprintf(bias_out, "%13d ", safb[snum][r]);
                  }
                  fprintf (bias_out, "\n" );
              }
          }
      }
   }

   /* reset values back to 0 */
   duplicate_fill(0, temparr, MAXROWS);
   for (snum=0; snum<control.column_n; snum++) {
       write_range_or_exit(mce, m_safb_col+snum, 0, temparr,
                           MAXROWS, "safb_col");
   }

   /* if in hybrid rs-scheme, reset those values back to zero too */
   if (control.hybrid_rs_active) {
       for (int row=0; row<control.rows; row++)
           update_bc_row_select(mce, row, 0, 0);
   }
   
   write_range_or_exit(mce, &m_rowsel, 0, temparr, MAXROWS, "rowsel");	
   
   if (control.bias_active) {
       duplicate_fill(0, temparr, control.column_n);
       write_range_or_exit(mce, &m_sq1bias, control.column_0, temparr, control.column_n, "sq1bias");
   }
   else
       printf("SQ1 bias unchanged.\n"); 
   
   if (sq1servo.df != NULL)
       fclose(sq1servo.df);
   
   fclose(bias_out);
   
   mcelib_destroy(mce);
   
   time(&finish);
   printf("sq1servo: elapsed time is %fs \n", difftime(finish, start));
      
   return SUCCESS;
}
