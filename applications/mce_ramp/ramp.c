#include <stdlib.h>
#include <stdio.h>

#include "ramp.h"

int run_ramp(loop_t* loop)
{
	int l, v, p;
	int totals[MAX_VALUES];

	if (loop == NULL) return 0;

	for (v=0; v<loop->value_count; v++) {
		totals[v] = loop->values[v].start;
	}

	for (l=0; l<loop->count; l++) {
		for (v=0; v<loop->value_count; v++) {
			for (p=0; p<loop->values[v].operation_count; p++) {
				process_operation(
					loop->values[v].operations+p,
					totals[v]);
			}
			totals[v] += loop->values[v].step;
		}
		if ( run_ramp(loop->sub_loop) != 0 )
			return -1;
	}
	return 0;
}

char buf[1024];
char* repeat_integer(int value, int count)
{
	char* s = buf;
	*s = 0;
	while (count--) s += sprintf(s, " %i", value);
	return buf;
}

int process_operation(operation_t *op, int value)
{
	printf("%s%s\n", op->command, repeat_integer(value, op->repeat));
	return 0;
}

int print_ambles(amble_t *ambles, int count)
{
	while (count--)
		printf("%s\n", *(ambles++));
	return 0;
}

int print_status_block(loop_t *loop, const char *prefix)
{
	int v, p;
	char new_prefix[AMBLE_LEN];

	if (loop == NULL) return 0;

	sprintf(new_prefix, "%s    ", prefix);

	printf("%s<loop> %i\n", prefix, loop->count);

	for (v=0; v<loop->value_count; v++) {
		printf("%s  <value> %i %i\n", prefix,
		       loop->values[v].start, loop->values[v].step);
		for (p=0; p<loop->values[v].operation_count; p++) {
			printf("%s    <operation> %s\n", prefix, loop->values[v].operations[p].command);
		}
	}
	return print_status_block(loop->sub_loop, new_prefix);
}
