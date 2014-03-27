/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/******************************************************
  mcecmd.h - header file for MCE API, command module

  Matthew Hasselfield, 08.01.02

******************************************************/

#ifndef _MCECMD_H_
#define _MCECMD_H_

/*! \file mcecmd.h
 *
 *  \brief Main header file for command module.
 *
 *  Contains routines for managing MCE command/reply.  Works with
 *  config module to translate command strings and to ensure data is
 *  in bounds.
 *
 *  This module is intended to connect to an mce_cmd character driver
 *  device file.
 */

#include <stdint.h>

/*
  Function prototypes
*/


/* Connection management */

int mcecmd_open (mce_context_t *context);

int mcecmd_close(mce_context_t *context);

/* int mcecmd_lock_replies(mce_context_t *context, int lock); */


/* MCE user commands - these are as simple as they look */

int mcecmd_start_application(mce_context_t* context, const mce_param_t *param);

int mcecmd_stop_application (mce_context_t* context, const mce_param_t *param);

int mcecmd_reset            (mce_context_t* context, const mce_param_t *param);

int mcecmd_write_block      (mce_context_t* context, const mce_param_t *param,
        int count, uint32_t *data);

int mcecmd_read_block       (mce_context_t* context, const mce_param_t *param,
        int count, uint32_t *data);


/* MCE special commands - these provide additional logical support */

int mcecmd_write_element    (mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t datum);

int mcecmd_read_element     (mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t *datum);

int mcecmd_write_range      (mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t *data, int count);

int mcecmd_read_range       (mce_context_t* context, const mce_param_t *param,
        int data_index, uint32_t *data, int count);

int mcecmd_write_block_check(mce_context_t* context, const mce_param_t *param,
        int count, uint32_t *data, int checks);


/* Raw i/o routines; roll your own packets */

int mcecmd_send_command_now (mce_context_t* context, mce_command *cmd);

int mcecmd_read_reply_now   (mce_context_t* context, mce_reply *rep);

int mcecmd_send_command     (mce_context_t* context,
			     mce_command *cmd, mce_reply *rep);


/* MCE parameter lookup */

int mcecmd_load_param       (mce_context_t* context, mce_param_t *param,
			  const char *card_str, const char *param_str);

int mcecmd_read_size        (const mce_param_t *p, int count);


/* Interface (PCI card) control */

int mcecmd_interface_reset  (mce_context_t* context);   // reset PCI card
int mcecmd_hardware_reset   (mce_context_t* context);    // reset MCE


/*
  Static members... no context required
*/

/* Custom packet creation */

int mcecmd_load_command(mce_command *cmd, uint32_t command,
        uint32_t card_id, uint32_t para_id, 
        int count, int data_count, const uint32_t *data);

/* Packet examination */

uint32_t mcecmd_checksum( const uint32_t *data, int count );
uint32_t mcecmd_cmd_checksum( const mce_command *cmd );
int mcecmd_cmd_match_rep( const mce_command *cmd, const mce_reply *rep );


#endif
