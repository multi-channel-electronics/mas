#ifndef _MCE_ERRORS_H_
#define _MCE_ERRORS_H_


#define MCE_ERR_BASE        +0x1000


#define MCE_ERR_FAILURE         (MCE_ERR_BASE + 0x0000)
#define MCE_ERR_HANDLE          (MCE_ERR_BASE + 0x0001)
#define MCE_ERR_DEVICE          (MCE_ERR_BASE + 0x0002)
#define MCE_ERR_FORMAT          (MCE_ERR_BASE + 0x0003)
#define MCE_ERR_REPLY           (MCE_ERR_BASE + 0x0004)
#define MCE_ERR_BOUNDS          (MCE_ERR_BASE + 0x0005)
#define MCE_ERR_CHKSUM          (MCE_ERR_BASE + 0x0006)
#define MCE_ERR_XML             (MCE_ERR_BASE + 0x0007)
#define MCE_ERR_READBACK        (MCE_ERR_BASE + 0x0008)
#define MCE_ERR_MULTICARD       (MCE_ERR_BASE + 0x0009)

#define MCE_ERR_NEED_CMD        (MCE_ERR_BASE + 0x000a)
#define MCE_ERR_NEED_DATA       (MCE_ERR_BASE + 0x000b)
#define MCE_ERR_NEED_CONFIG     (MCE_ERR_BASE + 0x000c)

#define MCE_ERR_CARDNOTFOUND    (MCE_ERR_BASE + 0x000d)
#define MCE_ERR_PARAMNOTFOUND   (MCE_ERR_BASE + 0x000e)

#define MCE_ERR_ACTIVE          (MCE_ERR_BASE + 0x0010)
#define MCE_ERR_NOT_ACTIVE      (MCE_ERR_BASE + 0x0011)
#define MCE_ERR_KERNEL          (MCE_ERR_BASE + 0x0012)
#define MCE_ERR_TIMEOUT         (MCE_ERR_BASE + 0x0013)

/* Interface subsystem error detail */
#define MCE_ERR_INT_UNKNOWN     (MCE_ERR_BASE + 0x0020)
#define MCE_ERR_INT_FAILURE     (MCE_ERR_BASE + 0x0021)
#define MCE_ERR_INT_BUSY        (MCE_ERR_BASE + 0x0022)
#define MCE_ERR_INT_TIMEOUT     (MCE_ERR_BASE + 0x0023)
#define MCE_ERR_INT_PROTO       (MCE_ERR_BASE + 0x0024)
#define MCE_ERR_INT_SURPRISE    (MCE_ERR_BASE + 0x0025)

/* Acquisition system errors */
#define MCE_ERR_FRAME_UNKNOWN   (MCE_ERR_BASE + 0x0030)
#define MCE_ERR_FRAME_TIMEOUT   (MCE_ERR_BASE + 0x0031)
#define MCE_ERR_FRAME_STOP      (MCE_ERR_BASE + 0x0032)
#define MCE_ERR_FRAME_LAST      (MCE_ERR_BASE + 0x0033)
#define MCE_ERR_FRAME_DEVICE    (MCE_ERR_BASE + 0x0034)
#define MCE_ERR_FRAME_OUTPUT    (MCE_ERR_BASE + 0x0035)
#define MCE_ERR_FRAME_CARD      (MCE_ERR_BASE + 0x0036)
#define MCE_ERR_FRAME_SIZE      (MCE_ERR_BASE + 0x0037)
#define MCE_ERR_FRAME_COUNT     (MCE_ERR_BASE + 0x0038)
#define MCE_ERR_FRAME_ROWS      (MCE_ERR_BASE + 0x0039)
#define MCE_ERR_FRAME_COLS      (MCE_ERR_BASE + 0x003a)

/* #define MCE_ERR_                MCE_ERR_BASE + 0x0001 */


#endif
