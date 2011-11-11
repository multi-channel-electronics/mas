/*******************************************************

 manip.c - manipulate MCE command / reply data according
           to the parameter description.

*******************************************************/

#include <mce_library.h>

int mcecmd_prewrite_manip(const mce_param_t* param, u32* data, int count)
{
	int i;
	const param_t *p = &param->param;

	if (!(p->flags & MCE_PARAM_MANIP))
		return 0;

	for (i=0; i<count; i++) {
		data[i] ^= p->op_xor;
	}

	return 0;
}

int mcecmd_postread_manip(const mce_param_t* param, u32* data, int count)
{
	int i;
	const param_t *p = &param->param;
	if (!(p->flags & MCE_PARAM_MANIP))
		return 0;

	for (i=0; i<count; i++) {
		data[i] ^= p->op_xor;
	}

	return 0;
}

