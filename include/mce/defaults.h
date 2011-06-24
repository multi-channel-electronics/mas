/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/* Helper routines for paths &c. */
#ifndef _MCE_DEFAULTS_H_
#define _MCE_DEFAULTS_H_

#include <mce_library.h>

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error; to use the default MCE, pass dev=-1 */
char *mcelib_shell_expand(const char* input, int dev);

int mcelib_default_mce(void);

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(int dev_index);
char *mcelib_default_hardwarefile(int dev_index);
char *mcelib_default_masfile(void);

#endif
