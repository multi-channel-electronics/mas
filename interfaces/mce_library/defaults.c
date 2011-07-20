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
    setenv("MAS_" #x, mcelib_lookup_dir(context, MAS_DIR_ ##x), 0)

/* sanitise the environment */
static void check_env(const mce_context_t context)
{
    char buffer[10];

    if (context == NULL)
        sprintf(buffer, "%i", mcelib_default_mce());
    else
        sprintf(buffer, "%i", context->dev_index);
    setenv("MAS_MCE_DEV", buffer, 1);

    SET_MASDEFAULT(BIN);
    if (context) {
        SET_MASDEFAULT(ETC);
    }
}

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const mce_context_t context, const char* input)
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
    const char *ptr = getenv("MAS_MCE_DEV");
    return (ptr == NULL) ? DEFAULT_MCE : atoi(ptr);
}

char *mcelib_default_experimentfile(const mce_context_t context)
{
    const char *datadir = mcelib_lookup_dir(context, MAS_DIR_DATA);
    char *expcfg = malloc(strlen(datadir) + 16);

    sprintf(expcfg, "%s/experiment.cfg", datadir);

    return expcfg;
}

char *mcelib_default_hardwarefile(const mce_context_t context)
{
    char *mcecfg;
    const char *etcdir = mcelib_lookup_dir(context, MAS_DIR_ETC);;

    if (etcdir == NULL)
        return NULL;

    mcecfg = malloc(strlen(etcdir) + 12);

    if (mcecfg == NULL)
        return NULL;

    sprintf(mcecfg, "%s/mce%i.cfg", etcdir, context->dev_index);

    return mcecfg;
}

char *mcelib_default_masfile(void)
{
    if (getenv("MCE_MAS_CFG"))
        return mcelib_shell_expand(NULL, "${MCE_MAS_CFG}");
    return mcelib_shell_expand(NULL, DEFAULT_MASFILE);
}

const char *mcelib_lookup_dir(const mce_context_t context, int index)
{
    const char *dir = NULL;
    switch(index) {
        case MAS_DIR_BIN:
            dir = getenv("MAS_BIN");
            if (dir == NULL)
                dir = MAS_PREFIX "/bin";
            break;
        case MAS_DIR_DATA:
            dir = getenv("MAS_DATA");
            if (dir == NULL) {
                const char *dataroot = mcelib_lookup_dir(context,
                        MAS_DIR_DATA_ROOT);
                free(context->data_dir);
                dir = context->data_dir = malloc(strlen(dataroot) + 2 +
                        strlen(context->data_subdir));
                sprintf(context->data_dir, "%s/%s", dataroot,
                        context->data_subdir);
            }
            break;
        case MAS_DIR_DATA_ROOT:
            dir = getenv("MAS_DATA_ROOT");
            if (dir == NULL)
                dir = context->data_root;
            break;
        case MAS_DIR_ETC:
            dir = getenv("MAS_ETC");
            if (dir == NULL)
                dir = context->etc_dir;
            break;
        case MAS_DIR_IDL:
            dir = getenv("MAS_IDL");
            if (dir == NULL) {
                const char *masroot = mcelib_lookup_dir(context, MAS_DIR_ROOT);
                free(context->idl_dir);
                dir = context->idl_dir = malloc(strlen(masroot) + 5);
                sprintf(context->idl_dir, "%s/idl", masroot);
            }
            break;
        case MAS_DIR_PYTHON:
            dir = getenv("MAS_PYTHON");
            if (dir == NULL) {
                const char *masroot = mcelib_lookup_dir(context, MAS_DIR_ROOT);
                free(context->python_dir);
                dir = context->python_dir = malloc(strlen(masroot) + 8);
                sprintf(context->python_dir, "%s/python", masroot);
            }
            break;
        case MAS_DIR_ROOT:
            dir = getenv("MAS_ROOT");
            if (dir == NULL)
                dir = context->mas_root;
            break;
        case MAS_DIR_SCRIPT:
            dir = getenv("MAS_SCRIPT");
            if (dir == NULL) {
                const char *masroot = mcelib_lookup_dir(context, MAS_DIR_ROOT);
                free(context->script_dir);
                dir = context->script_dir = malloc(strlen(masroot) + 8);
                sprintf(context->script_dir, "%s/script", masroot);
            }
            break;
        case MAS_DIR_TEMP:
            dir = getenv("MAS_TEMP");
            if (dir == NULL)
                dir = context->temp_dir;
            break;
        case MAS_DIR_TEMPLATE:
            dir = getenv("MAS_TEMPLATE");
            if (dir == NULL) {
                const char *masroot = mcelib_lookup_dir(context, MAS_DIR_ROOT);
                free(context->template_dir);
                dir = context->template_dir = malloc(strlen(masroot) + 10);
                sprintf(context->template_dir, "%s/template", masroot);
            }
            break;
        case MAS_DIR_TEST_SUITE:
            dir = getenv("MAS_TEST_SUITE");
            if (dir == NULL) {
                const char *masroot = mcelib_lookup_dir(context, MAS_DIR_ROOT);
                free(context->test_dir);
                dir = context->test_dir = malloc(strlen(masroot) + 12);
                sprintf(context->test_dir, "%s/test_suite", masroot);
            }
            break;
        default:
            fprintf(stderr, "mcelib: can't find directory #%i in %s\n", index,
                    __func__);
    }

    return dir;
}
