#include "mcecmd.h"

struct cmd_str {
	char name[32];
	mce_command_code code;
	int  carded;
	int  paraed;
};

#define NCOM 6

struct cmd_str commands[NCOM] = {
	{"r",      MCE_RB, 0, 0},
	{"w",      MCE_WB, 0, 0},
	{"go",     MCE_GO, 0, 0},
	{"stop",   MCE_ST, 0, 0},
	{"reset",  MCE_RS, 0, 0},
};

/* Syntax:
 *      "command card para [args]"
 *
 */

static
int command_key(char *name) {
	int i;
	if (name == NULL) return -1;
	for (i=0; i<NCOM; i++)
		if ( strcmp(commands[i].name, name)==0) return i;
	return -1;
}


int process_command(char *line, char *errstr)
{	
	int i;

	if (line==NULL) {
		sprintf(errstr, "Null command line argument.");
		return -1;
	}

        //Determine key
	char *cmd = strtok(line, WHITE);
	char *card = strtok(NULL, WHITE);
	char *para = strtok(NULL, WHITE);
	int para_type = -1;
	
	if (cmd==NULL) cmd = "";

	int key = command_key(cmd);
	if (key < 0) {
		sprintf(errstr, "unknown command %s\n", cmd);
		return -1;	
	     
	if (card==NULL) {
		sprintf(errstr, "expected card string or number");
		return -1;
	}
	if (para==NULL) {
		sprintf(errstr, "expected param string or number");
		return -1;
	}
	
       
	//Attempt to convert card and para to integers, otherwise lookup
	param_properties_t props = {
		.flags = 0,
	};

	int raw_mode = ( get_int(card, &props.card_id)==0 &&
			 get_int(para, &props.para_id)==0 );

	if (!raw_mode) {
		//Extract trailing numerals from para as para_index
		int index = 0;
		char *s;
		for ( s=para; *s!=0 && ( *s<'0' || *s>'9'); s++);
		sscanf(s, "%i", &index);
		*s = 0;

		if (mce_lookup(handle, &props,
			       card, para, index) !=0) {
			sprintf(errstr, "Could not locate card:param:index "
				"= %s:%s:%i", card, para, index);
			return -1;
		}
	}


        // Rules for arguments: In raw mode, for all commands except
        // read_block, 0 to MCE_CMD_DATA_MAX arguments may be passed;
        // for read_block only a single argument is accepted and it is
        // the size of the read.

        // In non-raw mode:
        // rb: up to props.count args are accepted, but ignored.
        // wb: exactly props.count args must be given
        // other commands: up to props.count args accepted, but ignored

	int code = commands[key].code;
	char *name = commands[key].name;

	int exact_mode =
		(!raw_mode && (code==MCE_WB)) ||
		( raw_mode && (code==MCE_RB));
	
	if (raw_mode) {
		props.count = (code==MCE_RB) ?
			1 : MCE_CMD_DATA_MAX;
	}

	int data_count = 0;
	int bufr[MCE_REP_DATA_MAX];
	int bufw[MCE_CMD_DATA_MAX];
	char *token;
	while ( (token=strtok(NULL, WHITE)) !=NULL ) {
		if (data_count>=props.count) {
			sprintf(errstr, "too many arguments!");
			return -1;
		}
		if ( get_int(token, bufw+data_count++) !=0) {
			sprintf(errstr, "integer arguments expected!");
			return -1;
		}
	}
	if (exact_mode && data_count<props.count) {
		sprintf(errstr, "%i arguments expected.", props.count);
		return -1;
	}

	int to_read = raw_mode ? bufw[0] : props.count;

	// That will likely do it.
	int ret_val = 0;
	errstr[0] = 0;

	switch(code) {

	case MCE_RS:
		ret_val = mce_reset(handle, props.card_id, props.para_id);
		break;

	case MCE_GO:
		ret_val = mce_start_application(handle, props.card_id, props.para_id);
		break;

	case MCE_ST:
		ret_val = mce_stop_application(handle, props.card_id, props.para_id);
		break;

	case MCE_RB:
		//Check count in bounds
		if (to_read<0 || to_read>MCE_REP_DATA_MAX) {
			sprintf(errstr, "read length out of bounds!");
			return -1;
		}
		ret_val = mce_read_block(handle,
					 props.card_id, props.para_id,
					 to_read, bufr);

		if (ret_val==0) {
			char* next = errstr;
			for (i=0; i<to_read; i++) {
				next += sprintf(next, " %#x", bufr[i]);
			}
		}
		break;

	case MCE_WB:
		//Check bounds
		for (i=0; i<data_count; i++) {
			if ((props.flags & PARAM_MIN)
			    && bufw[i]<props.minimum) break;
			if ((props.flags & PARAM_MAX)
			    && bufw[i]<props.maximum) break;
		}
		if (i!=data_count) {
			sprintf(errstr,	"command expects values in range "
					"[%#x, %#x] = [%i,%i]\n",
				props.minimum, props.maximum,
				props.minimum, props.maximum);
			return -1;
		}

		ret_val = mce_write_block(handle,
					  props.card_id, props.para_id,
					  data_count, bufw);
		break;

	default:
		sprintf(errstr, "command '%s' not implemented\n",
			name);
		return -1;
	}

	return ret_val;
}
