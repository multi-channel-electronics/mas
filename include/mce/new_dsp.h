#ifndef _DSP_H_
#define _DSP_H_

#include <linux/types.h>

#define DSP_COMMAND_SIZE 128
#define DSP_DATAGRAM_BUFFER_SIZE 128

// Flags for dsp_command
#define DSP_EXPECT_DSP_REPLY 0x01
#define DSP_EXPECT_MCE_REPLY 0x02
#define DSP_IGNORE_DSP_REPLY 0x04

#define DSP_DEFAULT_TIMEOUT_US 1000000

#pragma pack(push,1)
struct dsp_command {
	// Internal; decoded by driver.
	__s32 size;
	__s32 flags;
	__s32 owner;
	__s32 timeout_us;
	// Payload; sent to DSP
	__s16 cmd;
	__s16 data_size;
	__s32 data[DSP_COMMAND_SIZE];
};

struct dsp_datagram {
	__s16 version;
	__s16 total_size;
	__s16 type;
	__s16 unused1;
	__s32 fw_version;
	__s16 unused2[10];
	__s32 buffer[DSP_DATAGRAM_BUFFER_SIZE];
};

struct dsp_reply {
	__s16 err;
	__s16 size;
	__s16 cmd;
	__s16 unused1;
	__s32 data[DSP_COMMAND_SIZE];
};

struct mce_reply {
	__s16 type;
	__s16 _spaces;
	__s32 size;
	__s32 data[DSP_COMMAND_SIZE];
};

#pragma pack(pop)

#define DSP_REPLY(datagramp) ((struct dsp_reply*)(&((datagramp)->buffer)))
#define MCE_REPLY(datagramp) ((struct mce_reply*)(&((datagramp)->buffer)))


/* Codes for dsp_command.cmd; must match firmware. */
#define DSP_CMD_READ_P         0x01
#define DSP_CMD_READ_X         0x02
#define DSP_CMD_READ_Y         0x03
#define DSP_CMD_WRITE_P        0x05
#define DSP_CMD_WRITE_X        0x06
#define DSP_CMD_WRITE_Y        0x07
#define DSP_CMD_SET_REP_BUF    0x09
#define DSP_CMD_SET_DATA_MULTI 0x0B
#define DSP_CMD_SET_TAIL_INF   0x12
#define DSP_CMD_SEND_MCE       0x21
#define DSP_CMD_POST_MCE       0x22


/* type codes for dsp_datagram */
#define DGRAM_TYPE_DSP_REP    1
#define DGRAM_TYPE_MCE_REP    2
#define DGRAM_TYPE_BUF_INFO   3


#endif /* _DSP_H_ */


