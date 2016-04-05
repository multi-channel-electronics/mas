/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <string.h>

#include "context.h"
#include "virtual.h"

int mcecmd_write_virtual(mce_context_t* context, const mce_param_t* param,
        int data_index, uint32_t* data, int count)
{
	maprange_t mr;
	mce_param_t child;
	int i, error;
	int data_start, data_stop;

	if (param->card.nature != MCE_NATURE_VIRTUAL) return -1;
	
	for (i=0; i<param->param.map_count; i++) {
		error = mceconfig_param_maprange(
			context, &param->param, i, &mr);
		if (error) return error;

		data_start = data_index;
		data_stop = data_index + count;

		if (mr.start > data_start)
			data_start = mr.start;

		if (mr.start + mr.count < data_stop)
			data_stop = mr.start + mr.count;

		if (data_start >= data_stop)
			continue;

		error = mceconfig_lookup(context, mr.card_name, mr.param_name,
					  &child.card, &child.param);
		if (error) return error;

		error = mcecmd_write_range(context, &child,
					   mr.offset + (data_start - mr.start),
					   data + (data_start - data_index),
					   data_stop - data_start);
		if (error) return error;
	}
	return 0;
}


int mcecmd_read_virtual(mce_context_t* context, const mce_param_t* param,
        int data_index, uint32_t* data, int count)
{
	maprange_t mr;
	mce_param_t child;
	int i, error;
	int data_start, data_stop;

	if (param->card.nature != MCE_NATURE_VIRTUAL) return -1;

	memset(data, 0, count*sizeof(*data));

	for (i=0; i<param->param.map_count; i++) {
		error = mceconfig_param_maprange(
			context, &param->param, i, &mr);
		if (error) return error;

		data_start = data_index;
		data_stop = data_index + count;

		if (mr.start > data_start)
			data_start = mr.start;

		if (mr.start + mr.count < data_stop)
			data_stop = mr.start + mr.count;

		if (data_start >= data_stop)
			continue;

		error = mceconfig_lookup(context, mr.card_name, mr.param_name,
					  &child.card, &child.param);
		if (error) return error;

		error = mcecmd_read_range(context, &child,
					  mr.offset + (data_start - mr.start),
					  data + (data_start - data_index),
					  data_stop - data_start);
		if (error) return error;
	}
	return 0;
}


/* Bank Scheme 1 - this is like a low-level hard-coded virtual card
   mapping.

   Starting with fw_rev 6.0.0, many per-row parameters need to accept
   64 values.  But due to various deep-seated protocol limitations it
   is not straightforward to read or write more than 55 words.  "Bank
   Scheme 1" allows us to directly to access a card's parameters
   starting at index 32, by passing the card_id+0x10 instead of
   card_id.  For example, to fill "rc1 gaini0" (card_id=0x3,
   param_id=0x78) with the 64 values 0,1,...63, we would do:

   * rc1 gaini0 is card=0x3 param=0x78
   * wb 0x03 0x78   0  1  2  3 ... 31
   * wb 0x13 0x78  32 33 34 35 ... 63

*/

#define BANK1_SPLIT_IDX      32   /* Absolute index at which the upper
                                     bank access begins. */
#define BANK1_CARD_SHIFT   0x10   /* What to add to the card_id to
                                     access the upper bank. */
#define BANK1_ACTIVATE_IDX   55   /* Threshold for actually making use
                                     of the upper bank; i.e. at what
                                     index _must_ we use the upper
                                     bank to obtain access? */

int mcecmd_readwrite_banked(mce_context_t* context, const mce_param_t* param,
                            int data_index, uint32_t* data, int count, char rw)
{
    int i, error = 0;

    /* Whatever happens, we'll want one of these. */
	mce_param_t child;
    memcpy(&child, param, sizeof(child));
    child.param.bank_scheme = 0;

    if (rw != 'w')
        memset(data, 0, count*sizeof(*data));

   /* Absolute indices into the data indicating start, split point,
       and end of desired range. */
    int index_lo = data_index;
    int index_mi = BANK1_SPLIT_IDX;
    int index_hi = data_index + count;

    /* Make sure lo <= mi <= hi in what follows... */
    if (index_mi < index_lo)
        index_mi = index_lo;
    
    /* Don't always split them.  Do a single command if it will fit into the lo bank. */
    if (index_lo < BANK1_SPLIT_IDX && index_hi < BANK1_ACTIVATE_IDX)
        index_mi = index_hi;
    
    int count_lo = index_mi - index_lo;
    int count_hi = index_hi - index_mi;
    
    /* Read / write lower bank */
    if (count_lo) {
        if (rw == 'w')
            error = mcecmd_write_range(context, &child, index_lo, data, count_lo);
        else
            error = mcecmd_read_range (context, &child, index_lo, data, count_lo);
    }
    
    if (!error && count_hi) {
        /* Activate upper bank */
        for (i=0; i<child.card.card_count; i++)
            child.card.id[i] += BANK1_CARD_SHIFT;
        /* Offset indices as required. */
        if (rw == 'w')
            error = mcecmd_write_range(context, &child, index_mi - BANK1_SPLIT_IDX,
                                       data+count_lo, count_hi);
        else
            error = mcecmd_read_range (context, &child, index_mi - BANK1_SPLIT_IDX,
                                       data+count_lo, count_hi);
    }

	return error;
}
