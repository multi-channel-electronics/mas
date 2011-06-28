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
typedef struct mce_context *mce_context_t;

#include <libmaslog.h>
#include <mceconfig.h>
#include <mcecmd.h>
#include <mcedata.h>

#define MCE_DEFAULT_MCE (-1)
/* Creation / destruction of context structure */
mce_context_t mcelib_create(int dev_num, const char *mas_config,
    unsigned char udepth);
void mcelib_destroy(mce_context_t context);

/* context data */
const char *mcelib_dev(mce_context_t context);
int mcelib_ndev(mce_context_t context);
struct config_t *mcelib_mascfg(mce_context_t context);
int mcelib_ddepth(mce_context_t context);

/* Error look-up, versioning */

char* mcelib_error_string(int error);
char* mcelib_version();

#endif
