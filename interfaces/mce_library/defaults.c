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
static void check_env(int dev)
{
    static int done = -1;

    if (dev < 0)
        dev = -1;

    /* no need to try this every time */
    if ((dev != -1 && done == dev) || (dev == -1 && done == DEFAULT_MCE)) {
        return;
    }

    /* MAS_MCE_DEV is overridden if dev is set */
    char buffer[10];
    if (dev == -1) {
        sprintf(buffer, "%i", DEFAULT_MCE);
        setenv("MAS_MCE_DEV", buffer, 0);
        dev = DEFAULT_MCE;
    } else {
        sprintf(buffer, "%i", dev);
        setenv("MAS_MCE_DEV", buffer, 1);
    }
    SET_MASDEFAULT(BIN);
    SET_MASDEFAULT(DATA);
    SET_MASDEFAULT(DATA_ROOT);
    SET_MASDEFAULT(IDL);
    SET_MASDEFAULT(PYTHON);
    SET_MASDEFAULT(SCRIPT);
    SET_MASDEFAULT(TEMP);
    SET_MASDEFAULT(TEMPLATE);
    SET_MASDEFAULT(TEST_SUITE);
    done = dev;
}

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const char* input, int dev)
{
#if 0
    fprintf(stderr, "%s(\"%s\", %i)\n", __func__, input, dev);
#endif
    char *ptr;
    static int depth = 0;
    wordexp_t expansion;

    /* avoid infinite recursion */
    if (depth > 100) {
        ptr = strdup(input);
        return ptr;
    } else
        depth++;

    check_env(dev);

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
        ptr = mcelib_shell_expand(ptr, dev);
        free(old);
    }
#if 0
    else
        fprintf(stderr, "%s = \"%s\"\n", __func__, ptr);
#endif

    depth--;
    return ptr;
}

int mcelib_default_mce(void)
{
    const char *ptr = getenv("MAS_MCE_DEV");
    return (ptr == NULL) ? DEFAULT_MCE : atoi(ptr);
}

char *mcelib_default_experimentfile(int dev_index)
{
    return mcelib_shell_expand(DEFAULT_EXPERIMENTFILE, dev_index);
}

char *mcelib_default_hardwarefile(int dev_index)
{
    return mcelib_shell_expand(DEFAULT_HARDWAREFILE, dev_index);
}

char *mcelib_default_masfile(void)
{
    return mcelib_shell_expand(DEFAULT_MASFILE, -1);
}
