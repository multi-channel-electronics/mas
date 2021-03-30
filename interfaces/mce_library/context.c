/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mce/defaults.h>

#include "context.h"
#include "autoversion.h"
#include "../defaults/config.h"

#ifndef NO_MCE_OPS
#include <sys/ioctl.h>
#include "mce/dsp_errors.h"
#include "mce/ioctl.h"

/* open a device node; this requires figuring out which kernel driver
 * we're dealing with */
int mcedev_open(mce_context_t *context, mce_subsystem_t subsys)
{
    int fd;
    char dev_name[21];

    /* get the firmware version, if unknown.  This implies we don't have
     * an active DSP subsystem */
    if (context->drv_type == MCE_DSP_UNKNOWN) {
        /* we determine driver type by trying to open a device and then using
         * an IOCTL common to both to get the driver type.  We could also
         * probably determine this by looking at the /proc file, but string
         * parsing is aesthetically displeasing */

        int new_dev = 1;
        sprintf(dev_name, "/dev/mce_dev%u", (unsigned)context->fibre_card);

        fd = open(dev_name, O_RDWR);
        if (fd < 0) {
            /* didn't work, try the legacy device */
            new_dev = 0;
            sprintf(dev_name, "/dev/mce_dsp%u", (unsigned)context->fibre_card);
            fd = open(dev_name, O_RDWR);

            if (fd < 0) {
                /* still no... bail */
                return -DSP_ERR_DEVICE;
            }
        }

        /* determine the driver type.  DSPIOCT_GET_DRV_TYPE returns 1 for the
         * current driver, 0 for the legacy driver, -1 on error */
        switch (ioctl(fd, DSPIOCT_GET_DRV_TYPE)) {
            case 0:
                context->drv_type = MCE_DSP_OLD;
                break;
            case 1:
                context->drv_type = MCE_DSP;
                break;
            default:
                close(fd);
                return -DSP_ERR_DEVICE;
        }

        /* sanity check: make sure we opened the correct device for the correct
         * driver */
        if (new_dev && context->drv_type == MCE_DSP_OLD) {
            close(fd);
            sprintf(dev_name, "/dev/mce_dsp%u", (unsigned)context->fibre_card);
            fd = open(dev_name, O_RDWR);
        } else if (!new_dev && context->drv_type == MCE_DSP) {
            /* this should fail, since we've tried already, but might as
             * will give it a go ... */
            close(fd);
            sprintf(dev_name, "/dev/mce_dsp%u", (unsigned)context->fibre_card);
            fd = open(dev_name, O_RDWR);
        }

        /* problem opening the correct device... abort */
        if (fd < 0) {
            context->drv_type = MCE_DSP_UNKNOWN; /* so we try again next time */
            return -DSP_ERR_DEVICE;
        }

        /* success on the DSP device open */
        context->dsp.fd = fd;
        context->dsp.opened = 1;
    }

    /* now try opening the requested subsystem, if necessary */
    switch (subsys) {
        case MCE_SUBSYSTEM_DSP:
            /* already done */
            break;
        case MCE_SUBSYSTEM_CMD:
            if (context->drv_type == MCE_DSP) {
                /* dup the file descriptor .. is this kosher? */
                fd = dup(context->dsp.fd);
            } else {
                sprintf(dev_name, "/dev/mce_cmd%u",
                        (unsigned)context->fibre_card);

                fd = open(dev_name, O_RDWR);
            }

            if (fd < 0)
                return -MCE_ERR_DEVICE;

            context->cmd.fd = fd;
            context->cmd.connected = 1;
            break;
        case MCE_SUBSYSTEM_DATA:
            if (context->drv_type == MCE_DSP) {
                /* dup the file descriptor .. is this kosher? */
                fd = dup(context->dsp.fd);
            } else {
                sprintf(dev_name, "/dev/mce_data%u",
                        (unsigned)context->fibre_card);

                fd = open(dev_name, O_RDWR);
            }

            if (fd < 0)
                return -MCE_ERR_DEVICE;

            context->data.fd = fd;
            context->data.connected = 1;
            sprintf(context->data.dev_name, context->drv_type == MCE_DSP ?
                    "/dev/mce_dev%u" : "/dev/mce_data%u",
                    (unsigned)context->fibre_card);
            break;
    }

    return 0;
}
#endif

/* Return non-zero if we're using an old DSP program (U0106 or earlier).
 * Returns zero if it's too early to determine which version we have (ie. if
 * the caller hasn't opened one of the subsystems yet.
 */
int mcelib_legacy(const mce_context_t *c)
{
    return (c->drv_type == MCE_DSP_OLD);
}

/* Retreive a directory named "name" from mas.cfg, or use default "def" if not
 * found */
static char *get_default_dir(const mce_context_t *c,
        config_setting_t *masconfig, const char *name, const char *def)
{
    config_setting_t *config_item;

    if (masconfig == NULL)
        return def ? strdup(def) : NULL;

    if ((config_item = config_setting_get_member(masconfig, name)) == NULL
            || (config_setting_type(config_item) != CONFIG_TYPE_STRING))
    {
        mcelib_warning(c, "Missing required variable `%s' in MAS "
                "configuration.", name);
        mcelib_warning(c, "Using configuration defaults.");
        return def ? strdup(def) : NULL;
    }
    return strdup(config_setting_get_string(config_item));
}

static int mcelib_output(int severity, const char *message)
{
    fputs(message, stderr);

    /* append a newline if necessary */
    if (message[strlen(message) - 1] != '\n')
        fputc('\n', stderr);

    return 0;
}

/* Change the terminal output function used by the library -- passing NULL
 * resets this to the default value */
void mcelib_set_termio(mce_context_t *c, int (*func)(int, const char*))
{
    c->termio = func ? func : mcelib_output;
}

mce_context_t* mcelib_create_termio(int fibre_card, const char *mas_config,
        unsigned int flags, int (*termio_func)(int, const char*))
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
    mcelib_set_termio(c, termio_func);

    /* load mas.cfg */
    if (mas_config)
        mas_cfg = strdup(mas_config);
    else {
        mas_cfg = mcelib_default_masfile();
        if (mas_cfg == NULL) {
            mcelib_error(c, "Unable to obtain path to default MAS config!\n");
            free(c);
            return NULL;
        }
    }

    c->mas_cfg = (struct config_t *)malloc(sizeof(struct config_t));
    config_init(c->mas_cfg);

    /* read mas.cfg */
    if (!config_read_file(c->mas_cfg, mas_cfg)) {
        mcelib_warning(c, "Could not read config file '%s':", mas_cfg);
        mcelib_warning(c, "    %s on line %i", config_error_text(c->mas_cfg),
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
            /* no mas.cfg: use the configure-time default */
            c->etc_dir = strdup(MAS_ETCDIR);
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
        /* fallback to configure-time default if not specified */
#if MULTICARD
        c->data_root = malloc(sizeof(MAS_DATAROOT) + 20);
        sprintf(c->data_root, MAS_DATAROOT "%i", c->fibre_card);
#else
        c->data_root = strdup(MAS_DATAROOT);
#endif
    } else
        c->data_root = strdup(config_setting_get_string_elem(config_item,
                    c->fibre_card));

    /* other dirs -- fallback to configure-time defaults if not specified */
    c->config_dir = get_default_dir(c, masconfig, "confdir", MAS_CONFDIR);
    c->temp_dir =  get_default_dir(c, masconfig, "tmpdir", MAS_TEMPDIR);
    c->data_subdir = get_default_dir(c, masconfig, "datadir", MAS_DATADIR);
    c->mas_root = get_default_dir(c, masconfig, "masroot", MAS_ROOTDIR);

    /* jam dir */
    c->jam_dir = get_default_dir(c, masconfig, "jamdir", MAS_JAMDIR);
    free(mas_cfg);

    return c;
}

mce_context_t* mcelib_create(int fibre_card, const char *mas_config,
        unsigned int flags)
{
    return mcelib_create_termio(fibre_card, mas_config, flags, NULL);
}


void mcelib_destroy(mce_context_t* context)
{
    if (context==NULL)
        return;

    mceconfig_close(context);
#ifndef NO_MCE_OPS
    mcedata_close(context);
    mcecmd_close(context);
#endif

    maslog_close(context->maslog);
    if (context->mas_cfg)
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

const char* mcelib_version()
{
    return VERSION_STRING;
}
