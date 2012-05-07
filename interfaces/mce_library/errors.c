/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdarg.h>

#include "mce/mce_errors.h"
#include "context.h"

int mcelib_warning(const mce_context_t *c, const char *fmt, ...)
{
    va_list ap;
    int ret;

    if (c->flags & MCELIB_QUIET)
        return 0;

    fputs("mcelib: Warning: ", stderr);
    va_start(ap, fmt);
    ret = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    return ret;
}

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

	case MCE_ERR_FRAME_UNKNOWN:
		return "Unknown acquisition system error.";
		
	case MCE_ERR_FRAME_TIMEOUT:
		return "Frame data timed-out.";
		
	case MCE_ERR_FRAME_STOP:
		return "Frame data was stopped.";
		
	case MCE_ERR_FRAME_LAST:
		return "Last frame received.";
		
	case MCE_ERR_FRAME_DEVICE:
		return "Acquisition stopped because of device read error.";
		
	case MCE_ERR_FRAME_OUTPUT:
		return "Acquisition stopped because of frame write error.";
		
	case MCE_ERR_FRAME_CARD:
		return "Acquisition does not support the specified card combination.";
		
	case MCE_ERR_FRAME_SIZE:
		return "Frame size is invalid or could not be configured.";
		
	case MCE_ERR_FRAME_COUNT:
		return "Frame count is invalid or could not be configured.";
		
	case MCE_ERR_FRAME_ROWS:
		return "Frame rows could not be determined or was invalid.";

	case MCE_ERR_FRAME_COLS:
		return "Frame columns could not be determined or was invalid.";

	case 0:
		return "Success.";
	}

	return "Unknown mce_library error.";
}
