/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/* Helper routines for paths &c. */
#ifndef _MCE_DEFAULTS_H_
#define _MCE_DEFAULTS_H_

#include <mce_library.h>

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const mce_context_t context, const char* input);

/* look up a canonical directory path.  Typically used by mce_script, this
 * will check the environment, then consult mas.cfg, then pick a possibly
 * sensible default.
 */
#define MAS_DIR_ETC          0
#define MAS_DIR_BIN          1
#define MAS_DIR_DATA         2
#define MAS_DIR_DATA_ROOT    3
#define MAS_DIR_IDL          4
#define MAS_DIR_PYTHON       5
#define MAS_DIR_ROOT         6
#define MAS_DIR_SCRIPT       7
#define MAS_DIR_TEMP         8
#define MAS_DIR_TEMPLATE     9
#define MAS_DIR_TEST_SUITE  10
const char *mcelib_lookup_dir(const mce_context_t context, int index);

int mcelib_default_mce(void);

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(const mce_context_t context);
char *mcelib_default_hardwarefile(const mce_context_t context);
char *mcelib_default_masfile(void);

#endif
