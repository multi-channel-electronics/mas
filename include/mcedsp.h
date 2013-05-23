/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _MCEDSP_H_
#define _MCEDSP_H_

/***************************************************
  mcedsp.h - header file for MCE SDSU card API

  Matthew Hasselfield, 07.06.05

***************************************************/

/* dsp.h defines the structures used by the dsp driver */

#include <mce_library.h>
#include <mce/new_dsp.h>


/* Maximum number of handles available */
/*
#define MAX_CONS 16
*/

/*
   API: all functions return a negative error value on failure.  On
   success, the return value is 0 with the exception of the 4
   dsp_read_word* commands, which return the 24 bit data found at the
   specified address.

   Begin a session by calling dsp_open.  The return value is the
   handle that must be used in subsequent calls.  Connections should
   be closed when a session is finished.
*/

int mcedsp_open(mce_context_t *context);
int mcedsp_close(mce_context_t *context);


/*
   The user may construct commands and issue them using dsp_command.
   Shortcuts for existing command are available.

   The memory type (dsp_memory_code) is defined in dsp.h.
*/

typedef enum {
    DSP_MEMP = 1,
    DSP_MEMX = 2,
    DSP_MEMY = 3
} dsp_memory_code;


/*
int mcedsp_send_command(mce_context_t *context, dsp_command *cmd);
*/

int mcedsp_read_word(mce_context_t *context, dsp_memory_code mem, int address);
int mcedsp_read_word_X(mce_context_t *context, int address);
int mcedsp_read_word_Y(mce_context_t *context, int address);
int mcedsp_read_word_P(mce_context_t *context, int address);

int mcedsp_write_word(mce_context_t *context, dsp_memory_code mem, int address,
        u32 value);
int mcedsp_write_word_X(mce_context_t *context, int address, u32 value);
int mcedsp_write_word_Y(mce_context_t *context, int address, u32 value);
int mcedsp_write_word_P(mce_context_t *context, int address, u32 value);

int mcedsp_version(mce_context_t *context);

int mcedsp_reset(mce_context_t *context);
/*
int mcedsp_start_application(mce_context_t *context, int data);

int mcedsp_stop_application(mce_context_t *context);
*/
int mcedsp_reset_mce(mce_context_t *context);
/*
int mcedsp_qt_set(mce_context_t *context, int var, int arg1, int arg2);

int mcedsp_ioctl(mce_context_t *context, unsigned int iocmd, unsigned long arg);
*/
int mcedsp_reset_flags(mce_context_t *context);

int mcedsp_error(mce_context_t *context);

int mcedsp_speak(mce_context_t *context, unsigned long arg);


int mcedsp_atomem(char *mem_type);

char *mcedsp_memtoa(int mem);

char *mcedsp_error_string(int error);


#endif
