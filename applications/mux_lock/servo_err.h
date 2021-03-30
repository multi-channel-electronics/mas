/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/***********************************************************
 *    servo_err.h: for error handling
 * Revision history:
 * <date $Date: 2007/09/20 21:35:35 $>
 * $Log: servo_err.h,v $
 * Revision 1.1  2007/09/20 21:35:35  mce
 * MA initital release
 *
 ***********************************************************/
#include <stdio.h>

/* error handling defines */
#define ERROR_MSG_PREAMBLE "ERROR:"
#define ERR_OUTPUT stderr
#define ERRPRINT(s) fprintf(ERR_OUTPUT, "%s %s\n",ERROR_MSG_PREAMBLE,s);


// error codes
#define SUCCESS 0

//mas API errors
#define ERR_MCE_LCFG 100
#define ERR_MCE_OPEN 101
#define ERR_MCE_DATA 102
#define ERR_MCE_PARA 103
#define ERR_MCE_RB   104
#define ERR_MCE_WB   105
#define ERR_MCE_GO   106
#define ERR_MCE_ECFG 107

//
#define ERR_NUM_ARGS 1
#define ERR_DATA_DIR 2
#define ERR_SAFB_INI 4
#define ERR_INI_READ 5
#define ERR_S2FB_INI 6
#define ERR_DATA_FIL 7
#define ERR_DATA_ROW 8
#define ERR_UNIMPL   9

#define ERR_RUN_FILE 3
#define ERR_TMP_FILE 6

//char *err_msg[2]=
//{"Error loading config file", "Error openning mce device"};
