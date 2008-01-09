/* Production and analysis of MCE command and reply packets. */

#include <mce_library.h>

int mcecmd_load_command(mce_command *cmd, u32 command,
		     u32 card_id, u32 para_id, 
		     int count, int data_count, const u32 *data)
{
	int i;

	memset(cmd, 0, sizeof(*cmd));

	cmd->preamble[0] = PREAMBLE_0;
	cmd->preamble[1] = PREAMBLE_1;
	cmd->command = command;
	cmd->para_id = para_id;
	cmd->card_id = card_id;
	cmd->count = count;

	for (i=0; i<data_count && i<MCE_CMD_DATA_MAX; i++)
		cmd->data[i] = data[i];
	
	cmd->checksum = mcecmd_cmd_checksum(cmd);

	if ((data_count < 0 || data_count >= MCE_CMD_DATA_MAX)  ||
	    (count < 0      || count >= MCE_CMD_DATA_MAX))
		return -MCE_ERR_BOUNDS;

	return 0;
}


/* Specialty */

u32 mcecmd_cmd_checksum( const mce_command *cmd )
{
	u32 chksum = 0;
	u32 *p = (u32*) &cmd->command;
	while (p < (u32*) &cmd->checksum )
		chksum ^= *p++;
	return chksum;
}

u32 mcecmd_checksum( const u32 *data, int count )
{
	u32 chksum = 0;
	while (count>0)
		chksum ^= data[--count];	
	return chksum;
}


int mcecmd_cmd_match_rep( const mce_command *cmd, const mce_reply *rep )
{
	// Check sanity of response
	if ( (rep->command | 0x20200000) != cmd->command)
		return -MCE_ERR_REPLY;
	if (rep->para_id != cmd->para_id)
		return -MCE_ERR_REPLY;
	if (rep->card_id != cmd->card_id)
		return -MCE_ERR_REPLY;

	// Check success of command
	switch (rep->ok_er) {
	case MCE_OK:
		break;
	case MCE_ER:
		// Error code should be extracted; checksum; ...
		return -MCE_ERR_FAILURE;
	default:
		return -MCE_ERR_REPLY;
	}
	return 0;
}

