#ifndef _DSPIOCTL_H_
#define _DSPIOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* IOCTL stuff */

#define DSPIOCT_MAGIC 't'
#define DSPIOCT_CMD0   _IO(DSPIOCT_MAGIC,  0)
#define DSPIOCT_CMD1   _IO(DSPIOCT_MAGIC,  1)
#define DSPIOCT_CMD2   _IO(DSPIOCT_MAGIC,  2)
#define DSPIOCT_CMD3   _IO(DSPIOCT_MAGIC,  3)
#define DSPIOCT_CMD4   _IO(DSPIOCT_MAGIC,  4)


#define DSPIOCT_R_HRXS   _IOR(DSPIOCT_MAGIC,  128, int)
#define DSPIOCT_R_HSTR   _IOR(DSPIOCT_MAGIC,  129, int)
#define DSPIOCT_R_HCVR   _IOR(DSPIOCT_MAGIC,  130, int)
#define DSPIOCT_R_HCTR   _IOR(DSPIOCT_MAGIC,  131, int)
#define DSPIOCT_W_HTXR   _IOW(DSPIOCT_MAGIC,  132, int)
#define DSPIOCT_W_HCVR   _IOW(DSPIOCT_MAGIC,  133, int)
#define DSPIOCT_W_HCTR   _IOW(DSPIOCT_MAGIC,  134, int)

#define DSPIOCT_COMMAND  _IOW(DSPIOCT_MAGIC,  140, int)
#define DSPIOCT_REPLY    _IOR(DSPIOCT_MAGIC,  141, int)

#define DSPIOCT_SET_REP_BUF  _IO(DSPIOCT_MAGIC,  150)
#define DSPIOCT_DUMP_BUF     _IO(DSPIOCT_MAGIC,  151)
#define DSPIOCT_TRIGGER_FAKE _IO(DSPIOCT_MAGIC,  152)

#define DSP_COMMAND_SIZE 128
#define DSP_DATAGRAM_BUFFER_SIZE 128

#define DSP_EXPECT_DSP_REPLY 0x01
#define DSP_EXPECT_MCE_REPLY 0x02

#pragma pack(push,1)
struct dsp_command {
	__s32 size;
	__s32 flags;
	__s32 data[DSP_COMMAND_SIZE];
};

struct dsp_reply {
	__s32 size;
	__s32 data[DSP_COMMAND_SIZE];
};

struct dsp_datagram {
	__s16 version;
	__s16 buffer_size;
	__s32 unused1[15];
	__s32 buffer[DSP_DATAGRAM_BUFFER_SIZE];
};

#pragma pack(pop)

/* All reply codes below DSP_CODE_REPLY_MAX are assumed to be replies
 * to commands send by the driver. */
#define DSP_CODE_REPLY_MAX  0x64



#endif

