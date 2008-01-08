/*! \file mce_cmd.c
 *
 *  \brief Program to send commands to the MCE.
 *
 *  This program uses mce_library to send commands and receive
 *  responses from the MCE.  The program accepts instructions from
 *  standard input or from the command line using the -x option.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <mce_library.h>

#include <cmdtree.h>

#include "cmd.h"
#include "options.h"

enum {
	ENUM_COMMAND_LOW,	
	COMMAND_RB,
	COMMAND_WB,
	COMMAND_REL,
	COMMAND_WEL,
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
	SPECIAL_ACQ_FLUSH,
	SPECIAL_QT_CONFIG,
	SPECIAL_QT_ENABLE,
	SPECIAL_MRESET,
	SPECIAL_DRESET,
	SPECIAL_CLEAR,
	SPECIAL_FAKESTOP,
	SPECIAL_EMPTY,
	SPECIAL_SLEEP,
	SPECIAL_COMMENT,
	SPECIAL_FRAME,
	SPECIAL_DEF,
	SPECIAL_DEC,
	SPECIAL_HEX,
	SPECIAL_ECHO,
	ENUM_SPECIAL_HIGH,
};   


cmdtree_opt_t anything_opts[] = {
	{ CMDTREE_INTEGER, "", 0, -1, 0, anything_opts },
	{ CMDTREE_STRING , "", 0, -1, 0, anything_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t integer_opts[] = {
	{ CMDTREE_INTEGER   , "", 0, -1, 0, integer_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t string_opts[] = {
	{ CMDTREE_STRING    , "", 0, -1, 0, string_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t command_placeholder_opts[] = {
	{ CMDTREE_INTEGER   , "", 0, -1, 0, integer_opts },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};


cmdtree_opt_t flat_args[] = {
	{ CMDTREE_STRING | CMDTREE_ARGS, "filename", 0, -1, 0, flat_args+1 },
	{ CMDTREE_STRING | CMDTREE_ARGS, "card"    , 0, -1, 0, NULL },
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};


cmdtree_opt_t fs_args[] = {
	{ CMDTREE_STRING | CMDTREE_ARGS, "filename", 0, -1, 0, fs_args+1 },
	{ CMDTREE_STRING | CMDTREE_ARGS, "card"    , 0, -1, 0, fs_args+2 },
	{ CMDTREE_INTEGER| CMDTREE_ARGS, "interval", 0, -1, 0, NULL},
	{ CMDTREE_TERMINATOR, "", 0, 0, 0, NULL},
};

cmdtree_opt_t root_opts[] = {
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "RB"      , 2, 3, COMMAND_RB, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "WB"      , 3,-1, COMMAND_WB, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "r"       , 2, 3, COMMAND_RB, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "w"       , 3,-1, COMMAND_WB, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "REL"     , 3, 3, COMMAND_REL, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "WEL"     , 4, 4, COMMAND_WEL, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "GO"      , 2,-1, COMMAND_GO, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "STOP"    , 2,-1, COMMAND_ST, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "RS"      , 2,-1, COMMAND_RS, command_placeholder_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "MCE_RESET", 0,0, SPECIAL_MRESET, NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "DSP_RESET", 0,0, SPECIAL_DRESET, NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "HELP"    , 0, 0, SPECIAL_HELP    , NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ACQ_GO"  , 1, 1, SPECIAL_ACQ     , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ACQ_CONFIG", 2, 2, SPECIAL_ACQ_CONFIG, flat_args},
 	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ACQ_CONFIG_FS", 3, 3, SPECIAL_ACQ_CONFIG_FS, fs_args},
 	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ACQ_PATH" , 1, 1, SPECIAL_ACQ_PATH , string_opts},
 	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ACQ_FLUSH", 0, 0, SPECIAL_ACQ_FLUSH, NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "QT_ENABLE", 1, 1, SPECIAL_QT_ENABLE, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "QT_CONFIG", 1, 1, SPECIAL_QT_CONFIG, integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "FAKESTOP", 0, 0, SPECIAL_FAKESTOP, NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "EMPTY"   , 0, 0, SPECIAL_EMPTY   , NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "SLEEP"   , 1, 1, SPECIAL_SLEEP   , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "FRAME"   , 1, 1, SPECIAL_FRAME   , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "DEF"     , 0, 0, SPECIAL_DEF     , NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "DEC"     , 0, 0, SPECIAL_DEC     , NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "HEX"     , 0, 0, SPECIAL_HEX     , NULL},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "ECHO"    , 1, 1, SPECIAL_ECHO    , integer_opts},
	{ CMDTREE_SELECT | CMDTREE_NOCASE, "#"       , 0,-1, SPECIAL_COMMENT , anything_opts},
	{ CMDTREE_TERMINATOR, "", 0,0,0, NULL},
};
	

// Lazy old globals...

mce_context_t* mce;

char *line;
char errstr[LINE_LEN];

options_t options = {
	cmd_device: DEFAULT_DEVICE,
	data_device: DEFAULT_DATA,
	config_file: DEFAULT_XML,
	display: SPECIAL_DEF,
};

mce_acq_t acq;

// This structure is used to cache data which eventually constructs acq.

struct my_acq_struct my_acq;

mce_param_t ret_dat_s;
mce_param_t num_rows_reported;

int  preload_mce_params();

int  bit_count(int k);

int  menuify_mceconfig(cmdtree_opt_t *opts);

int  process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg);

int  main(int argc, char **argv)
{
	FILE *ferr = stderr;
	FILE *fin  = stdin;
	int err = 0;

	if (process_options(&options, argc, argv)) {
		err = ERR_OPT;
		goto exit_now;
	}

	if (!options.nonzero_only) {
		printf("This is %s version %s\n",
		       PROGRAM_NAME, VERSION_STRING);
	}

	line = (char*) malloc(LINE_LEN);
	if (line==NULL) {
		fprintf(ferr, "memory error!\n");
		err = ERR_MEM;
		goto exit_now;
	}

	if ((mce = mcelib_create()) == NULL) {
		fprintf(ferr, "Could not create mce library context.\n");
		err = ERR_MEM;
		goto exit_now;
	}

	// Connect command, data, and configuration modules.

	if (mcecmd_open(mce, options.cmd_device) != 0) {
		fprintf(ferr, "Could not open mce device '%s'\n",
			options.cmd_device);
		err = ERR_MCE;
		goto exit_now;
	}

	if (mcedata_open(mce, options.data_device) != 0) {
		fprintf(ferr, "Could not open data device '%s'.\n",
			options.data_device);
		err = ERR_MCE;
		goto exit_now;
	}
	
	if (mceconfig_open(mce, options.config_file, NULL)!=0) {
		fprintf(ferr, "Could not load MCE config file '%s'.\n",
			options.config_file);
		err = ERR_MCE;
		goto exit_now;
	}

	menuify_mceconfig(root_opts);

	// Preload useful MCE parameter id's
	if (preload_mce_params()) {
		fprintf(ferr, "Could not pre-load useful MCE parameter id's.\n");
		err = ERR_MCE;
		goto exit_now;
	}
	
	//Open batch file, if given
	if (options.batch_now) {
		fin = fopen(options.batch_file, "r");
		if (fin==NULL) {
			fprintf(ferr, "Could not open batch file '%s'\n",
				options.batch_file);
			err = ERR_MCE;
			goto exit_now;
		}
	}
				

	char errmsg[1024] = "";
	char premsg[1024] = "";
	int line_count = 0;
	int done = 0;

	while (!done) {

		unsigned int n = LINE_LEN;

		if ( options.cmd_now ) {
			strcpy(line, options.cmd_command);
			done = 1;
		} else {

			getline(&line, &n, fin);
			if (n==0 || feof(fin)) break;

			n = strlen(line);
			if (line[n-1]=='\n') line[--n]=0;
			line_count++;
		}

		if (options.no_prefix)
			premsg[0] = 0;
		else
			sprintf(premsg, "Line %3i : ", line_count);

		if (options.echo) {
			printf("Cmd  %3i : %s\n", line_count, line);
		}

		errmsg[0] = 0;

		cmdtree_token_t args[NARGS];
		args[0].n = 0;
		int err = 0;

		cmdtree_debug = 0;

		err = cmdtree_tokenize(args, line, NARGS);
		if (err) {
			strcpy(errmsg, "could not tokenize");
		}

		if (!err) {
			int count = cmdtree_select( args, root_opts, errmsg);
			
			if (count < 0) {
				err = -1;
			} else if (count == 0) {
				if (options.interactive || args->n > 0) {
					cmdtree_list(errmsg, root_opts,
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
			if (options.interactive)
				continue;
			return 1;
		}
	}

	if (!options.nonzero_only)
		printf("Processed %i lines, exiting.\n", line_count);

exit_now:
	if (line!=NULL) free(line);

	mcelib_destroy(mce);

	return err;
}

int menuify_mceconfig(cmdtree_opt_t *opts)
{
	cmdtree_opt_t *card_opts;
	cmdtree_opt_t *para_opts;
	char *string_table;
	int i,j;
	int n_cards = mceconfig_card_count(mce);
	int n_params = 0;
	
	// Count parameters
	for (i=0; i<n_cards; i++) {
		card_t card;
		cardtype_t ct;
		paramset_t ps;
		if (mceconfig_card(mce, i, &card)) {
			fprintf(stderr, "Problem loading card data at index %i\n", i);
			return -1;
		}
		if (mceconfig_card_cardtype(mce, &card, &ct)) {
			fprintf(stderr, "Problem loading cardtype data for '%s'\n", card.name);
			return -1;
		}
		for (j=0; j<ct.paramset_count; j++) {
			mceconfig_cardtype_paramset(mce, &ct, j, &ps);
			n_params += ps.param_count;
		}
	}
	
	string_table = malloc((n_params+n_cards)*MCE_SHORT);
	card_opts = malloc((n_params + 3 * n_cards + 2)*sizeof(*card_opts));
	para_opts = card_opts + n_cards + 2;
		
	for (i=0; i<n_cards; i++) {
		card_t card;
		cardtype_t ct;
		if (mceconfig_card(mce, i, &card)) {
			fprintf(stderr, "Problem loading card data at index %i\n", i);
			return -1;
		}
		if (mceconfig_card_cardtype(mce, &card, &ct)) {
			fprintf(stderr, "Problem loading cardtype data for '%s'\n", card.name);
			return -1;
		}
		
		// Fill out menu entry for card
		card_opts[i].name = string_table;
 		strcpy(string_table, card.name);
		string_table += strlen(string_table) + 1;
		card_opts[i].min_args = 1;
		card_opts[i].max_args = -1;
		card_opts[i].flags = CMDTREE_SELECT | CMDTREE_NOCASE;
		card_opts[i].sub_opts = para_opts;
		card_opts[i].user_cargo = (unsigned long)card.cfg;


		int k;
		paramset_t ps;
		param_t p;

		for (j=0; j<ct.paramset_count; j++) {
			mceconfig_cardtype_paramset(mce, &ct, j, &ps);
			for (k=0; k<ps.param_count; k++) {
				mceconfig_paramset_param(mce, &ps, k, &p);
				strcpy(string_table, p.name);
				para_opts->name = string_table;
				string_table += strlen(string_table) + 1;
				para_opts->flags = CMDTREE_SELECT | CMDTREE_NOCASE;
				para_opts->min_args = 0;
				para_opts->max_args = p.count;
				para_opts->sub_opts = integer_opts;
				para_opts->user_cargo = (unsigned long)p.cfg;
				para_opts++;
			}
		}

		memcpy(para_opts, integer_opts, sizeof(integer_opts));
		para_opts += 2;
	}

	memcpy(card_opts+n_cards, integer_opts, sizeof(integer_opts));

	for (i=0; (opts[i].flags & CMDTREE_TYPE_MASK) != CMDTREE_TERMINATOR; i++) {
		if (opts[i].sub_opts == command_placeholder_opts)
			opts[i].sub_opts = card_opts;
	}

	return 0;
}


int preload_mce_params()
{
	int ret_val = 0;
	if ((ret_val=mcecmd_load_param(mce, &num_rows_reported, "cc", "num_rows_reported"))!=0) {
		fprintf(stderr, "Could not decode 'cc num_rows_reported' [%i]\n",
			ret_val);
		return -1;
	}
	
	if ((ret_val=mcecmd_load_param(mce, &ret_dat_s,  "cc", "ret_dat_s"))!=0) {
		fprintf(stderr, "Could not decode 'cc ret_dat_s' [%i]\n", ret_val);
		return -1;
	}
	return 0;
}


int learn_acq_params(int get_frame_count, int get_rows)
{
	u32 data[64];

	if (get_frame_count) {
		if (mcecmd_read_block(mce, &ret_dat_s, -1, data)) {
			sprintf(errstr, "Failed to read frame count from MCE");
			return -1;
		}
		my_acq.n_frames = data[1]-data[0]+1;
	}

	if (get_rows) {
		if (mcecmd_read_block(mce, &num_rows_reported, -1, data)) {
			sprintf(errstr, "Failed to read number of reported rows");
			return -1;
		}
		my_acq.rows = data[0];
	}
	return 0;
}


int translate_card_string(char *s)
{	
	if (strcmp(s, "rc1")==0)
		return MCEDATA_RC1;
	else if (strcmp(s, "rc2")==0)
		return MCEDATA_RC2;
	else if (strcmp(s, "rc3")==0)
		return MCEDATA_RC3;
	else if (strcmp(s, "rc4")==0)
		return MCEDATA_RC4;
	else if (strcmp(s, "rcs")==0)
		return MCEDATA_RC1 | MCEDATA_RC2 | MCEDATA_RC3 | MCEDATA_RC4;
	return -1;
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

int prepare_outfile(char *errmsg, int file_sequencing)
{
	// Cleanup last acq
	if (acq.actions.cleanup!=NULL && acq.actions.cleanup(&acq)) {
		sprintf(errmsg, "Failed to clean up previous acquisition: %s",
			acq.errstr);
		return -1;
	}

	// Basic init, including framesize -> driver.
	if (mcedata_acq_setup(&acq, mce, 0, my_acq.cards, my_acq.rows) != 0) {
		sprintf(errmsg, "Could not configure acquisition");
		return -1;
	}

	// Output type-specific
	if (file_sequencing) {
		if (mcedata_fileseq_create(&acq, my_acq.filename,
					   my_acq.interval, FS_DIGITS)) {
			sprintf(errmsg, "Could not set up file sequencer");
			return -1;
		}
	} else {
		if (mcedata_flatfile_create(&acq, my_acq.filename) != 0) {
			sprintf(errmsg, "Could not create flatfile");
			return -1; 
		}
	}

	// Initialize this file type
	if (acq.actions.init!=NULL && acq.actions.init(&acq)) {
		sprintf(errmsg, "Failed to initialize output system: %s",
			acq.errstr);
		return -1;
	}
	return 0;
}

int do_acq_compat(char *errmsg)
{
	if (learn_acq_params(1, 1)!=0)
		return -1;
	
	if (mcedata_acq_setup(&acq, mce, 0, my_acq.cards, my_acq.rows) != 0) {
		sprintf(errmsg, "Could not setup acq structure.\n");
		return -1;
	}

	if (mcedata_flatfile_create(&acq, my_acq.filename) != 0) {
		sprintf(errmsg, "Could not create flatfile");
		return -1;
	}

	if (acq.actions.init!=NULL && acq.actions.init(&acq)) {
		sprintf(errmsg, "Failed to initialize acquisition: %s",
			acq.errstr);
		return -1;
	}

	if (mcedata_acq_go(&acq, my_acq.n_frames) != 0) {
		sprintf(errmsg, "Acqusition step failed");
		return -1;
	}

	return 0;
}





int process_command(cmdtree_opt_t *opts, cmdtree_token_t *tokens, char *errmsg)
{
	int ret_val = 0;
	int err = 0;
	int i;
	mce_param_t mcep;
	int to_read, to_write, card_mul;
	u32 buf[NARGS];
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
		int raw_mode = (tokens[1].type != CMDTREE_SELECT) ||
			(tokens[2].type != CMDTREE_SELECT);
		card_mul = 1;
		to_read = 0;
		to_write = tokens[3].n;

		if (!raw_mode) {
			char card_word[MCE_SHORT];
			char para_word[MCE_SHORT];
			cmdtree_token_word(card_word, tokens+1);
			cmdtree_token_word(para_word, tokens+2);
			if (mcecmd_load_param(mce, &mcep, card_word, para_word)) {
				sprintf(errstr, "Could not load parameter '%s %s'",
					card_word, para_word);
				return -1;
			}
		} else {
			if ( tokens[1].type == CMDTREE_SELECT ) {
				mceconfig_cfg_card ((config_setting_t*)tokens[1].value,
						    &mcep.card);
			} else {
				mcep.card.id[0] = tokens[1].value;
				mcep.card.card_count = 1;
			}
			mcep.param.id = tokens[2].value;
			mcep.param.count = tokens[3].value;
			mcep.card.card_count =
				( tokens[4].type == CMDTREE_INTEGER  ?
				  tokens[4].value : 1 );
		}

		if (to_read == 0 && tokens[3].type == CMDTREE_INTEGER) {
			to_read = tokens[3].value;
			if (tokens[4].type == CMDTREE_INTEGER) {
				card_mul = tokens[4].value;
			}
		}

		switch( tokens[0].value ) {
		
		case COMMAND_RS:
			err = mcecmd_reset(mce, &mcep);
			break;

		case COMMAND_GO:
			if (options.das_compatible) {
				cmdtree_token_word( s, tokens+1 );
				my_acq.cards = translate_card_string(s);
				if (my_acq.cards<0) {
					sprintf(errmsg, "Bad card name.\n");
					ret_val = -1;
					break;
				}

				// Get num_rows and n_frames
				if (learn_acq_params(1, 1)) {
					ret_val = -1;
					break;
				}

				ret_val = prepare_outfile(errmsg, 0);
				if (ret_val)
					break;

				ret_val = mcedata_acq_go(&acq, my_acq.n_frames);
				if (ret_val != 0) {
					sprintf(errmsg, "Acquisition failed.\n");
				}
				
			} else {
				// If you get here, your data just accumulates
				//  in the driver's buffer, /dev/mce_data0, assuming that
				//  the frame size has been set correctly.
				err = mcecmd_start_application(mce, &mcep);
			}
			break;

		case COMMAND_ST:
			err = mcecmd_stop_application(mce, &mcep);
			break;

		case COMMAND_RB:

			err = mcecmd_read_block(mce, &mcep, -1, buf);
			if (err)
				break;

			int hex = (options.display == SPECIAL_HEX || 
				   ( options.display == SPECIAL_DEF &&
				     mcep.param.flags & MCE_PARAM_HEX ) );

			for (i=0; i < mcep.param.count *
				     mcep.card.card_count ; i++) {
				if (hex)
					errmsg += sprintf(errmsg, "%#x ", buf[i]);
				else 
					errmsg += sprintf(errmsg, "%i ", buf[i]);
			}
			break;

		case COMMAND_REL:
			//Reject REL on multi-card parameters
			if (card_mul != 1) {
				sprintf(errstr, "REL not allowed on multi-card parameters!");
				return -1;
			}
			err = mcecmd_read_element(mce, &mcep,
					    tokens[3].value, buf);
			if (err)
				break;

			if (options.display == SPECIAL_HEX )
				errmsg += sprintf(errmsg, "%#x ", *buf);
			else 
				errmsg += sprintf(errmsg, "%i ", *buf);
			break;

		case COMMAND_WB:
			
			for (i=0; i<tokens[3].n && i<NARGS; i++) {
				buf[i] = tokens[3+i].value;
			}

			err = mcecmd_write_block(mce, &mcep, to_write, buf);
			break;
			
		case COMMAND_WEL:
			
			err = mcecmd_write_element(mce, &mcep,
						tokens[3].value,
						tokens[4].value);
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
/* 			cmdtree_list(errmsg, root_opts, */
/* 				     "MCE commands: [ ", " ", "]"); */
			break;

		case SPECIAL_ACQ:
			my_acq.n_frames = tokens[1].value;
			if (mcedata_acq_go(&acq, my_acq.n_frames) != 0) {
				sprintf(errmsg, "Acquisition failed.\n");
				ret_val = -1;
			}
			break;

		case SPECIAL_ACQ_CONFIG:
			/* Args: filename, card */

			strcpy(my_acq.filename, options.acq_path);
			cmdtree_token_word( my_acq.filename + strlen(my_acq.filename),
					    tokens+1 );

			cmdtree_token_word( s, tokens+2 );
			my_acq.cards = translate_card_string(s);
			if (my_acq.cards < 0) {
				sprintf(errmsg, "Bad card option '%s'", s);
				ret_val = -1;
			}

			// Get num_rows from MCE
			if (learn_acq_params(0, 1)) {
				ret_val = -1;
				break;
			}

			ret_val = prepare_outfile(errmsg, 0);
			break;

		case SPECIAL_ACQ_CONFIG_FS:
			/* Args: filename, card, interval */

			strcpy(my_acq.filename, options.acq_path);
			cmdtree_token_word( my_acq.filename + strlen(my_acq.filename)
					    , tokens+1 );

			cmdtree_token_word( s, tokens+2 );
			my_acq.cards = translate_card_string(s);
			if (my_acq.cards < 0) {
				sprintf(errmsg, "Bad card option '%s'", s);
				ret_val = -1;
			}

			// Get num_rows from MCE
			if (learn_acq_params(0, 1)) {
				ret_val = -1;
				break;
			}

			ret_val = prepare_outfile(errmsg, 1);
			break;

		case SPECIAL_ACQ_FLUSH:
			if (acq.actions.flush != NULL) {
				acq.actions.flush(&acq);
			}
			break;

		case SPECIAL_ACQ_PATH:
			cmdtree_token_word( options.acq_path, tokens+1 );
			if (options.acq_path[0] != 0 && 
			    options.acq_path[strlen(options.acq_path)-1] != '/') {
				strcat(options.acq_path, "/");
			}
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

/* 		case SPECIAL_CLEAR: */
/* 			ret_val = mcecmd_reset(mce, tokens[1].value, tokens[2].value); */
/* 			break; */

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

		case SPECIAL_DEC:
			options.display = SPECIAL_DEC;
			break;

		case SPECIAL_HEX:
			options.display = SPECIAL_HEX;
			break;

		case SPECIAL_DEF:
			options.display = SPECIAL_DEF;
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
