#include <stdlib.h>
#include <mcecmd.h>
#include "mcecomms.h"

int command_sequence(mce_comms_t *mce, int first, int last)
{
	// W CC RET_DAT_S <first> <last>
	u32 data[2] = { first, last };
//		       p->frame_setup.seq_last};
	return mce_write_block(mce->mcecmd_handle,
			       mce->seq_card_id,
			       mce->seq_cmd_id,
			       2, data);
}

int command_go(mce_comms_t *mce, frame_setup_t *f)
{
	column_config_t *c = &f->colcfgs[f->colcfg_index];

	// GO RC# RET_DAT 1
	return mce_start_application(mce->mcecmd_handle,
				     c->card_id, c->para_id);
}

int command_stop(mce_comms_t *mce, frame_setup_t *f)
{
	column_config_t *c = &f->colcfgs[f->colcfg_index];

	// STOP RC# RET_DAT
	return mce_stop_application(mce->mcecmd_handle,
				    c->card_id, c->para_id);
}


