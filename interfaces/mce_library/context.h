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

struct mcedsp_context {
	int opened;
	int fd;
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

    int               ndev;           /* The number of MCE devices in the MAS
                                         configuration -- not used by mcelib
                                         itself, but stored for informational
                                         purposes for the user. */

    enum {
        none = 0, sdsu, net, eth
    }                 dev_route;      /* device routing */
    char             *dev_name;       /* hostname or eth device in url */
    uint64_t          dev_num;        /* device number or mac address in url */
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
