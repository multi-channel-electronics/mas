/* Helper routines for paths &c. */
#ifndef _MCE_DEFAULTS_H_
#define _MCE_DEFAULTS_H_

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const char* input);

int mcelib_default_fibre_card(void);

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(int card);
char *mcelib_default_hardwarefile(int card);
char *mcelib_default_masfile(void);

/* the same with device node names */
char *mcelib_cmd_device(int card);
char *mcelib_data_device(int card);
char *mcelib_dsp_device(int card);
#endif
