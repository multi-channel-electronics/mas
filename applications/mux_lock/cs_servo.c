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
 *    cs_servo
 *
 *    Based on rs_servo.c.  For mux11d two-level addressing.
 *
 *    Sweeps the chip-select flux (ac2 on_bias) while servoing SA FB,
 *    producing a .bias file that the Python CSServo class analyzes to
 *    determine per-chip on/off bias levels.
 *
 *    No bias ramp, no hybrid mux support, always outputs all rows.
 *    Ramp parameters come from experiment.cfg (cs_servo_flux_*).
 **************************************************************************/

struct {

    char *filename;

    int rc;         /* [1,2,3,4] or 0 for RCS */
    int column_0;   /* First column to servo */
    int column_n;   /* Number of columns to servo (and in readout frame) */

    int rows;       /* Row count */

    int fb;         /* cs_servo_flux_start */
    int dfb;        /* cs_servo_flux_step */
    int nfb;        /* cs_servo_flux_count */

    int ac2_num_rows;  /* Number of ac2 on_bias entries to fill */

    int sfb_init[MAXCOLS];

    double target[MAXCOLS];
    double gain[MAXCOLS];
    int quanta[MAXCOLS];
    int columns_off[MAXCOLS];

} control;


/***********************************************************
 * frame_callback: store the frame and fill row_data
 **********************************************************/
int frame_callback(unsigned long user_data, int frame_size, uint32_t *data)
{
    servo_t *myservo = (servo_t*)user_data;

    if (myservo->df != NULL)
        fwrite(data, sizeof(uint32_t), frame_size, myservo->df);

    memcpy(myservo->last_header, data, HEADER_OFFSET*sizeof(*data));
    memcpy(myservo->last_frame, data + HEADER_OFFSET,
            control.rows * control.column_n * sizeof(*data));

    myservo->fcount ++;
    return 0;
}


int load_exp_config(const char *filename)
{
    config_setting_t* cfg = load_config(filename);

    if (cfg == NULL) {
        fprintf(stderr, "Error loading config file: %s\n", filename);
        exit(1);
    }

    load_int(cfg, "default_num_rows", &control.rows);

    load_int(cfg, "cs_servo_flux_start", &control.fb);
    load_int(cfg, "cs_servo_flux_count", &control.nfb);
    load_int(cfg, "cs_servo_flux_step", &control.dfb);

    load_int(cfg, "ac2_num_rows", &control.ac2_num_rows);

    load_double_array(cfg, "cs_servo_gain",
            control.column_0, control.column_n, control.gain);
    load_int_array(cfg, "sa_fb",
            control.column_0, control.column_n, control.sfb_init);
    load_int_array(cfg, "sa_flux_quanta",
            control.column_0, control.column_n, control.quanta);
    load_int_array(cfg, "columns_off",
            control.column_0, control.column_n, control.columns_off);

    return 0;
}

/************************************************************
 *          M A I N
 ************************************************************/
int main(int argc, char **argv)
{
    char full_datafilename[256];
    const char *datadir;

    int32_t temparr[MAXTEMP];

    int i, r, snum;

    FILE *bias_out;
    char outfile[MAXLINE];
    char init_line1[MAXLINE];
    char init_line2[MAXLINE];

    int32_t safb[MAXCOLS][MAXROWS];

    int error = 0;
    char errmsg_temp[MAXLINE];

    time_t start, finish;

    option_t options = {
        .fibre_card = -1,
        .config_file = NULL,
        .hardware_file = NULL,
        .experiment_file = NULL,
        .argument_opts = 0,
    };
    int arg_offset = 0;

    time(&start);

    arg_offset = process_options(&options, argc, argv) - 1;
    if (arg_offset < 0)
        exit(ERR_NUM_ARGS);

    argc -= arg_offset;
    argv += arg_offset;

    if (argc != 3) {
        printf("Rev. 1.0\n");
        printf("usage: cs_servo [options] <rc> <filename>\n\n"
                "where\n"
                "    rc           readout card number (1-4)\n"
                "    filename     output file basename "
                "($$MAS_DATA will be prepended)\n");
        return ERR_NUM_ARGS;
    }

    memset(&control, 0, sizeof(control));

    if (argv[1][0] == 's') {
        control.rc = 0;
        control.column_0 = 0;
    } else {
        control.rc = atoi(argv[1]);
        control.column_0 = (control.rc - 1)*8;
    }
    control.filename = argv[2];

    /* Setup frame callback */
    servo_t sq1servo;
    mcedata_storage_t* ramb;
    ramb = mcedata_rambuff_create(frame_callback, (unsigned long) &sq1servo);

    /* Create MCE context */
    mce_context_t *mce = connect_mce_or_exit(&options);

    /* Configure acquisition */
    int cards = 0;
    if (control.rc != 0)
        cards = 1<<(control.rc-1);
    mce_acq_t acq;
    error = mcedata_acq_create(&acq, mce, 0, cards, control.rows, ramb);
    if (error != 0) {
        sprintf(errmsg_temp, "acq_setup failed [%i]: %s\n",
                error, mcelib_error_string(error));
        ERRPRINT(errmsg_temp);
        return ERR_MCE_GO;
    }
    control.rows = acq.rows;
    control.column_n = acq.cols * acq.n_cards;
    load_exp_config(options.experiment_file);

    printf("Card bits=%#x, column count=%i num_rows_reported=%d\n",
            acq.cards, control.column_n, control.rows);

    /* Lookup MCE parameters */
    mce_param_t m_chipsel, m_safb, m_safb_col[MAXCOLS];

    load_param_or_exit(mce, &m_chipsel, CHIPSEL_CARD, CHIPSEL_FB, 0);

    /* Verify mux11d */
    int mux11d = check_mux11d(mce, &m_safb, m_safb_col,
            control.column_0, control.column_n);
    if (mux11d != 1) {
        sprintf(errmsg_temp,
                "This system doesn't appear to be set up "
                "for mux11d operation.");
        ERRPRINT(errmsg_temp);
        exit(ERR_MCE_PARA);
    }

    if ((datadir = mcelib_lookup_dir(mce, MAS_DIR_DATA)) == NULL) {
        ERRPRINT("Error determining $MAS_DATA, quit");
        return ERR_DATA_DIR;
    }
    sprintf(full_datafilename, "%s/%s", datadir, control.filename);

    sq1servo.df = NULL;

    /* Open .bias output file */
    sprintf(outfile, "%s/%s.bias", datadir, control.filename);
    bias_out = fopen(outfile, "a");

    /* Initialize servo output */
    for (snum = 0; snum < control.column_n; snum++)
        for (r = 0; r < control.rows; r++)
            safb[snum][r] = control.sfb_init[snum];

    /* Record init values for runfile */
    sprintf(init_line1, "<safb.init>");
    for (int j = 0; j < control.column_n; j++)
        sprintf(init_line1 + strlen(init_line1), " %d", safb[j][0]);

    sprintf(init_line2, "<super_servo> 1");

    /* Generate runfile.  The flux sweep goes in loop1 (genrunfile's
       "bias" slot) so that load_ramp_params reads it correctly when
       par_title doesn't end with "bias". */
    error = genrunfile(
            full_datafilename, control.filename,
            MUX11D_CS_SERVO, control.rc,
            control.fb, control.dfb, control.nfb, 1,
            0, 0, 1,
            init_line1, init_line2,
            control.column_n, 1,
            control.gain, control.quanta);
    if (error != 0) {
        sprintf(errmsg_temp, "genrunfile %s.run failed with %d",
                control.filename, error);
        ERRPRINT(errmsg_temp);
        return ERR_RUN_FILE;
    }

    /* Write .bias header */
    fprintf(bias_out, "<bias> <flux> <row> ");
    for (snum = 0; snum < control.column_n; snum++)
        fprintf(bias_out, "<error%02d> ", snum + control.column_0);
    for (snum = 0; snum < control.column_n; snum++)
        fprintf(bias_out, "<safb%02d> ", snum + control.column_0);
    fprintf(bias_out, "\n");

    /* Initialize chip select */
    duplicate_fill(control.fb, temparr, control.ac2_num_rows);
    write_range_or_exit(mce, &m_chipsel, 0, temparr,
            control.ac2_num_rows, "chipsel");

    /* Sweep chip select flux */
    for (i = -options.preservo; i < control.nfb; i++) {

        /* Write SA FB per column */
        for (snum = 0; snum < control.column_n; snum++) {
            rerange(temparr, safb[snum], control.rows,
                    control.quanta+snum, 1);
            write_range_or_exit(mce, m_safb_col+snum, 0, temparr,
                    (m_safb_col+snum)->param.count, "safb_col");
        }

        if (i > 0) {
            /* Next chipsel value */
            duplicate_fill(control.fb + i*control.dfb, temparr,
                    control.ac2_num_rows);
            write_range_or_exit(mce, &m_chipsel, 0, temparr,
                    control.ac2_num_rows, "chipsel");
        }

        /* Acquire one frame */
        if ((error = mcedata_acq_go(&acq, 1)) != 0)
            error_action("data acquisition failed", error);

        /* Update servo */
        for (snum = 0; snum < control.column_n; snum++) {
            for (r = 0; r < control.rows; r++) {
                safb[snum][r] += control.gain[snum] *
                    ((int)sq1servo.last_frame[r*control.column_n + snum]
                     - control.target[snum]);
            }
        }

        /* Write to .bias file (all rows) */
        if (i >= 0) {
            for (r = 0; r < control.rows; r++) {
                fprintf(bias_out, " 0 %4i %2i ", i, r);
                for (snum = 0; snum < control.column_n; snum++)
                    fprintf(bias_out, "%13d ",
                            sq1servo.last_frame[
                                r*control.column_n + snum]);
                for (snum = 0; snum < control.column_n; snum++)
                    fprintf(bias_out, "%13d ", safb[snum][r]);
                fprintf(bias_out, "\n");
            }
        }
    }

    /* Reset ac2 on_bias to zero */
    duplicate_fill(0, temparr, control.ac2_num_rows);
    write_range_or_exit(mce, &m_chipsel, 0, temparr,
            control.ac2_num_rows, "chipsel");

    /* Reset SA FB to zero */
    duplicate_fill(0, temparr, MAXROWS);
    for (snum = 0; snum < control.column_n; snum++) {
        write_range_or_exit(mce, m_safb_col+snum, 0, temparr,
                (m_safb_col+snum)->param.count, "safb_col");
    }

    if (sq1servo.df != NULL)
        fclose(sq1servo.df);

    fclose(bias_out);

    mcelib_destroy(mce);

    time(&finish);
    printf("cs_servo: elapsed time is %fs \n", difftime(finish, start));

    return SUCCESS;
}
