#ifndef _MCECOMMS_H_
#define _MCECOMMS_H_

#include "frames.h"

typedef struct mce_comms_struct {

	char hardware_file[MEDLEN];

	char seq_card_str[SHORTLEN];
	char seq_cmd_str[SHORTLEN];
	char go_cmd_str[SHORTLEN];

	int seq_card_id;
	int seq_cmd_id;

	char mcecmd_name[MEDLEN];
	char mcedata_name[MEDLEN];
	int  mcecmd_handle;
	int  mcedata_fd;

} mce_comms_t;


int command_sequence(mce_comms_t *mce, int first, int last);
int command_go(mce_comms_t *mce, frame_setup_t *f);
int command_stop(mce_comms_t *mce, frame_setup_t *f);


#endif
