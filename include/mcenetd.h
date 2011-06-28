#ifndef MCENETD_H
#define MCENETD_H

#include<mce_library.h>

/* ports */
#define MCENETD_CTLPORT 1111
#define MCENETD_DSPPORT 1221
#define MCENETD_CMDPORT 2112
#define MCENETD_DATPORT 2222

/* protocol operations */
#define MCENETD_STOP  0x00
#define MCENETD_HELLO 0x01
#define MCENETD_READY 0x02

/* functions */
int mcenet_hello(mce_context_t context);

#endif
