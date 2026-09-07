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
 *    determine per-chip on/off bias levels.  Optionally this may be
 *    done for a series of sq1_bias values (cs_servo_bias_*).
 *
 *    No hybrid mux support, always outputs all rows.
 *    Ramp parameters come from experiment.cfg (cs_servo_flux_*,
 *    cs_servo_bias_*).
 **************************************************************************/

struct {

    char *filename;

    int rc;         /* [1,2,3,4] or 0 for RCS */
    int column_0;   /* First column to servo */
    int column_n;   /* Number of columns to servo (and in readout frame) */

    int rows;       /* Row count */

    int bias_active;  /* cs_servo_bias_ramp */
    int bias;         /* cs_servo_bias_start */
    int dbias;        /* cs_servo_bias_step */
    int nbias;        /* cs_servo_bias_count */

    int fb;         /* cs_servo_flux_start */
    int dfb;        /* cs_servo_flux_step */
    int nfb;        /* cs_servo_flux_count */

    int ac2_num_rows;  /* Number of ac2 on_bias entries to fill */
    int ac2_row_order[MAXACROWS];  /* ac2 on_bias register addresses */

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

    load_int(cfg, "cs_servo_bias_ramp", &control.bias_active);
    load_int(cfg, "cs_servo_bias_start", &control.bias);
    load_int(cfg, "cs_servo_bias_count", &control.nbias);
    load_int(cfg, "cs_servo_bias_step", &control.dbias);

    load_int(cfg, "ac2_num_rows", &control.ac2_num_rows);
    load_int_array(cfg, "ac2_row_order", 0, control.ac2_num_rows,
            control.ac2_row_order);

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

/* write_chipsel: write 'value' to ac2 on_bias at each configured chip
   select address.  Addresses (ac2_row_order) need not be contiguous or
   start at zero, so each is written individually. */
void write_chipsel(mce_context_t *mce, mce_param_t *m_chipsel, int32_t value)
{
    int32_t v = value;
    for (int j = 0; j < control.ac2_num_rows; j++)
        write_range_or_exit(mce, m_chipsel, control.ac2_row_order[j],
                &v, 1, "chipsel");
}

/************************************************************
 *          M A I N
 ************************************************************/
int main(int argc, char **argv)
{
    char full_datafilename[256];
    const char *datadir;

    int32_t temparr[MAXTEMP];

    int i, j, r, snum;

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

    /* Make sure we servo exactly once when bias is suppressed. */
    if (!control.bias_active)
        control.nbias = 1;

    /* Lookup MCE parameters */
    mce_param_t m_chipsel, m_sq1bias, m_safb, m_safb_col[MAXCOLS];

    load_param_or_exit(mce, &m_chipsel, CHIPSEL_CARD, CHIPSEL_FB, 0);
    load_param_or_exit(mce, &m_sq1bias, SQ1_CARD, SQ1_BIAS, 0);

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
    for (j = 0; j < control.column_n; j++)
        sprintf(init_line1 + strlen(init_line1), " %d", safb[j][0]);

    sprintf(init_line2, "<super_servo> 1");

    /* Generate runfile.  loop1 is the (optional) sq1 bias ramp, loop2
       is the chip-select flux sweep. */
    error = genrunfile(
            full_datafilename, control.filename,
            MUX11D_CS_SERVO, control.rc,
            control.bias, control.dbias, control.nbias, control.bias_active,
            control.fb, control.dfb, control.nfb,
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

    /* Sweep sq1 bias (if ramping), and chip select flux */
    for (j = 0; j < control.nbias; j++) {

        if (control.bias_active) {
            /* Update the SQ1 bias */
            duplicate_fill(control.bias + j*control.dbias, temparr,
                    control.column_n);
            write_range_or_exit(mce, &m_sq1bias, control.column_0, temparr,
                    control.column_n, "sq1bias");
        }

        /* Initialize chip select */
        write_chipsel(mce, &m_chipsel, control.fb);

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
                write_chipsel(mce, &m_chipsel, control.fb + i*control.dfb);
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
                    fprintf(bias_out, "%2i %4i %2i ", j, i, r);
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
    }

    /* Reset ac2 on_bias to zero */
    write_chipsel(mce, &m_chipsel, 0);

    /* Reset SA FB to zero */
    duplicate_fill(0, temparr, MAXROWS);
    for (snum = 0; snum < control.column_n; snum++) {
        write_range_or_exit(mce, m_safb_col+snum, 0, temparr,
                (m_safb_col+snum)->param.count, "safb_col");
    }

    if (control.bias_active) {
        duplicate_fill(0, temparr, control.column_n);
        write_range_or_exit(mce, &m_sq1bias, control.column_0, temparr,
                control.column_n, "sq1bias");
    } else {
        printf("SQ1 bias unchanged.\n");
    }

    if (sq1servo.df != NULL)
        fclose(sq1servo.df);

    fclose(bias_out);

    mcelib_destroy(mce);

    time(&finish);
    printf("cs_servo: elapsed time is %fs \n", difftime(finish, start));

    return SUCCESS;
}
