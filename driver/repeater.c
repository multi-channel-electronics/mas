/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#include <linux/interrupt.h>

#include "mce_options.h"
#include "repeater.h"

void repeater_task(unsigned long data)
{
	repeater_t *r = (repeater_t *)data;
	int last_chance = ++r->attempts >= r->max_attempts;
	int err = r->func(r->private_data, last_chance);
	if (err == -EAGAIN) {
		if (!last_chance) {
			tasklet_schedule(&r->tasklet);
		} else {
                        PRINT_ERR(NOCARD, "giving up after %i attempts\n",
                                        r->attempts);
		}
	}
	if (err == 0 && r->attempts != 1) {
                PRINT_ERR(NOCARD, "succeeded after %i attempts\n", r->attempts);
	}
}

void repeater_init(repeater_t *repeater, repeater_func func, unsigned long data)
{
	repeater->private_data = data;
	repeater->func = func;
	tasklet_init(&repeater->tasklet, repeater_task, (unsigned long)repeater);
}

void repeater_execute(repeater_t *repeater, int max_attempts)
{
	repeater->attempts = 0;
	if (max_attempts <= 0) max_attempts = 1;
	repeater->max_attempts = max_attempts;
	repeater_task((unsigned long)repeater);
}

void repeater_kill(repeater_t *repeater)
{
	tasklet_kill(&repeater->tasklet);
}
