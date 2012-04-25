/* Helper routines for paths &c. */
#ifndef _MCE_DEFAULTS_H_
#define _MCE_DEFAULTS_H_

#include <mce_library.h>

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const mce_context_t *context, const char* input);

int mcelib_default_mce(void);

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(const mce_context_t *context);
char *mcelib_default_hardwarefile(const mce_context_t *context);
char *mcelib_default_masfile(void);

#endif
