#ifndef _LIBMASLOG_H_
#define _LIBMASLOG_H_

#include <mce_library.h>

/*
   When using libmcelog it is necessary to also link to libconfig,
   i.e. "gcc src.c -o src -lmcelog -lconfig"

   Since libconfig likes to install in /usr/local/lib, the application
   will fail to find libconfig.so.2 unless this folder is added to the
   shared library path.  This is most easily accomplished by adding
   /usr/local/lib to /etc/ld.so.conf .
*/

/* Logging levels, passed to maslog_print; messages are only logged if
 *  their level meets or exceeds the server logging threshold */
#define MASLOG_DRIVER    0
#define MASLOG_DETAIL    1
#define MASLOG_INFO      2
#define MASLOG_ALWAYS    3

struct maslog_struct;
typedef struct maslog_struct maslog_t;

int maslog_close(maslog_t *logger);
int maslog_print(maslog_t *logger, const char *str);
int maslog_print_level(maslog_t *logger, const char *str, int level);
int maslog_write(maslog_t *logger, const char *buf, int size);
maslog_t *maslog_connect(mce_context_t *context, char *name);

#endif
