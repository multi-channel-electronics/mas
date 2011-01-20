/****************************************************************************/
/*																			*/
/*	Module:			jamcomp.h												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Contains the function prototypes for compressing		*/
/*					and uncompressing Boolean array data.					*/
/*																			*/
/****************************************************************************/

#ifndef INC_JAMCOMP_H
#define INC_JAMCOMP_H

int32_t jam_uncompress
(
	char *in, 
	int32_t in_length, 
	char *out, 
	int32_t out_length,
	int version
);

#endif /* INC_JAMCOMP_H */

