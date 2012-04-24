/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <string.h>

#include "context.h"
#include "virtual.h"

int mcecmd_write_virtual(mce_context_t* context, const mce_param_t* param,
			 int data_index, u32* data, int count)
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
			int data_index, u32* data, int count)
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
