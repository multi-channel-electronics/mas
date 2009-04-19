#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ramp.h"
#include "options.h"

int main(int argc, char** argv)
{
	options_t options;
	memset(&options, 0, sizeof(options));

	char ambles[MAX_AMBLES][LINE_LEN];
	loop_t loops[MAX_LOOPS];
	value_t values[MAX_VALUES];
	operation_t operations[MAX_OPERATIONS];

	options.ambles = ambles;
	options.loops = loops;
	options.values = values;
	options.operations = operations;

	process_options(&options, argc, argv);

	print_ambles(options.preambles, options.preamble_count);

	if (options.status_block) {
		print_status_block(loops, "  ");
	} else {
		run_ramp(loops);
	}
	print_ambles(options.postambles, options.postamble_count);

	return 0;
}
