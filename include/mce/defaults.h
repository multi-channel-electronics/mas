/* Helper routines for paths &c. */
#ifndef _MCE_DEFAULTS_H_
#define _MCE_DEFAULTS_H_

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error; to use the default fibre card, pass card=-1 */
char *mcelib_shell_expand(const char* input, int card);

int mcelib_default_fibre_card(void);

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(int card);
char *mcelib_default_hardwarefile(int card);
char *mcelib_default_masfile(void);

#endif
