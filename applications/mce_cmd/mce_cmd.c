/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/*! \file mce_cmd.c
 *
 *  \brief Program to send commands to the MCE.
 *
 *  This program uses mce_library to send commands and receive
 *  responses from the MCE.  The program accepts instructions from
 *  standard input or from the command line using the -x option.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <mce/cmdtree.h>
#include <mce/data_ioctl.h>
#include <mce/defaults.h>

#include "cmd.h"
#include "options.h"

enum {
    ENUM_COMMAND_LOW,
	COMMAND_RB,
	COMMAND_WB,
	COMMAND_REL,
	COMMAND_WEL,
	COMMAND_RRA,
	COMMAND_WRA,
	COMMAND_GO,
	COMMAND_ST,
	COMMAND_RS,
	ENUM_COMMAND_HIGH,
	ENUM_SPECIAL_LOW,
	SPECIAL_HELP,
	SPECIAL_ACQ,
	SPECIAL_ACQ_PATH,
	SPECIAL_ACQ_CONFIG,
	SPECIAL_ACQ_CONFIG_FS,
	SPECIAL_ACQ_CONFIG_DIRFILE,
	SPECIAL_ACQ_CONFIG_DIRFILESEQ,
	SPECIAL_ACQ_FLUSH,
	SPECIAL_LOCK_QUERY,
	SPECIAL_LOCK_RESET,
	SPECIAL_LOCK_DOWN,
	SPECIAL_LOCK_UP,
	SPECIAL_QT_CONFIG,
	SPECIAL_QT_ENABLE,
	SPECIAL_MRESET,
	SPECIAL_DRESET,
	SPECIAL_REPLY_LOCK,
	SPECIAL_REPLY_UNLOCK,
	SPECIAL_CLEAR,
	SPECIAL_FAKESTOP,
	SPECIAL_EMPTY,
	SPECIAL_SLEEP,
	SPECIAL_COMMENT,
	SPECIAL_FRAME,
	SPECIAL_DISPLAY,
	SPECIAL_DEF,
	SPECIAL_DEC,
	SPECIAL_HEX,
	SPECIAL_ECHO,
	ENUM_SPECIAL_HIGH,
};


#define SEL_NO   (MASCMDTREE_SELECT | MASCMDTREE_NOCASE)

mascmdtree_opt_t anything_opts[] = {
    { MASCMDTREE_INTEGER, "", 0, -1, 0, anything_opts },
    { MASCMDTREE_STRING , "", 0, -1, 0, anything_opts },
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

mascmdtree_opt_t integer_opts[] = {
    { MASCMDTREE_INTEGER   , "", 0, -1, 0, integer_opts },
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

mascmdtree_opt_t string_opts[] = {
    { MASCMDTREE_STRING    , "", 0, -1, 0, string_opts },
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

mascmdtree_opt_t command_placeholder_opts[] = {
    { MASCMDTREE_INTEGER   , "", 0, -1, 0, integer_opts },
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};


mascmdtree_opt_t flat_args[] = {
    { MASCMDTREE_STRING | MASCMDTREE_ARGS, "filename", 0, -1, 0, flat_args+1 },
    { MASCMDTREE_STRING | MASCMDTREE_ARGS, "card"    , 0, -1, 0, NULL },
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};


mascmdtree_opt_t fs_args[] = {
    { MASCMDTREE_STRING | MASCMDTREE_ARGS, "filename", 0, -1, 0, fs_args+1 },
    { MASCMDTREE_STRING | MASCMDTREE_ARGS, "card"    , 0, -1, 0, fs_args+2 },
    { MASCMDTREE_INTEGER| MASCMDTREE_ARGS, "interval", 0, -1, 0, NULL},
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

mascmdtree_opt_t display_opts[] = {
    { SEL_NO, "DEF"     , 0, 0, SPECIAL_DEF     , NULL},
    { SEL_NO, "DEC"     , 0, 0, SPECIAL_DEC     , NULL},
    { SEL_NO, "HEX"     , 0, 0, SPECIAL_HEX     , NULL},
    { MASCMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};


mascmdtree_opt_t root_opts[] = {
	{ SEL_NO, "RB"      , 2, 3, COMMAND_RB, command_placeholder_opts},
	{ SEL_NO, "WB"      , 3,-1, COMMAND_WB, command_placeholder_opts},
/*  { SEL_NO, "REL"     , 3, 3, COMMAND_REL, command_placeholder_opts}, */
/*    SEL_NO, "WEL"     , 4, 4, COMMAND_WEL, command_placeholder_opts}, */
	{ SEL_NO, "RRA"     , 4, 4, COMMAND_RRA, command_placeholder_opts},
	{ SEL_NO, "WRA"     , 4,-1, COMMAND_WRA, command_placeholder_opts},
	{ SEL_NO, "GO"      , 2,-1, COMMAND_GO, command_placeholder_opts},
	{ SEL_NO, "STOP"    , 2,-1, COMMAND_ST, command_placeholder_opts},
	{ SEL_NO, "RS"      , 2,-1, COMMAND_RS, command_placeholder_opts},
	{ SEL_NO, "MCE_RESET", 0,0, SPECIAL_MRESET, NULL},
	{ SEL_NO, "DSP_RESET", 0,0, SPECIAL_DRESET, NULL},
	{ SEL_NO, "LOCK_REPLIES",   0,0, SPECIAL_REPLY_LOCK, NULL},
	{ SEL_NO, "UNLOCK_REPLIES", 0,0, SPECIAL_REPLY_UNLOCK, NULL},
	{ SEL_NO, "HELP"    , 0, 0, SPECIAL_HELP    , NULL},
	{ SEL_NO, "ACQ_GO"  , 1, 1, SPECIAL_ACQ     , integer_opts},
	{ SEL_NO, "ACQ_CONFIG", 2, 2, SPECIAL_ACQ_CONFIG, flat_args},
    { SEL_NO, "ACQ_CONFIG_FS", 3, 3, SPECIAL_ACQ_CONFIG_FS, fs_args},
    { SEL_NO, "ACQ_CONFIG_DIRFILE", 2, 2, SPECIAL_ACQ_CONFIG_DIRFILE,
        flat_args},
    { SEL_NO, "ACQ_CONFIG_DIRFILE_FS", 3, 3, SPECIAL_ACQ_CONFIG_DIRFILESEQ,
        fs_args},
    { SEL_NO, "ACQ_PATH" , 1, 1, SPECIAL_ACQ_PATH , string_opts},
    { SEL_NO, "ACQ_FLUSH", 0, 0, SPECIAL_ACQ_FLUSH, NULL},
	{ SEL_NO, "QT_ENABLE", 1, 1, SPECIAL_QT_ENABLE, integer_opts},
	{ SEL_NO, "QT_CONFIG", 1, 1, SPECIAL_QT_CONFIG, integer_opts},
	{ SEL_NO, "LOCK_QUERY", 0, 0, SPECIAL_LOCK_QUERY, NULL},
	{ SEL_NO, "LOCK_RESET", 0, 0, SPECIAL_LOCK_RESET, NULL},
	{ SEL_NO, "LOCK_DOWN" , 0, 0, SPECIAL_LOCK_DOWN, NULL},
	{ SEL_NO, "LOCK_UP"   , 0, 0, SPECIAL_LOCK_UP, NULL},
	{ SEL_NO, "FAKESTOP", 0, 0, SPECIAL_FAKESTOP, NULL},
	{ SEL_NO, "EMPTY"   , 0, 0, SPECIAL_EMPTY   , NULL},
	{ SEL_NO, "SLEEP"   , 1, 1, SPECIAL_SLEEP   , integer_opts},
	{ SEL_NO, "FRAME"   , 1, 1, SPECIAL_FRAME   , integer_opts},
	{ SEL_NO, "DISPLAY" , 1, 1, SPECIAL_DISPLAY , display_opts},
	{ SEL_NO, "ECHO"    , 1, 1, SPECIAL_ECHO    , integer_opts},
	{ SEL_NO, "#"       , 0,-1, SPECIAL_COMMENT , anything_opts},
	{ SEL_NO, "##"      , 0,-1, SPECIAL_COMMENT , anything_opts},
    { MASCMDTREE_TERMINATOR, "", 0,0,0, NULL},
};

/* Table for decoding RC strings into bit sets */
mascmdtree_opt_t rc_list[] = {
	{ SEL_NO, "rc1", 0, 0, MCEDATA_RC1, NULL },
	{ SEL_NO, "rc2", 0, 0, MCEDATA_RC2, NULL },
	{ SEL_NO, "rc3", 0, 0, MCEDATA_RC3, NULL },
	{ SEL_NO, "rc4", 0, 0, MCEDATA_RC4, NULL },
	{ SEL_NO, "rcs", 0, 0, 0, NULL},
    { MASCMDTREE_TERMINATOR, "", 0,0,0, NULL},
};

// Lazy old globals...

mce_context_t* mce;

char *line = NULL;
char *line_buffer = NULL;

options_t options = {
	.hardware_file =  NULL,
	.masconfig_file = NULL,
	.display =        SPECIAL_DEF,
	.acq_path =       "./",
	.use_readline =   1,
	.fibre_card =     -1,
};


/* kill_switch increments with each Ctrl-C (SIGINT).  When it gets to
   2, exit(1) is called by the signal handler.  If SIGINT is received
   while waiting for input (input_switch), the handler calls exit(0). */

int kill_switch = 0;
int input_switch = 0;

mce_acq_t* acq;

mce_param_t ret_dat_s;
mce_param_t num_rows_reported;

int  bit_count(int k);

int  menuify_mceconfig(mascmdtree_opt_t *opts);

int process_command(mascmdtree_opt_t *opts, mascmdtree_token_t *tokens,
        char *errmsg);

int pathify_filename(char *dest, const char *src);

void die(int sig);

int  main(int argc, char **argv)
{
	char msg[1024];
	FILE *ferr = stderr;
	FILE *fin  = stdin;
	int err = 0;

	if (process_options(&options, argc, argv)) {
		err = ERR_OPT;
		goto exit_now;
	}

	if (options.version_only) {
		printf("%s\n", VERSION_STRING);
		exit(0);
	}

	if (!options.nonzero_only) {
		printf("This is %s version %s\n",
		       PROGRAM_NAME, VERSION_STRING);
	}

	line_buffer = (char*) malloc(LINE_LEN);
	if (line_buffer==NULL) {
		fprintf(ferr, "memory error!\n");
		err = ERR_MEM;
		goto exit_now;
	}

    if ((mce = mcelib_create(options.fibre_card,
                    options.masconfig_file)) == NULL)
    {
        fprintf(ferr, "Could not initialise MAS.\n");
		err = ERR_MEM;
		goto exit_now;
	}

	// Connect command, data, and configuration modules.

    if (mcecmd_open(mce) != 0) {
        fprintf(ferr, "Could not open CMD device\n");
		err = ERR_MCE;
		goto exit_now;
    }

    if (mcedata_open(mce) != 0) {
        fprintf(ferr, "Could not open DATA device\n");
		err = ERR_MCE;
		goto exit_now;
	}

    if (mceconfig_open(mce, options.hardware_file, NULL)!=0) {
        fprintf(ferr, "Could not load MCE config file '%s'.\n",
                options.hardware_file);
        err = ERR_MCE;
        goto exit_now;
    }

    // Log!
    options.logger = maslog_connect(mce, "mce_cmd");

    menuify_mceconfig(root_opts);

    //Open batch file, if given
    if (options.batch_now) {
        fin = fopen(options.batch_file, "r");
        if (fin==NULL) {
            fprintf(ferr, "could not open batch file '%s'\n",
                    options.batch_file);
            sprintf(msg, "failed to read script '%s'\n", options.batch_file);
            maslog_print(options.logger, msg);
            err = ERR_MCE;
            goto exit_now;
        }
        sprintf(msg, "reading commands from '%s'\n", options.batch_file);
        maslog_print(options.logger, msg);
    }

    // Install signal handler for Ctrl-C and normal kill
    signal(SIGTERM, die);
    signal(SIGINT, die);

    char errmsg[1024] = "";
    char premsg[1024] = "";
    int line_count = 0;
    int done = 0;

    while (!done) {
        mascmdtree_token_t args[NARGS];
        size_t n = LINE_LEN;

        // Set input semaphore, then check kill condition.
        input_switch = 1;
        if (kill_switch) break;

        if ( options.cmds_now > 0 ) {
            line = options.cmd_set[options.cmds_idx++];
            done = (options.cmds_idx == options.cmds_now);
        } else {
            if (options.use_readline) {
                line = readline("");
                if (line == NULL)
                    break;
                strcpy(line_buffer, line);
                add_history(line);
                free(line);
                line = line_buffer;
            } else {
                line = line_buffer;
                int ret = getline(&line, &n, fin);
                if (ret == -1 || n==0 || feof(fin)) break;
            }

            n = strlen(line);
            if (line[n-1]=='\n') line[--n]=0;
            line_count++;
        }

        // Clear input semaphore; SIGs now will just set kill_switch
        input_switch = 0;

        if (options.no_prefix)
            premsg[0] = 0;
        else
            sprintf(premsg, "Line %3i : ", line_count);

        if (options.echo) {
            printf("Cmd  %3i : %s\n", line_count, line);
        }

        errmsg[0] = 0;
        args[0].n = 0;
        mascmdtree_debug = 0;

        err = mascmdtree_tokenize(args, line, NARGS);
        if (err) {
            strcpy(errmsg, "could not tokenize");
        }

        if (!err) {
            int count = mascmdtree_select( args, root_opts, errmsg);

            if (count < 0) {
                err = -1;
            } else if (count == 0) {
                if (options.interactive || args->n > 0) {
                    mascmdtree_list(errmsg, root_opts,
                            "mce_cmd expects argument from [ ", " ", "]");
                    err = -1;
                }
            } else {
                err = process_command(root_opts, args, errmsg);
                if (err==0) err = 1;
            }
        }

        if (err > 0) {
            if (*errmsg == 0) {
                if (!options.nonzero_only)
                    printf("%sok\n", premsg);
            } else
                printf("%sok : %s\n", premsg, errmsg);
        } else if (err < 0) {
            printf("%serror : %s\n", premsg, errmsg);
            if (!options.interactive) {
                sprintf(msg, "tried (line %i): '%s' ; failed (code -%#x): '%s'\n",
                        line_count, line, -err, errmsg);
                maslog_print(options.logger, msg);
                done = 1;
            }
        }
    }

    if (!options.nonzero_only)
        printf("Processed %i lines, exiting.\n", line_count);

exit_now:
    // Clean up acq!
    if (acq != NULL)
        mcedata_acq_destroy(acq);

    if (line_buffer!=NULL) free(line_buffer);

    mcelib_destroy(mce);

    return (err>=0) ? 0 : 1;
}

#define FILL_MENU(m, _name, min, max, data, opts ) \
    m.name = _name; \
m.min_args = min; \
m.max_args = max; \
m.flags = MASCMDTREE_SELECT | MASCMDTREE_NOCASE; \
m.sub_opts = opts; \
m.user_cargo = (unsigned long)data;


int menuify_mceconfig(mascmdtree_opt_t *opts)
{
    mascmdtree_opt_t *card_opts;
    mascmdtree_opt_t *para_opts;
    char *string_table;
    int i,j, error;
    int n_cards = mceconfig_card_count(mce);
    int n_params = 0;

    // Count parameters
    for (i=0; i<n_cards; i++) {
        card_t card;
        if (mceconfig_card(mce, i, &card)) {
            fprintf(stderr, "Problem loading card data at index %i\n", i);
            return -1;
        }

        error = mceconfig_card_paramcount(mce, &card);
        if (error < 0) {
            fprintf(stderr, "Problem counting parameters of '%s'\n", card.name);
            return -1;
        }
        n_params += error;
    }

    string_table = malloc((n_params+n_cards)*MCE_SHORT);
    card_opts = malloc((n_params + 3 * n_cards + 2)*sizeof(*card_opts));
    para_opts = card_opts + n_cards + 2;

    for (i=0; i<n_cards; i++) {
        card_t card;
        param_t p;
        int count;

        if (mceconfig_card(mce, i, &card)) {
            fprintf(stderr, "Problem loading card data at index %i\n", i);
            return -1;
        }

        // Fill out menu entry for card
        FILL_MENU( card_opts[i], string_table, 1, -1, card.cfg, para_opts );
        strcpy(string_table, card.name);
        string_table += strlen(string_table) + 1;

        count = mceconfig_card_paramcount(mce, &card);
        for (j=0; j<count; j++) {
            mceconfig_card_param(mce, &card, j, &p);
            FILL_MENU( (*para_opts), string_table, 0, -1, p.cfg, integer_opts);
            para_opts++;
            strcpy(string_table, p.name);
            string_table += strlen(string_table) + 1;
        }

        memcpy(para_opts, integer_opts, sizeof(integer_opts));
        para_opts += 2;
    }

    memcpy(card_opts+n_cards, integer_opts, sizeof(integer_opts));

    for (i = 0; (opts[i].flags & MASCMDTREE_TYPE_MASK) != MASCMDTREE_TERMINATOR;
            i++)
    {
        if (opts[i].sub_opts == command_placeholder_opts)
            opts[i].sub_opts = card_opts;
    }

    return 0;
}

int translate_card_string(char *s, char *errmsg)
{
    mascmdtree_token_t rc_token;
    if (mascmdtree_tokenize(&rc_token, s, 1) != 0) {
        sprintf(errmsg, "invalid readout specification string\n");
        return -1;
    }

    if (mascmdtree_select(&rc_token, rc_list, errmsg) <= 0)
        return -1;

    return rc_token.value;
}

int bit_count(int k)
{
    int i = 32;
    int count = 0;
    while (i-- > 0) {
        count += (k&1);
        k = k >> 1;
    }
    return count;
}

int prepare_outfile(char *errmsg, int storage_option)
{
    int error;
    mcedata_storage_t* storage;

    // Cleanup last acq
    if (acq != NULL) {
        mcedata_acq_destroy(acq);
        acq = NULL;
    }

    acq = (mce_acq_t*)malloc(sizeof(mce_acq_t));
    if (acq == NULL) {
        sprintf(errmsg, "Failed to allocate memory for mce_acq_t structure!\n");
        return -1;
    }

    // Setup storage-specific handler
    switch(storage_option) {

        case SPECIAL_ACQ_CONFIG:
            storage = mcedata_flatfile_create(options.acq_filename);
            if (storage == NULL) {
                sprintf(errmsg, "Could not create flatfile");
                return -1;
            }
            break;

        case SPECIAL_ACQ_CONFIG_FS:
            storage = mcedata_fileseq_create(options.acq_filename,
                    options.acq_interval,
                    FS_DIGITS);
            if (storage == NULL) {
                sprintf(errmsg, "Could not set up file sequencer");
                return -1;
            }
            break;

        case SPECIAL_ACQ_CONFIG_DIRFILE:
            storage = mcedata_dirfile_create(options.acq_filename, 0);
            if (storage == NULL) {
                sprintf(errmsg, "Could not create flatfile");
                return -1;
            }
            break;

        case SPECIAL_ACQ_CONFIG_DIRFILESEQ:
            storage = mcedata_dirfileseq_create(options.acq_filename,
                    options.acq_interval,
                    FS_DIGITS, 0);
            if (storage == NULL) {
                sprintf(errmsg, "Could not create flatfile");
                return -1;
            }
            break;

        default:
            sprintf(errmsg, "Unimplemented storage type.");
            return -1;
    }

    // Initialize the acquisition system
    if ((error=mcedata_acq_create(acq, mce, 0, options.acq_cards, -1, storage)) != 0) {
        sprintf(errmsg, "Could not configure acquisition: %s",
                mcelib_error_string(error));
        return -1;
    }

    return 0;
}

int data_string(char* dest, const u32 *buf, int count, const mce_param_t *p)
{
    int i;
    int offset = 0;
    int hex = (options.display == SPECIAL_HEX ||
            ( options.display == SPECIAL_DEF &&
              p->param.flags & MCE_PARAM_HEX ) );

    for (i=0; i<count ; i++) {
        if (hex) offset += sprintf(dest + offset, "%#x ", buf[i]);
        else     offset += sprintf(dest + offset, "%i ", buf[i]);
    }
    return offset;
}



int process_command(mascmdtree_opt_t *opts, mascmdtree_token_t *tokens,
        char *errmsg)
{
    int ret_val = 0;
    int err = 0;
    int i;
    mce_param_t mcep;
    int to_read, to_request, to_write, card_mul, index;
    u32 buf[DATA_SIZE];
    char s[LINE_LEN];

    errmsg[0] = 0;

    int is_command = (tokens[0].value >= ENUM_COMMAND_LOW &&
            tokens[0].value < ENUM_COMMAND_HIGH);

    if (is_command) {

        // Token[0] is the command (RB, WB, etc.)
        // Token[1] is the card
        // Token[2] is the param
        // Token[3+] are the data

        // Allow integer values for card and para.
        int raw_mode = (tokens[1].type != MASCMDTREE_SELECT) ||
            (tokens[2].type != MASCMDTREE_SELECT);
        card_mul = 1;
        to_read = 0;

        if (!raw_mode) {
            char card_word[MCE_SHORT];
            char para_word[MCE_SHORT];
            mascmdtree_token_word(card_word, tokens+1);
            mascmdtree_token_word(para_word, tokens+2);
            if (mcecmd_load_param(mce, &mcep, card_word, para_word)) {
                sprintf(errmsg, "Could not load parameter '%s %s'",
                        card_word, para_word);
                return -1;
            }
        } else {
            if ( tokens[1].type == MASCMDTREE_SELECT ) {
                mceconfig_cfg_card ((config_setting_t*)tokens[1].data,
                        &mcep.card);
            } else {
                mcep.card.id[0] = tokens[1].value;
                mcep.card.card_count = 1;
            }
            mcep.param.id = tokens[2].value;
            mcep.param.count = tokens[3].value;
            mcep.card.card_count =
                ( tokens[4].type == MASCMDTREE_INTEGER  ?
                  tokens[4].value : 1 );
        }

        if (to_read == 0 && tokens[3].type == MASCMDTREE_INTEGER) {
            to_read = tokens[3].value;
            if (tokens[4].type == MASCMDTREE_INTEGER) {
                card_mul = tokens[4].value;
            }
        }

        switch( tokens[0].value ) {

            case COMMAND_RS:
                err = mcecmd_reset(mce, &mcep);
                break;

            case COMMAND_GO:

                // If you get here, your data just accumulates
                //  in the driver's buffer, /dev/mce_data0, assuming that
                //  the frame size has been set correctly.
                err = mcecmd_start_application(mce, &mcep);
                break;

            case COMMAND_ST:
                err = mcecmd_stop_application(mce, &mcep);
                break;

            case COMMAND_RB:
                to_request = -1;
                to_read = mcecmd_read_size(&mcep, to_request);
                err = mcecmd_read_block(mce, &mcep, to_request, buf);
                if (err) break;

                errmsg += data_string(errmsg, buf, to_read, &mcep);

                break;

            case COMMAND_REL:
                to_request = -1;
                to_read = mcecmd_read_size(&mcep, to_request);
                err = mcecmd_read_element(mce, &mcep,
                        tokens[3].value, buf);
                if (err) break;

                errmsg += data_string(errmsg, buf, to_read, &mcep);

                break;

            case COMMAND_RRA:
                index = tokens[3].value;
                to_request = tokens[4].value;
                to_read = mcecmd_read_size(&mcep, to_request);
                err = mcecmd_read_range(mce, &mcep, index, buf, to_request);
                if (err) break;

                errmsg += data_string(errmsg, buf, to_read, &mcep);

                break;

            case COMMAND_WB:
                to_write = tokens[3].n;
                for (i=0; i<tokens[3].n && i<NARGS; i++)
                    buf[i] = tokens[3+i].value;

                err = mcecmd_write_block(mce, &mcep, to_write, buf);
                break;

            case COMMAND_WEL:
                err = mcecmd_write_element(mce, &mcep,
                        tokens[3].value,
                        tokens[4].value);
                break;

            case COMMAND_WRA:
                index = tokens[3].value;
                to_write = tokens[4].n;
                for (i=0; i<to_write && i<NARGS; i++)
                    buf[i] = tokens[4+i].value;

                err = mcecmd_write_range(mce, &mcep, index, buf, to_write);
                break;

            default:
                sprintf(errmsg, "command not implemented");
                return -1;
        }

        if (err!=0 && errmsg[0] == 0) {
            sprintf(errmsg, "mce library error -%#08x : %s", -err,
                    mcelib_error_string(err));
            ret_val = -1;
        }
    } else {

        switch(tokens[0].value) {
            case SPECIAL_HELP:
                /*              mascmdtree_list(errmsg, root_opts, */
                /*                      "MCE commands: [ ", " ", "]"); */
                break;

            case SPECIAL_ACQ:
                options.acq_frames = tokens[1].value;
                if ((err=mcedata_acq_go(acq, options.acq_frames)) != 0) {
                    sprintf(errmsg, "%s\n", mcelib_error_string(err));
                    ret_val = -1;
                }
                break;

            case SPECIAL_ACQ_CONFIG:
                /* Args: filename, card */

                /* Assemble file name using any path override */
                mascmdtree_token_word( s, tokens+1 );
                pathify_filename(options.acq_filename, s);

                /* Decode card name */
                mascmdtree_token_word( s, tokens+2 );
                options.acq_cards = translate_card_string(s, errmsg);
                if (options.acq_cards < 0) {
                    ret_val = -1;
                    break;
                }

                ret_val = prepare_outfile(errmsg, SPECIAL_ACQ_CONFIG);
                break;

            case SPECIAL_ACQ_CONFIG_FS:
                /* Args: filename, card, interval */

                /* Assemble file name using any path override */
                mascmdtree_token_word( s, tokens+1 );
                pathify_filename(options.acq_filename, s);

                /* Decode card name */
                mascmdtree_token_word( s, tokens+2 );
                options.acq_cards = translate_card_string(s, errmsg);
                if (options.acq_cards < 0) {
                    ret_val = -1;
                    break;
                }

                /* Store acquisition interval */
                options.acq_interval = tokens[3].value;

                ret_val = prepare_outfile(errmsg, SPECIAL_ACQ_CONFIG_FS);
                break;

            case SPECIAL_ACQ_CONFIG_DIRFILE:
            case SPECIAL_ACQ_CONFIG_DIRFILESEQ:
                /* Args: filename, card[, interval ] */

                /* Assemble file name using any path override */
                mascmdtree_token_word( s, tokens+1 );
                pathify_filename(options.acq_filename, s);

                /* Decode card name */
                mascmdtree_token_word( s, tokens+2 );
                options.acq_cards = translate_card_string(s, errmsg);
                if (options.acq_cards < 0) {
                    ret_val = -1;
                    break;
                }

                if (tokens[0].value == SPECIAL_ACQ_CONFIG_DIRFILESEQ)
                    options.acq_interval = tokens[3].value;

                ret_val = prepare_outfile(errmsg, tokens[0].value);
                break;

            case SPECIAL_ACQ_FLUSH:
                if (acq->storage->flush != NULL) {
                    acq->storage->flush(acq);
                }
                break;

            case SPECIAL_ACQ_PATH:
                mascmdtree_token_word( options.acq_path, tokens+1 );
                if (options.acq_path[0] != 0 &&
                        options.acq_path[strlen(options.acq_path)-1] != '/') {
                    strcat(options.acq_path, "/");
                }
                break;

            case SPECIAL_LOCK_QUERY:
                errmsg += sprintf(errmsg, "%i", mcedata_lock_query(mce));
                break;

            case SPECIAL_LOCK_RESET:
                ret_val = mcedata_lock_reset(mce);
                break;

            case SPECIAL_LOCK_DOWN:
                ret_val = mcedata_lock_down(mce);
                if (ret_val != 0) {
                    errmsg += sprintf(errmsg, "could not get data lock.\n");
                }
                break;

            case SPECIAL_LOCK_UP:
                ret_val = mcedata_lock_up(mce);
                break;

            case SPECIAL_QT_ENABLE:
                ret_val = mcedata_qt_enable(mce, tokens[1].value);
                break;

            case SPECIAL_QT_CONFIG:
                ret_val = mcedata_qt_setup(mce, tokens[1].value);
                break;

            case SPECIAL_MRESET:
                ret_val = mcecmd_hardware_reset(mce);
                break;

            case SPECIAL_DRESET:
                ret_val = mcecmd_interface_reset(mce);
                break;

            case SPECIAL_REPLY_LOCK:
                ret_val = mcecmd_lock_replies(mce, 1);
                break;

            case SPECIAL_REPLY_UNLOCK:
                ret_val = mcecmd_lock_replies(mce, 0);
                break;

                /*          case SPECIAL_CLEAR: */
                /*              ret_val = mcecmd_reset(mce, tokens[1].value, tokens[2].value);*/
                /*              break; */

            case SPECIAL_FAKESTOP:
                ret_val = mcedata_fake_stopframe(mce);
                break;

            case SPECIAL_EMPTY:
                ret_val = mcedata_empty_data(mce);
                break;

            case SPECIAL_SLEEP:
                usleep(tokens[1].value);
                break;

            case SPECIAL_COMMENT:
                break;

            case SPECIAL_FRAME:
                ret_val = mcedata_set_datasize(mce, tokens[1].value);
                if (ret_val != 0) {
                    sprintf(errmsg, "mce_library error %i", ret_val);
                }
                break;

            case SPECIAL_DISPLAY:
                options.display = tokens[1].value;
                break;

            case SPECIAL_ECHO:
                options.echo = tokens[1].value;
                break;

            default:
                sprintf(errmsg, "command not implemented");
                ret_val = -1;
        }
    }

    return ret_val;
}


int get_int(char *card, int *card_id)
{
    char *end = card;
    if (end==NULL || *end==0) return -1;
    *card_id = strtol(card, &end, 0);
    if (*end!=0) return -1;
    return 0;
}

int pathify_filename(char *dest, const char *src)
{
    char len = strlen(options.acq_path);

    // Initial '/' roots it...
    if (src[0] == '/' || len == 0) {
        strcpy(dest, src);
        return 0;
    }

    // Otherwise, concat.
    strcpy(dest, options.acq_path);

    if (dest[len-1] != '/')
        strcat(dest, "/");
    strcat(dest, src);

    return 0;
}

void die(int sig)
{
    // If we're accepting input, just cleanup and exit.
    if (input_switch) {
        // Clean up acq!
        if (acq != NULL)
            mcedata_acq_destroy(acq);
        exit(0);
    } else {
        switch (kill_switch++) {
            case 1:
                fprintf(stderr, "Your eagerness to exit has been noted. "
                        "Press Ctrl-C again to force quit.\n");
                break;
            case 2:
                fprintf(stderr, "Killed inopportunely! mce_reset recommended!\n");
                exit(1);
        }
    }
}

