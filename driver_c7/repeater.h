/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <linux/interrupt.h>

typedef int (*repeater_func)(unsigned long data, int last_chance);

typedef struct {
    int attempts;
    int max_attempts;
    repeater_func func;
    unsigned long private_data;
    struct tasklet_struct tasklet;
} repeater_t;

void repeater_init(repeater_t *repeater, repeater_func func, unsigned long data);
void repeater_execute(repeater_t *repeater, int max_attempts);
void repeater_kill(repeater_t *repeater);
