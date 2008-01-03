#ifndef _MCE_LIBRARY_H_
#define _MCE_LIBRARY_H_

#include <libmaslog.h>
#include <mce.h>
#include <mce_errors.h>


/*
  - First define mce_context_t, since some modules need to point to it.
  - Config module must preceed command module, which uses its types
  - Command module must preceed data, which can issue commands
*/

struct mce_context;
typedef struct mce_context mce_context_t;

#include <mceconfig.h>
#include <mcecmd.h>
#include <mcedata.h>


/* Context structure associates connections on the three modules. */

struct mce_context {

	mcecmd_t    cmd;
	mcedata_t   data;
	mceconfig_t config;
	logger_t logger;

};


/* Creation / destruction of context structure */

mce_context_t* mcelib_create(void);
void mcelib_destroy(mce_context_t* context);


/* Error look-up, versioning */

char* mcelib_error_string(int error);
char* mcelib_version();

/* Macros for easy dereferencing of mce_context_t* context into cmd,
   data, and config members, and to check for connections of each
   type. */

#define  C_cmd          context->cmd
#define  C_data         context->data
#define  C_config       context->config
#define  C_logger       (context->logger)

#define  C_cmd_check    if (!C_cmd.connected)    return -MCE_ERR_NEED_CMD
#define  C_data_check   if (!C_data.connected)   return -MCE_ERR_NEED_DATA
#define  C_config_check if (!C_config.connected) return -MCE_ERR_NEED_CONFIG
#define  C_logger_check if (!C_config.connected) return -MCE_ERR_NEED_CONFIG


#endif
