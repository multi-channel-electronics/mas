#ifndef _LIBMASLOG_H_
#define _LIBMASLOG_H_

typedef struct maslog_struct *maslog_t;

#include <mce_library.h>

/* Logging levels, passed to maslog_print; messages are only logged if
 *  their level meets or exceeds the server logging threshold */
#define MASLOG_DRIVER    0
#define MASLOG_DETAIL    1
#define MASLOG_INFO      2
#define MASLOG_ALWAYS    3

int maslog_close(maslog_t maslog);
int maslog_print(maslog_t maslog, const char *str);
int maslog_print_level(maslog_t maslog, const char *str, int level);
int maslog_write(maslog_t maslog, const char *buf, int size);
maslog_t maslog_connect(const mce_context_t mce, const char *name);

#endif
