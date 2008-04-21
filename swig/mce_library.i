/* mce_library.i */

/* This is just to get the mce_library functions into python; we
should provide a separate object oriented abstraction layer for the
various modules, and then a third layer for higher level
functionality. */

%module mce_library

%include carrays.i

%{

#include "../include/mce_library.h"

%}

/* Types we want to manipulate directly should be described */

%typedef unsigned u32;
%array_class(u32, u32array)


/* Prototypes to wrap */

%include "../include/mce_library.h"
%include "../include/mceconfig.h"
%include "../include/mcecmd.h"
%include "../include/mcedata.h"

%inline %{

	/* Expose the default config/device files */

	const char default_cmdfile[] = DEFAULT_CMDFILE;
	const char default_datafile[] = DEFAULT_DATAFILE;
	const char default_masfile[] = DEFAULT_MASFILE;
	const char default_hardwarefile[] = DEFAULT_HARDWAREFILE;
	const char default_experimentfile[] = DEFAULT_EXPERIMENTFILE;

	
	/* Frame-getter, using rambuff storage_t */
		
	int frame_callback(unsigned user_data, int size, u32 *data)
	{
		//Re-type 
		u32 *buf = (u32*)user_data;

	        memcpy(buf, data, size*sizeof(u32));
		return 0;
	}

	int read_frame(mce_context_t *context, u32 *buf, int cards) {
		mcedata_storage_t* ramb; 
		mce_acq_t acq;
		ramb = mcedata_rambuff_create( frame_callback, (unsigned)buf );
		mcedata_acq_create(&acq, context, 0, cards, -1, ramb);
		mcedata_acq_go(&acq, 1);
		mcedata_acq_destroy(&acq);
		return 0;
	}		

%}

int read_frame(mce_context_t *context, u32 *buf, int cards);

/* Class definitions */

