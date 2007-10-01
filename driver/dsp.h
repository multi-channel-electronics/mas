#ifndef _DSP_H_
#define _DSP_H_

#include <linux/types.h>

#ifndef __KERNEL__
#  define u32 __u32
#endif


#define DSP_DATAMASK 0x00ffffff

typedef enum 
{

	DSP_RDM  = 0x0052444d,   // "RDM" READ memory 
	DSP_WRM  = 0x0057524d,   // "WRM" WRITE memory 
	DSP_GOA  = 0x00474f41,   // "GOA" Application GO 
	DSP_STP  = 0x00535450,   // "STP" Application STOP 
	DSP_RST  = 0x00525354,   // "RST" PCI board software RESET 
	DSP_CON  = 0x00434f4e,   // "CON" Data packet to CONTROLLER 
	DSP_HST  = 0x00485354,   // "HST" Data packet to HOST 
	DSP_RCO  = 0x0052434f    // "RCO" RESET 

} dsp_command_code;


typedef enum
{

	DSP_MEMX = 0x00005f58, // "_X"  - dsp X memory
	DSP_MEMY = 0x00005f59, // "_Y"  -     Y
	DSP_MEMP = 0x00005f50, // "_P"  -     P

} dsp_memory_code;


typedef enum
{

	DSP_REP  = 0x00524550, // "REP" - dsp command reply
	DSP_NFY  = 0x004E4659, // "NFY" - mce packet has arrived

} dsp_message_code;


typedef enum
{

	DSP_ERR  = 0x00455252, // "ERR" - dsp command error
	DSP_ACK  = 0x0041434b, // "ACK" - dsp command completed

} dsp_reply_code;


typedef enum
{
	
	DSP_RP   = 0x5250, // "RP"  - mce command reply
	DSP_DA   = 0x4441, // "DA"  - mce data packet

} dsp_notify_code;


typedef struct {

	dsp_command_code command;
	u32 args[3];

} dsp_command;


typedef struct {

	dsp_message_code type;
	u32              command;
	u32              reply;
	u32              data;

} dsp_message;

typedef struct {

	dsp_message_code type;
	dsp_notify_code  code;
	u32              size_lo;  //bits 15-0
	u32              size_hi;  //bits 31-16

} dsp_notification;

#endif
