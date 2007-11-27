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

#define MCE_SHORT 32
#define MCE_LONG 1024

#include <mce.h>
#include <mceconfig.h>
#include <mce_errors.h>


/*! \def MAX_CONS
 *  \brief Number of MCE handles available
 */

#define MAX_CONS 16


/*
   API: all functions return a negative error value on failure.  On
   success, the return value is 0.

   Begin a session by calling mce_open.  The return value is the
   handle that must be used in subsequent calls.  Connections should
   be closed when a session is finished.
*/

int mce_open(char *dev_name);
int mce_close(int handle);

int mce_set_config(int handle, const mceconfig_t *config);

/* MCE user commands - these are as simple as they look */

int mce_start_application(int handle, const mce_param_t *param);
int mce_stop_application(int handle, const mce_param_t *param);
int mce_reset(int handle, const mce_param_t *param);

int mce_write_block(int handle, const mce_param_t *param,
		    int count, const u32 *data);
int mce_read_block(int handle, const mce_param_t *param,
		   int count, u32 *data);


/* MCE special commands - these provide additional logical support */

int mce_write_element(int handle, const mce_param_t *param,
		      int data_index, u32 datum);

int mce_read_element(int handle, const mce_param_t *param,
		     int data_index, u32 *datum);

int mce_write_block_check(int handle, const mce_param_t *param,
			  int count, const u32 *data, int checks);


/* Raw i/o routines; roll your own packets */

int mce_send_command_now(int handle, mce_command *cmd);
int mce_read_reply_now(int handle, mce_reply *rep);
int mce_send_command(int handle, mce_command *cmd, mce_reply *rep);


/* MCE parameter lookup */

int mce_load_param(int handle, mce_param_t *param,
		   const char *card_str, const char *param_str);


/* Custom packet creation and sending */

int mce_load_command(mce_command *cmd, u32 command,
		     u32 card_id, u32 para_id, 
		     int count, const u32 *data);


/* Useful things... */

u32 mce_checksum( const u32 *data, int count );
u32 mce_cmd_checksum( const mce_command *cmd );
int mce_cmd_match_rep( const mce_command *cmd, const mce_reply *rep );


/* Perhaps you are a human */

char *mce_error_string(int error);



#endif
