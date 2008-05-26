#ifndef _MCEDSP_H_
#define _MCEDSP_H_

/***************************************************
  mcedsp.h - header file for MCE SDSU card API

  Matthew Hasselfield, 07.06.05

***************************************************/

/* dsp.h defines the structures used by the dsp driver */

#include <dsp.h>


/* Maximum number of handles available */

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

int dsp_open(char *dev_name);
int dsp_close(int handle);


/*
   The user may construct commands and issue them using dsp_command.
   Shortcuts for existing command are available.

   The memory type (dsp_memory_code) is defined in dsp.h.
*/

int dsp_send_command(int handle, dsp_command *cmd);


int dsp_read_word(int handle, dsp_memory_code mem, int address);
int dsp_read_word_X(int handle, int address);
int dsp_read_word_Y(int handle, int address);
int dsp_read_word_P(int handle, int address);

int dsp_write_word(int handle, dsp_memory_code mem, int address, u32 value);
int dsp_write_word_X(int handle, int address, u32 value);
int dsp_write_word_Y(int handle, int address, u32 value);
int dsp_write_word_P(int handle, int address, u32 value);

int dsp_version(int handle);

int dsp_reset(int handle);

int dsp_clear(int handle);

int dsp_start_application(int handle, int data);

int dsp_stop_application(int handle);

int dsp_reset_mce(int handle);

int dsp_qt_set(int handle, int var, int arg1, int arg2);

int dsp_ioctl(int handle, unsigned int iocmd, unsigned long arg);

int dsp_reset_flags(int handle);

int dsp_error(int handle);

int dsp_speak(int handle, unsigned long arg);


int dsp_atomem(char *mem_type);

char *dsp_memtoa(int mem);


char *dsp_error_string(int error);


#endif
