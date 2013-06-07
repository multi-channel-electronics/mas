/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _MCE_LIBRARY_H_
#define _MCE_LIBRARY_H_

#include <mce/types.h>
#include <mce/mce_errors.h>


/*
  - First define mce_context_t, since some modules need to point to it.
  - Config module must preceed command module, which uses its types
  - Command module must preceed data, which can issue commands
*/

struct mce_context;
typedef struct mce_context mce_context_t;

#include <libmaslog.h>
#include <mceconfig.h>
#include <mcecmd.h>
#include <mcedata.h>

/* mcelib flags */
#define MCELIB_QUIET  0x1   /* suppress warning message */

/* Creation / destruction of context structure */
#define MCE_DEFAULT_MCE (-1)
#define MCE_NULL_MCE (-2)
mce_context_t* mcelib_create(int fibre_card, const char *mas_config,
        unsigned int flags);
void mcelib_destroy(mce_context_t* context);

/* terminal output redirection */
#define MCELIB_PRINT 0
#define MCELIB_WARN  1
#define MCELIB_ERR   2
void mcelib_set_termio(mce_context_t *context, int (*func)(int, const char*));

/* this behaves identically to mcelib_create above, but it sets the termio
 * function before doing anything interesting */
mce_context_t* mcelib_create_termio(int fibre_card, const char *mas_config,
        unsigned int flags, int (*func)(int, const char*));

/* Error look-up, versioning */

const char* mcelib_error_string(int error);
const char* mcelib_version(void);

#endif
