/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mce_library.h>

#include "ramp.h"
#include "options.h"

int main(int argc, char** argv)
{
	options_t options;
	memset(&options, 0, sizeof(options));
    options.fibre_card = MCE_DEFAULT_MCE;

	char ambles[MAX_AMBLES][LINE_LEN];

	// If we don't zero these, it's segfault city.
	options.ambles = ambles;
	options.loops = calloc(MAX_LOOPS, sizeof(*options.loops));
	options.values = calloc(MAX_VALUES, sizeof(*options.values));
	options.operations = calloc(MAX_VALUES, sizeof(*options.operations));

	process_options(&options, argc, argv);

	print_ambles(options.preambles, options.preamble_count);

	if (options.status_block) {
		print_status_block(options.loops, "  ");
	} else {
		run_ramp(options.loops);
	}
	print_ambles(options.postambles, options.postamble_count);

	return 0;
}
