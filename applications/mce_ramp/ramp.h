/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _RAMP_H_
#define _RAMP_H_

#include "../../defaults/config.h"

/* Structure!

   - Set of nested loops.  Only one loop per level.
   -   Loop has any number of value sets, each having a start and increment
   -     Each value set is associated with an operation (mce command...)
   -   Loop has a sub-loop, or not.

*/

#define MAX_AMBLES         20
#define MAX_LOOPS          5
#define MAX_VALUES         20
#define MAX_OPERATIONS     100

#define AMBLE_LEN 1024

typedef char amble_t[AMBLE_LEN];

typedef struct {
    char* command;
    int repeat;
} operation_t;

typedef struct {
    int start;
    int step;

    int operation_count;
    operation_t* operations;
} value_t;

typedef struct loop_struct loop_t;

struct loop_struct {
    int count;

    int value_count;
    value_t* values;

    loop_t *sub_loop;
};


int run_ramp(loop_t* loop);
int process_operation(operation_t *op, int value);
int print_ambles(amble_t *ambles, int count);
int print_status_block(loop_t *loop, const char *prefix);


#endif
