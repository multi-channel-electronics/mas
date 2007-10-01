#ifndef _MCE_H_
#define _MCE_H_

#include <linux/types.h>
#include <linux/unistd.h>

#ifndef __KERNEL__
typedef __u32 u32;
typedef __u16 u16;
#endif


/* The preamble, for reference */

#define PREAMBLE_0        0xa5a5a5a5
#define PREAMBLE_1        0x5a5a5a5a


/* Command words */

typedef enum {

	MCE_WB =          0x20205742,
        MCE_GO =          0x2020474f,
        MCE_ST =          0x20205354,
        MCE_RS =          0x20205253,
        MCE_RB =          0x20205242,

} mce_command_code;


/* 16 bit OK/ER (fate of 2020) */

#define MCE_OK            0x4f4b
#define MCE_ER            0x4552

/* That's right */

#define MCE_CMD_DATA_MAX 58
#define MCE_REP_DATA_MAX 58

#pragma pack(1)

typedef struct {

	u32 preamble[2];
	u32 command;
        u16 para_id;
	u16 card_id;
	u32 count;
	u32 data[MCE_CMD_DATA_MAX];
	u32 checksum;

} mce_command;


/* mce_reply
 *  Except for successful RB, data words are
 *        data[0] = errno (or 0 if ok)
 *        data[1] = chksum
 *  successful RB has
 *        data[0..(N-1)] = read results
 *        data[N] = checksum;
 */

typedef struct {

	u16 ok_er;
	u16 command;
	u16 para_id;
	u16 card_id;
	u32 data[MCE_REP_DATA_MAX];

} mce_reply;

typedef struct {

	u16 ok_er;
	u16 command;
	u16 para_id;
	u16 card_id;
	u32 error_code;
	u32 checksum;
	u32 zero[MCE_REP_DATA_MAX-2];

} mce_reply_simple;


#pragma pack()



#endif
