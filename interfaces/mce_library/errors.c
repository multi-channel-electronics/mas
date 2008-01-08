#include "mce_errors.h"

char *mcelib_error_string(int error)
{
	switch (-error) {

	case MCE_ERR_FAILURE:
		return "MCE replied with ERROR.";

	case MCE_ERR_HANDLE:
		return "Bad mce_library handle.";

	case MCE_ERR_DEVICE:
		return "I/O error on driver device file.";

	case MCE_ERR_FORMAT:
		return "Bad packet structure.";

	case MCE_ERR_REPLY:
		return "Reply does not match command.";

	case MCE_ERR_BOUNDS:
		return "Data size is out of bounds.";

	case MCE_ERR_CHKSUM:
		return "Checksum failure.";

	case MCE_ERR_XML:
		return "Configuration file not loaded.";

	case MCE_ERR_READBACK:
		return "Read-back write check failed.";

	case MCE_ERR_MULTICARD:
		return "Function does not have multi-card support.";

	case MCE_ERR_NEED_CMD:
		return "Command connection not established.";

	case MCE_ERR_NEED_DATA:
		return "Data connection not established.";

	case MCE_ERR_NEED_CONFIG:
		return "Config connection not established.";

	case MCE_ERR_ACTIVE:
		return "Command not issued because previous command is outstanding.";

	case MCE_ERR_NOT_ACTIVE:
		return "Reply not available because no command has been issued.";

	case MCE_ERR_KERNEL:
		return "Driver failed on routine kernel call.";

	case MCE_ERR_TIMEOUT:
		return "Timed-out waiting for MCE reply.";

	case MCE_ERR_INT_UNKNOWN:
		return "Unexpected interface (DSP) error.";

	case MCE_ERR_INT_FAILURE:
		return "Interface (DSP) command failed.";

	case MCE_ERR_INT_BUSY:
		return "Interface (DSP) is busy.";

	case MCE_ERR_INT_TIMEOUT:
		return "Interface (DSP) timed-out during a command.";

	case MCE_ERR_INT_PROTO:
		return "Interface (DSP) returned unexpected data.";

	case MCE_ERR_INT_SURPRISE:
		return "Unexpected packet notification from interface (DSP).";

	case 0:
		return "Success.";
	}

	return "Unknown mce_library error.";
}
