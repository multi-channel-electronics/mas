/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/* mce_library.i */

/* This is just to get the mce_library functions into python; we
should provide a separate object oriented abstraction layer for the
various modules, and then a third layer for higher level
functionality. */

%module mce_library

%include carrays.i

%{

#include "../include/mce_library.h"
#include "../include/mcedata.h"
#include "../defaults/config.h"
#include "../include/mce/defaults.h"

%}

/* Types we want to manipulate directly should be described */
 
%typedef uint32_t u32;
%array_class(u32, u32array)
%array_class(int, intarray)

/* Prototypes to wrap */

%include "stdint.i"
%include "../include/mce_library.h"
%include "../include/mceconfig.h"
%include "../include/mcecmd.h"
%include "../include/mcedata.h"
%include "../include/mce/defaults.h"

%inline %{
    /* Expose the default config/device files */

  const int multicard_mas = MULTICARD;

    typedef struct {
        int index;
        u32 *buf;
        int select;
        u32 *channels;
        int n_channels;
    } frame_handler_t;


    /* Frame-getter, using rambuff storage_t */

    int frame_callback(unsigned long user_data, int size, u32 *data)
    {
        int i;

        //Re-type
        frame_handler_t *f = (frame_handler_t*)user_data;

        if (f->select) {
            for (i=0; i< f->n_channels; i++) {
                f->buf[f->index * f->n_channels + i] = data[f->channels[i]];
            }
        } else {
                memcpy(f->buf + f->index * size, data, size*sizeof(u32));
        }
        f->index++;
        return 0;
    }

    int read_frames(mce_context_t *context, u32 *buf, int cards, int count) {
        frame_handler_t f;
        f.buf = buf;
        f.index = 0;
        f.select = 0;
        mcedata_storage_t* ramb;
        mce_acq_t acq;
        ramb = mcedata_rambuff_create( frame_callback, (unsigned long)&f );
        mcedata_acq_create(&acq, context, 0, cards, -1, ramb);
        mcedata_acq_go(&acq, count);
        mcedata_acq_destroy(&acq);
        return 0;
    }

    int read_channels(mce_context_t *context, u32 *buf, int cards, int count,
              u32 *channels, int n_channels) {
        frame_handler_t f;
        f.buf = buf;
        f.index = 0;
        f.select = 1;
        f.channels = channels;
        f.n_channels = n_channels;

        mcedata_storage_t* ramb;
        mce_acq_t acq;
        ramb = mcedata_rambuff_create( frame_callback, (unsigned long)&f );
        mcedata_acq_create(&acq, context, 0, cards, -1, ramb);
        mcedata_acq_go(&acq, count);
        mcedata_acq_destroy(&acq);
        return 0;
    }

  int u32_to_int(int *dst, u32 *src, int n) {
        int i;
        for (i=0; i<n; i++) {
      dst[i] = (int)src[i];
        }
        return 0;
    }

    u32* u32_from_int_p(int *i) {
            return (u32*)i;
    }

%}

/* Class definitions */

