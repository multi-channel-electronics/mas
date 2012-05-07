/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef MCELIB_CONTEXT_H
#define MCELIB_CONTEXT_H

#include <mce_library.h>
#include <libmaslog.h>

struct maslog_struct {
    int fd;       // ha ha, it's just an int.
};

/* Root configuration structure */

typedef struct mceconfig {
    int connected;
    char *filename;

    struct config_t cfg;

    config_setting_t *parameter_sets;
    config_setting_t *card_types;
    config_setting_t *components;
    config_setting_t *mappings;

    int card_count;
    int cardtype_count;
    int paramset_count;
    int mapping_count;
} mceconfig_t;

/* Module information structure */

typedef struct mcecmd {
    int connected;
    int fd;

    char dev_name[MCE_LONG];
    char errstr[MCE_LONG];
} mcecmd_t;

/* Module information structure */

typedef struct mcedata {
    int connected;
    int fd;

    char dev_name[MCE_LONG];
    char errstr[MCE_LONG];

    void *map;
    int map_size;
} mcedata_t;

#define MCEDATA_PACKET_MAX 4096 /* Maximum frame size in dwords */

typedef struct mcedsp {
    int opened;
    int fd;
} mcedsp_t;

/* Context structure associates connections on the three modules. */

struct mce_context {
    mcecmd_t          cmd;            /* command subsystem */
    mcedata_t         data;           /* data subsystem */
    mcedsp_t          dsp;            /* dsp subsystem */
    mceconfig_t       config;         /* hardware config subsystem */
    maslog_t         *maslog;         /* maslog subsystem */

    unsigned int      flags;          /* MCELIB public flags */
    struct config_t  *mas_cfg;        /* MAS configuration */
    int               fibre_card;     /* logical fibre card number */

    char             *config_dir;     /* $(MAS_CONFIG} */
    char             *data_root;      /* the base data directory */
    char             *data_subdir;    /* "current_data", or whatever */
    char             *etc_dir;        /* location of the mce.cfg files */
    char             *mas_root;       /* ${MAS_ROOT} */
    char             *temp_dir;       /* ${MAS_TEMP} */

    /* these are here for proper memory management */
    char             *data_dir;       /* ${MAS_DATA} */
    char             *idl_dir;        /* ${MAS_IDL} */
    char             *python_dir;     /* ${MAS_PYTHON} */
    char             *script_dir;     /* ${MAS_SCRIPT} */
    char             *template_dir;   /* ${MAS_TEMPLATE} */
    char             *test_dir;       /* ${MAS_TEST_SUITE} */
};

/* Macros for easy dereferencing of mce_context_t context into cmd,
   data, and config members, and to check for connections of each
   type. */

#define  C_cmd          context->cmd
#define  C_data         context->data
#define  C_config       context->config

#define  C_cmd_check    if (!C_cmd.connected)    return -MCE_ERR_NEED_CMD
#define  C_data_check   if (!C_data.connected)   return -MCE_ERR_NEED_DATA
#define  C_config_check if (!C_config.connected) return -MCE_ERR_NEED_CONFIG
#define  C_maslog_check if (!C_config.connected) return -MCE_ERR_NEED_CONFIG

int mcelib_warning(const mce_context_t *context, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

#endif
