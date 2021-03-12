
int mcecmd_write_virtual(mce_context_t* context, const mce_param_t* param,
			 int data_index, const u32 *data, int count);

int mcecmd_read_virtual (mce_context_t* context, const mce_param_t* param,
			 int data_index, u32 *data, int count);
