/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mce/defaults.h>

#include "context.h"
#include "version.h"
#include "../defaults/config.h"

/* Retreive a directory named "name" from mas.cfg, or use default "def" if not
 * found */
static char *get_default_dir(const mce_context_t *c,
        config_setting_t *masconfig, const char *name, const char *def)
{
    config_setting_t *config_item;
    
    if (masconfig == NULL)
        return strdup(def);

    if ((config_item = config_setting_get_member(masconfig, name)) == NULL
            || (config_setting_type(config_item) != CONFIG_TYPE_STRING))
    {
        mcelib_warning(c, "Missing required variable `%s' in MAS "
                "configuration.", name);
        mcelib_warning(c, "Using configuration defaults.");
        if (def == NULL)
            return NULL;
        return strdup(def);
    }
    return strdup(config_setting_get_string(config_item));
}

mce_context_t* mcelib_create(int fibre_card, const char *mas_config,
        unsigned int flags)
{
    char *mas_cfg;
    config_setting_t *masconfig = NULL;
    config_setting_t *config_item = NULL;

    mce_context_t* c = (mce_context_t*)malloc(sizeof(mce_context_t));

    if (c == NULL)
        return NULL;

    memset(c, 0, sizeof(mce_context_t));
    if (fibre_card == MCE_DEFAULT_MCE)
        c->fibre_card = mcelib_default_mce();
    else
        c->fibre_card = fibre_card;

    c->flags = flags;

    /* load mas.cfg */
    if (mas_config)
        mas_cfg = strdup(mas_config);
    else {
        mas_cfg = mcelib_default_masfile();
        if (mas_cfg == NULL) {
            fprintf(stderr,
                    "mcelib: Unable to obtain path to default MAS config!\n");
            free(c);
            return NULL;
        }
    }

    c->mas_cfg = (struct config_t *)malloc(sizeof(struct config_t));
    config_init(c->mas_cfg);

    /* read mas.cfg */
    if (!config_read_file(c->mas_cfg, mas_cfg)) {
        mcelib_warning(c, "Could not read config file '%s':", mas_cfg);
        mcelib_warning(c, "    %s on lne %i", config_error_text(c->mas_cfg),
                config_error_line(c->mas_cfg));
        mcelib_warning(c, "Using configuration defaults.");

        config_destroy(c->mas_cfg);
        free(c->mas_cfg);
        /* c->mas_cfg indicates we can't read mas.cfg */
        c->mas_cfg = NULL;
    }

    /* load default paths */
    if (c->mas_cfg)
        masconfig = config_lookup(c->mas_cfg, "mas");
    if (masconfig == NULL) {
        /* this message is only meaningful if mas.cfg is readable. */
        if (c->mas_cfg) {
            mcelib_warning(c, "Missing required section `mas' in MAS "
                    "configuration.");
            mcelib_warning(c, "Using configuration defaults.\n");
        }
    } else
        c->etc_dir = get_default_dir(c, masconfig, "etcdir", NULL);

    /* etc dir (the location of mce*.cfg) */
    if (c->etc_dir == NULL) {
        if (c->mas_cfg) {
            /* if etcdir is not specified, MAS assumes the hardware config files
             * are in the same directory as the mas.cfg that we've just read. */

            /* (dirname may modify it's input) */
            char *temp = strdup(mas_cfg);
            c->etc_dir = strdup(dirname(temp));
            free(temp);

            /* absolutify, if necessary */
            if (c->etc_dir[0] != '/') {
                char *realpath = c->etc_dir;
                char *cwd = getcwd(NULL, 0);
                c->etc_dir = realloc(cwd, strlen(cwd) + strlen(realpath) + 2);
                strcat(strcat(c->etc_dir, "/"), realpath);
                free(realpath);
            }
        } else {
            /* no mas.cfg: just use a default value */
            c->etc_dir = strdup("/etc/mce");
        }
    }

    /* data root */
    if (c->fibre_card == MCE_NULL_MCE)
        c->data_root = NULL;
    else if (masconfig == NULL || (config_item =
                config_setting_get_member(masconfig, "dataroot")) == NULL ||
            (config_setting_type(config_item) != CONFIG_TYPE_ARRAY) ||
            config_setting_type(config_setting_get_elem(config_item, 0)) !=
            CONFIG_TYPE_STRING ||
            config_setting_length(config_item) <= c->fibre_card)
    {
        c->data_root = malloc(20);
#if MULTICARD
        sprintf(c->data_root, "/data/mce%i", c->fibre_card);
#else
        strcpy(c->data_root, "/data/cryo");
#endif
    } else
        c->data_root = strdup(config_setting_get_string_elem(config_item,
                    c->fibre_card));

    /* other dirs */
    c->config_dir = get_default_dir(c, masconfig, "confdir",
            MAS_PREFIX "/config");
    c->temp_dir =  get_default_dir(c, masconfig, "tmpdir", "/tmp");
    c->data_subdir = get_default_dir(c, masconfig, "datadir", "current_data");
    c->mas_root = get_default_dir(c, masconfig, "masroot",
            MAS_PREFIX "/mce_script");

    /* jam dir */
    c->jam_dir = get_default_dir(c, masconfig, "jamdir",
#if MULTICARD
            MAS_PREFIX "/firmware"
#else
            NULL
#endif
            );

#if ! MULTICARD
    /* in the single card case, we use MAS_ROOT/firmware if it exists for
     * backwards compatibility
     */
    if (c->jam_dir == NULL) {
        struct stat statbuf;

        c->jam_dir = malloc(strlen(c->mas_root) + sizeof("/firmware") + 1);
        sprintf(c->jam_dir, "%s/firmware", c->mas_root);

        /* does it exist and is it a directory? */
        if (stat(c->jam_dir, &statbuf) || !S_ISDIR(statbuf.st_mode)) {
            free(c->jam_dir);
            c->jam_dir = strdup(MAS_PREFIX "/firmware");
        }
    }
#endif

    free(mas_cfg);

    return c;
}


void mcelib_destroy(mce_context_t* context)
{
    if (context==NULL)
        return;

    mceconfig_close(context);
    mcedata_close(context);
    mcecmd_close(context);

    maslog_close(context->maslog);
    config_destroy(context->mas_cfg);
    free(context->mas_cfg);

    free(context->config_dir);
    free(context->etc_dir);
    free(context->data_dir);
    free(context->data_subdir);
    free(context->data_root);
    free(context->mas_root);
    free(context->idl_dir);
    free(context->python_dir);
    free(context->script_dir);
    free(context->template_dir);
    free(context->temp_dir);
    free(context->test_dir);

    free(context);
}


char* mcelib_version()
{
    return VERSION_STRING;
}
