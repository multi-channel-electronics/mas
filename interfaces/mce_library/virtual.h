
int mcecmd_write_virtual(mce_context_t* context, const mce_param_t* param,
    int data_index, uint32_t *data, int count);

int mcecmd_read_virtual (mce_context_t* context, const mce_param_t* param,
    int data_index, uint32_t *data, int count);

int mcecmd_readwrite_banked (mce_context_t* context, const mce_param_t* param,
			     int data_index, uint32_t *data, int count, char rw);
