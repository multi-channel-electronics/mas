/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>

#include <mce_library.h>
#include <mce/defaults.h>
#include <mce/socks.h>
#include <mcenetd.h>

#include "context.h"
#include "eth.h"
#include "net.h"
#include "sdsu.h"
#include "version.h"
#include "../defaults/config.h"

/* determine the endpoint (type of the actual physical MCE communicating to
 * this MAS.  Incidentally, this will also detect network loops, since a
 * self-referential connection won't work here, given that we haven't started
 * our server yet */
static int find_endpoint(mce_context_t context)
{
    /* non-networked devices are easy */
    if (context->dev_route != net) {
        context->dev_endpoint = (context->dev_route == eth) ? 1 : 0;
        return 0;
    }

    /* connect to the remote host */
    context->net.sock = massock_connect(context->dev_name, MCENETD_CTLPORT);
    if (context->net.sock < 0) {
        fprintf(stderr, "mcelib: Unable to connect to mcenetd at %s\n",
                context->dev_name);
        return -1;
    }

    /* send the greeting, this will set the endpoint */
    if (mcenet_hello(context))
        return -1;

    return 0;
}

/* parse the MCE URL */
static int parse_url(mce_context_t context)
{
    char *nptr, *ptr = context->url;
    /* check for "mce:/", case insensitive */
    if ((*(ptr++) & 0xDF) != 'M' || (*(ptr++) & 0xDF) != 'C'
            || (*(ptr++) & 0xDF) != 'E' || *(ptr++) != ':' || *(ptr++)!= '/') {
        fprintf(stderr, "mcelib: Unknown scheme in URL: %s\n", context->url);
        return 1;

    }
    context->dev_name = NULL;
    context->dev_route = none;

    /* check for authority (ie dev_namename) section */
    if (*ptr == '/') {
        ptr++;
        /* this is more space than we need, but ... whatever */
        nptr = context->dev_name = strdup(context->url);

        /* copy dev_namename, it ends at the next '/' */
        while (*ptr && *ptr != '/')
            *(nptr++) = *(ptr++);
        *nptr = '\0';

        if (*(ptr++) != '/' || *ptr < '0' || *ptr > '9') {
            fprintf(stderr, "mcelib: bad path in URL: %s\n", context->url);
            return 1;
        }

        /* convert to a number, ensuring there's no trailing */
        context->dev_num = (int)strtol(ptr, &nptr, 10);

        /* trailing characters */
        if (*nptr) {
            fprintf(stderr, "mcelib: trailing data in URL: %s\n", context->url);
            return 1;
        }
        context->dev_route = net;
    } else if (*ptr == 's') {
        /* SDSU card, hopefully */
        ptr++;
        if (*(ptr++) != 'd' || *(ptr++) != 's' || *(ptr++) != 'u' ||
                *(ptr++) != '/' || *ptr < '0' || *ptr > '9') {
            fprintf(stderr, "mcelib: bad path in URL: %s\n", context->url);
            return 1;
        }

        /* convert to a number, ensuring there's no trailing */
        context->dev_num = (int)strtol(ptr, &nptr, 10);

        /* trailing characters */
        if (*nptr) {
            fprintf(stderr, "mcelib: trailing data in URL: %s\n", context->url);
            return 1;
        }
        context->dev_route = sdsu;
    } else if (*ptr == 'e') {
        /* Ethernet, hopefully */
        ptr++;
        if (*(ptr++) != 't' || *(ptr++) != 'h' || *(ptr++) != '/' ||
                *ptr == '\0') {
            fprintf(stderr, "mcelib: bad path in URL: %s\n", context->url);
            return 1;
        }

        /* this is more space than we need, but ... whatever */
        nptr = context->dev_name = strdup(context->url);

        /* copy dev_namename, it ends at the next '/' */
        while (*ptr && *ptr != '?' && *ptr != '/')
            *(nptr++) = *(ptr++);
        *nptr = '\0';

        if (*(ptr++) != '?') {
            fprintf(stderr, "mcelib: bad path in URL: %s\n", context->url);
            return 1;
        }

        if (strlen(ptr) != 17) {
            fprintf(stderr, "mcelib: bad query in URL: %s\n", context->url);
            return 1;
        }

        /* MAC address: it's a weirdly-encoded 48-bit integer */
        unsigned int a, b, c, d, e, f;
        int n = sscanf(ptr, "%2x:%2x:%2x:%2x:%2x:%2x", &a, &b, &c, &d, &e, &f);
        if (n != 6) {
            fprintf(stderr, "mcelib: bad query in URL: %s\n", context->url);
            return 1;
        }
        /* XXX check endianness */
        context->dev_num = ((uint64_t)f << 40) + ((uint64_t)e << 32) +
            ((uint64_t)d << 24) + ((uint64_t)c << 16) + ((uint64_t)b << 8) + a;

        /* trailing characters */
        if (*nptr) {
            fprintf(stderr, "mcelib: trailing data in URL: %s\n", context->url);
            return 1;
        }
        context->dev_route = net;
    } else {
        /* something else */
        fprintf(stderr, "mcelib: bad path in URL: %s\n", context->url);
        return 1;
    }

    return 0;
}

mce_context_t mcelib_create(int dev_num, const char *mas_config,
        unsigned char udepth)
{
    char *envdev;
    config_setting_t *masconfig;
    config_setting_t *mcelist = NULL;
    mce_context_t c = (mce_context_t)malloc(sizeof(struct mce_context));
    c->mas_cfg = (struct config_t *)malloc(sizeof(struct config_t));
    config_init(c->mas_cfg);

    if (c == NULL)
        return NULL;

    c->url = c->dev_name = NULL;
    c->maslog = NULL;
    c->cmd.connected = 0;
    c->data.connected = 0;
    c->config.connected = 0;
    c->udepth = udepth;

    /* if MCE_DEVICE_URL is set in the environment, we use that as our MCE
     * device, and ignore mas.cfg completely.  This has implications for the
     * logger started by the cmd subsystem, but q.v. for a similar workaround */
    envdev = getenv("MCE_DEVICE_URL");
    if (envdev) {
        c->mas_cfg = NULL;
        c->ndev = 0;
        c->dev_index = 0;
        c->url = strdup(envdev);
        if (parse_url(c)) {
            mcelib_destroy(c);
            return NULL;
        }
        if (find_endpoint(c)) {
            mcelib_destroy(c);
            return NULL;
        }
        return c;
    }

    if (dev_num == MCE_DEFAULT_MCE || dev_num < 0)
        c->dev_index = mcelib_default_mce();
    else
        c->dev_index = dev_num;

    /* load mas.cfg */
    if (mas_config != NULL) {
        if (!config_read_file(c->mas_cfg, mas_config)) {
            fprintf(stderr, "mcelib: Could not read config file '%s':\n"
                    "   %s on line %i\n", mas_config,
                    config_error_text(c->mas_cfg),
                    config_error_line(c->mas_cfg));
            mcelib_destroy(c);
            return NULL;
        }
    } else {
        char *ptr = mcelib_default_masfile();
        if (ptr == NULL) {
            fprintf(stderr,
                    "mcelib: Unable to obtain path to default configfile!\n");
            mcelib_destroy(c);
            return NULL;
        } else if (!config_read_file(c->mas_cfg, ptr)) {
            fprintf(stderr,
                    "mcelib: Could not read default configfile '%s'\n", ptr);
            mcelib_destroy(c);
            free(ptr);
            return NULL;
        }
        free(ptr);
    }

    /* load the device list */
    masconfig = config_lookup(c->mas_cfg, "mas");
    if (masconfig == NULL)
        fprintf(stderr, "mcelib: Missing required section `mas' in MAS "
                "configuration.\n");
    else
        mcelist = config_setting_get_member(masconfig, "mce_devices");

    if (mcelist) {
        if (config_setting_type(mcelist) != CONFIG_TYPE_ARRAY ||
                config_setting_type(config_setting_get_elem(mcelist, 0))
                != CONFIG_TYPE_STRING) {
            fprintf(stderr, "mcelib: Ignoring malformed MCE device list.\n");
            mcelist = NULL;
        } else {
            c->ndev = config_setting_length(mcelist);
            if (c->dev_index >= c->ndev) {
                fprintf(stderr, "mcelib: Device number out of range.\n");
                mcelib_destroy(c);
                return NULL;
            }
            c->url = strdup(config_setting_get_string_elem(mcelist,
                        c->dev_index));
        }
    }
    if (mcelist == NULL) {
        fprintf(stderr, "mcelib: No MCE device list.  Assuming a naive "
                "default.\n");
        c->ndev = MAX_FIBRE_CARD;

        if (c->dev_index >= MAX_FIBRE_CARD) {
            fprintf(stderr, "mcelib: Device number out of range.\n");
            mcelib_destroy(c);
            return NULL;
        }

        c->url = (char *)malloc(8);
        sprintf(c->url, "mce://%i", c->dev_index);
    }

    if (parse_url(c)) {
        mcelib_destroy(c);
        return NULL;
    }
    return c;
}

const char *mcelib_dev(mce_context_t context)
{
    return context->url;
}

struct config_t *mcelib_mascfg(mce_context_t context)
{
    return context->mas_cfg;
}

int mcelib_ndev(mce_context_t context)
{
    return context->ndev;
}

int mcelib_ddepth(mce_context_t context)
{
    return context->ddepth;
}

void mcelib_destroy(mce_context_t context)
{
    if (context==NULL) return;

    mceconfig_close(context);
    mcedata_close(context);
    mcecmd_close(context);

    maslog_close(context->maslog);
    config_destroy(context->mas_cfg);
    free(context->mas_cfg);
    free(context->url);
    free(context->dev_name);

    free(context);
}


char* mcelib_version()
{
    return VERSION_STRING;
}
