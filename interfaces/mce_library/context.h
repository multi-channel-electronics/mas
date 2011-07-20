/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef MCELIB_CONTEXT_H
#define MCELIB_CONTEXT_H
/* Context structure associates connections on the three modules. */

#include <libmaslog.h>
#include <mceconfig.h>
#include <mcecmd.h>
#include <mcedata.h>
#include <stdint.h>

#define SET_IO_METHODS(c,subsys,name) do { \
    c->subsys.connect = mce ## subsys ## _ ## name ## _connect; \
    c->subsys.disconnect = mce ## subsys ## _ ## name ## _disconnect; \
    c->subsys.ioctl = mce ## subsys ## _ ## name ## _ioctl; \
    c->subsys.read = mce ## subsys ## _ ## name ## _read; \
    c->subsys.write = mce ## subsys ## _ ## name ## _write; \
} while (0);

typedef struct mcedata {

    int connected;
    int fd;

    char *dev_name;
    char errstr[MCE_LONG];

    void *map;
    int map_size;

    /* I/O methods */
    int (*connect)(mce_context_t);
    int (*disconnect)(mce_context_t);
    int (*ioctl)(mce_context_t, unsigned long int, int);
    ssize_t (*read)(mce_context_t, void*, size_t);
    void *write; /* laziness */
} mcedata_t;

typedef struct mceconfig {

    int connected;

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

typedef struct mcecmd {

    int connected;
    int fd;

    char dev_name[MCE_LONG];
    char errstr[MCE_LONG];

    /* I/O methods */
    int (*connect)(mce_context_t);
    int (*disconnect)(mce_context_t);
    int (*ioctl)(mce_context_t, unsigned long int, int);
    int (*read)(mce_context_t, void*, size_t);
    int (*write)(mce_context_t, const void*, size_t);
} mcecmd_t;

typedef enum {
    none = 0, sdsu, net, eth
} devtype_t;

struct mcenet_client {
    int           proto;          /* server protocol version */
    int           ctl_sock;       /* network control socket */
    int           dsp_sock;       /* network control socket */
    int           cmd_sock;       /* network control socket */
    int           dat_sock;       /* network control socket */
    unsigned char token;          /* handshake token */
    unsigned char flags;          /* server flags */
    unsigned char udepth;         /* upstream depth */
    unsigned char ddepth;         /* downstream depth */
};

struct mcedsp_context {
    int opened;
    int fd;

    /* I/O methods */
    int (*connect)(mce_context_t);
    int (*disconnect)(mce_context_t);
    int (*ioctl)(mce_context_t, unsigned long int, int);
    int (*read)(mce_context_t, void*, size_t);
    int (*write)(mce_context_t, const void*, size_t);
};
typedef struct mcedsp_context mcedsp_t;

struct mce_context {
    mcecmd_t          cmd;            /* command subsystem */
    mcedata_t         data;           /* data subsystem */
    mcedsp_t          dsp;            /* dsp subsystem */
    mceconfig_t       config;         /* hardware config subsystem */
    maslog_t          maslog;         /* maslog subsystem */

    struct config_t  *mas_cfg;        /* MAS configuration */
    int               dev_index;      /* MCE device index for this context */
    char             *url;            /* MCE device URL */

    char             *data_root;      /* the base data directory */
    char             *data_dir;       /* DATA_ROOT/current_data, or whatever */
    char             *data_subdir;    /* "current_data", or whatever */
    char             *etc_dir;        /* location of the mce.cfg files */
    char             *mas_root;       /* ${MAS_ROOT} */
    char             *idl_dir;        /* ${MAS_IDL} */
    char             *python_dir;     /* ${MAS_PYTHON} */
    char             *script_dir;     /* ${MAS_SCRIPT} */
    char             *template_dir;   /* ${MAS_TEMPLATE} */
    char             *temp_dir;       /* ${MAS_TEMP} */
    char             *test_dir;       /* ${MAS_TEST_SUITE} */

    int               ndev;           /* The number of MCE devices in the MAS
                                         configuration -- not used by mcelib
                                         itself, but stored for informational
                                         purposes for the user. */

    struct mcenet_client net;         /* mcenet client data */
    unsigned char        udepth;      /* upstream depth */
    unsigned char        ddepth;      /* downstream depth */

    devtype_t          dev_route;     /* device routing (the type of the device
                                         immediately below us) */
    devtype_t          dev_endpoint;  /* the type of the physical MCE that
                                         MAS ultimately controls */
    char             *dev_name;       /* hostname or eth device in url */
    uint64_t          dev_num;        /* device number or mac address in url */

    void             *transport_data; /* private configuration data for transport layer */
};

/* Macros for easy dereferencing of mce_context_t context into cmd,
   data, and config members, and to check for connections of each
   type. */

#define  C_cmd          context->cmd
#define  C_data         context->data
#define  C_config       context->config
#define  C_maslog       (context->maslog)

#define  C_cmd_check    if (!C_cmd.connected)    return -MCE_ERR_NEED_CMD
#define  C_data_check   if (!C_data.connected)   return -MCE_ERR_NEED_DATA
#define  C_config_check if (!C_config.connected) return -MCE_ERR_NEED_CONFIG
#define  C_maslog_check if (!C_config.connected) return -MCE_ERR_NEED_CONFIG


#endif
