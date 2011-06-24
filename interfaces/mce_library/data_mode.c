/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <mce/data_mode.h>

/* This is stupid cpp tricks, to be sure.

   The data modes are defined in data_mode.def.

   We use two sets of macros to parse the definition file; the first
   set first defines mce_data_field variables that describe the
   fields, and the second set just serve to form a comma delimited
   list of the mce_data_field variables for inclusion in the master
   list.
*/


/* The mce_data_field names.  e.g. mode0_ERROR */
#define DATA_FIELD(MODE, NAME) \
	mode ## MODE ## _ ## NAME

/* Variable declaration */
#define DATA_FIELD_DECLARE(MODE, NAME)				\
	static struct mce_data_field DATA_FIELD(MODE, NAME)

/* Macro set 1 */

#define DECLARE_RAW(MODE, NAME)				\
	DATA_FIELD_DECLARE(MODE, NAME) = {		\
			.data_mode = MODE,		\
			.type = DATA_MODE_RAW,		\
			.name = MCE_DATA_ ## NAME,	\
	};
		              
#define DECLARE_SCALE(MODE, NAME, SCALE)		\
	DATA_FIELD_DECLARE(MODE, NAME) = {		\
			.data_mode = MODE,		\
			.type = DATA_MODE_SCALE,	\
			.name = MCE_DATA_ ## NAME,	\
			.scalar = SCALE,		\
	};

#define DECLARE_EXTRACT(MODE, NAME, START, COUNT)	\
	DATA_FIELD_DECLARE(MODE, NAME) = {		\
			.data_mode = MODE,		\
			.type = DATA_MODE_EXTRACT,	\
			.name = MCE_DATA_ ## NAME,	\
			.bit_start = START,		\
			.bit_count = COUNT,		\
	};

#define DECLARE_EXTRACT_SCALE(MODE, NAME, START, COUNT, SCALE)	\
	DATA_FIELD_DECLARE(MODE, NAME) = {			\
			.data_mode = MODE,		\
			.type = DATA_MODE_EXTRACT_SCALE, \
			.name = MCE_DATA_ ## NAME,	\
			.bit_start = START,		\
			.bit_count = COUNT,		\
			.scalar = SCALE,		\
	};


/* Define mode0_error, etc... */

#include "data_mode.def"


/* Reset... */

#undef DECLARE_RAW
#undef DECLARE_SCALE
#undef DECLARE_EXTRACT
#undef DECLARE_EXTRACT_SCALE


/* Macro set 2 */

#define DECLARE_RAW(MODE, NAME) \
	& DATA_FIELD(MODE, NAME),
#define DECLARE_EXTRACT(MODE, NAME, START, COUNT) \
	& DATA_FIELD(MODE, NAME),
#define DECLARE_SCALE(MODE, NAME, SCALE) \
	& DATA_FIELD(MODE, NAME),
#define DECLARE_EXTRACT_SCALE(MODE, NAME, START, COUNT, SCALE) \
	& DATA_FIELD(MODE, NAME),


/* Define the list of data fields, NULL terminated. */

struct mce_data_field* mce_data_fields[] = {

#include "data_mode.def"

	NULL,
};
