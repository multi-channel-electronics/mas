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


/* #define MCE_ERR_                MCE_ERR_BASE + 0x0001 */


#endif
