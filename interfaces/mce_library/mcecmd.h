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


/*! \def MAX_CONS
 *  \brief Number of MCE handles available
 */

#define MAX_CONS 16


/*! \def MCE_PARAM_*
 *
 * \brief Type of MCE parameter.
 */

#define MCE_PARAM_INT 0
#define MCE_PARAM_CMD 1
#define MCE_PARAM_RST 2

/*! \struct param_properties_t;
 *
 *  \brief MCE parameter properties.
 *
 *  This structure contains information that helps in sending data to
 *  a particular MCE parameter location.  The card_id and para_id are
 *  the internal memory addresses for the data.  The Other fields
 *  describe the type of data and limits that software should enforce.
 *
 *  The flags member contains 
 */



/*! @name PARAM defines
 *
 * PARAM_* are used for param_properties_t::flags
 */

/*! @{ */
#define PARAM_MIN    0x0001 /** Parameter has minimum */
#define PARAM_MAX    0x0002 /** Parameter has maximum */
#define PARAM_DEF    0x0004
#define PARAM_PARAM  0x0010
#define PARAM_CARD   0x0020
/*! @} */

typedef struct {

	int card_id;
	int para_id;	

	int count;
	int card_count;

	int flags;

	int minimum;
	int maximum;
	int def_val;

} param_properties_t;


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

/* Ioctl related - note argument is fd, not handle (fixme) */

int mce_ioctl(int fd, int key, unsigned long arg);
int mce_set_datasize(int fd, int datasize);
int mce_empty_data(int fd);
int mce_fake_stopframe(int fd);
int mce_qt_setup(int fd, int frame_index);
int mce_qt_enable(int fd, int on);

/* XML/config lookup facilities */

int mce_param_init(param_properties_t *p);

int mce_free_config(int handle);
int mce_load_config(int handle, char *filename);
int mce_lookup(int handle, param_properties_t *props,
	       char *card_str, char *para_str, int para_index);

#endif
