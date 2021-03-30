/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _CMD_H_
#define _CMD_H_

#include <mce_library.h>

#include "../../defaults/config.h"

#define PROGRAM_NAME "mce_cmd"

#define LINE_LEN 1024
#define NARGS 256
#define DATA_SIZE 2048

#define FS_DIGITS 3 /* Number of digits in file sequencing file name */

enum {
    ERR_MEM=-1,
    ERR_OPT=-2,
    ERR_MCE=-3
};

#endif
