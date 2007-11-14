/***************************************************
  mcecmd.h - header file for MCE API

  Matthew Hasselfield, 07.06.11

***************************************************/

#ifndef _MCECMD_H_
#define _MCECMD_H_

/*! \file mcecmd.h
 *  \brief Header file for libmce
 *
 *  Projects linking to libmce should include this header file.
 */

/* mce.h defines the structures used by the dsp driver */

#include <mce.h>
#include <mce_errors.h>

/*! \def MCE_SHORT
 *  \brief Length of short strings.
 */

#define MCE_SHORT 32


/*! \def MCE_LONG
 *  \brief Length of long strings.
 */

#define MCE_LONG 1024


/*! \def MAX_CONS
 *  \brief Number of MCE handles available
 */

#define MAX_CONS 16


/*
   API: all functions return a negative error value on failure.  On
   success, the return value is 0 with the exception of the 4
   dsp_read_word* commands, which return the 24 bit data found at the
   specified address.

   Begin a session by calling dsp_open.  The return value is the
   handle that must be used in subsequent calls.  Connections should
   be closed when a session is finished.
*/

char *mce_error_string(int error);

int mce_open(char *dev_name);
int mce_close(int handle);


int mce_send_command_now(int handle, mce_command *cmd);
int mce_read_reply_now(int handle, mce_reply *rep);
int mce_send_command(int handle, mce_command *cmd, mce_reply *rep);


u32 calc_checksum( const mce_command *cmd );
u32 checksum_data( const u32 *data, int count );
int match_cmd_rep( const mce_command *cmd, const mce_reply *rep );


int mce_send_command_simple(int handle, int card_id, int para_id,
			    u32 cmd_code);

int mce_start_application(int handle, int card_id, int para_id);
int mce_stop_application(int handle, int card_id, int para_id);
int mce_reset(int handle, int card_id, int para_id);

int mce_write_block(int handle, int card_id, int para_id,
		    int n_data, const u32 *data);
int mce_read_block(int handle, int card_id, int para_id,
		   int n_data, u32 *data, int n_cards);


/* XML/config lookup facilities

int mce_param_init(param_properties_t *p);

int mce_free_config(int handle);
int mce_load_config(int handle, char *filename);
int mce_lookup(int handle, param_properties_t *props,
	       char *card_str, char *para_str, int para_index);
*/

#endif
