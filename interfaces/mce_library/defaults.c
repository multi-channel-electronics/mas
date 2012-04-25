/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/* Helper routines for paths &c. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include "../../defaults/config.h"
#include "mce/defaults.h"
#include "context.h"

#define SET_MASDEFAULT(x) \
  setenv("MASDEFAULT_" #x, MASDEFAULT_ ## x, 0)

/* sanitise the environment */
static void check_env(const mce_context_t *context)
{
    char buffer[10];

    if (context == NULL)
        sprintf(buffer, "%i", mcelib_default_mce());
    else
        sprintf(buffer, "%i", context->fibre_card);
    setenv("MAS_MCE_DEV", buffer, 1);

    SET_MASDEFAULT(BIN);
    SET_MASDEFAULT(DATA);
    SET_MASDEFAULT(DATA_ROOT);
    SET_MASDEFAULT(IDL);
    SET_MASDEFAULT(PYTHON);
    SET_MASDEFAULT(SCRIPT);
    SET_MASDEFAULT(TEMP);
    SET_MASDEFAULT(TEMPLATE);
    SET_MASDEFAULT(TEST_SUITE);
}

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const mce_context_t *context, const char* input)
{
    char *ptr;
    static int depth = 0;
    wordexp_t expansion;

    /* avoid infinite recursion */
    if (depth > 100) {
        ptr = strdup(input);
        return ptr;
    } else
        depth++;

    check_env(context);

    /* shell expand the path, if necessary */
    if (wordexp(input, &expansion, 0) != 0) {
        /* path expansion failed */
        depth--;
        return NULL;
    }

    if (expansion.we_wordc > 1) {
        /* path expansion resulted in multiple values */
        depth--;
        return NULL;
    }

    ptr = strdup(expansion.we_wordv[0]);

    /* clean up */
    wordfree(&expansion);

    /* if there are still things to expand, run it through again */
    if (strcmp(ptr, input) && strchr(ptr, '$')) {
        char *old = ptr;
        ptr = mcelib_shell_expand(context, ptr);
        free(old);
    }

    depth--;
    return ptr;
}

int mcelib_default_mce(void)
{
#if !MULTICARD
    return 0;
#else
    const char *ptr = getenv("MAS_CARD");
    return (ptr == NULL) ?  DEFAULT_FIBRE_CARD : atoi(ptr);
#endif
}

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(const mce_context_t *context)
{
    return mcelib_shell_expand(context, DEFAULT_EXPERIMENTFILE);
}

char *mcelib_default_hardwarefile(const mce_context_t *context)
{
    return mcelib_shell_expand(context, DEFAULT_HARDWAREFILE);
}

char *mcelib_default_masfile(void)
{
    if (getenv("MCE_MAS_CFG"))
        return mcelib_shell_expand(NULL, "${MCE_MAS_CFG}");
    return mcelib_shell_expand(NULL, DEFAULT_MASFILE);
}
