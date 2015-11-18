#include "ioctl_U0106_dsp.h"
#include "ioctl_U0106_mce.h"
#include "ioctl_U0106_data.h"

#define DSPIOCT_MOREMAGIC 'M'
#define DSPIOCT_GET_DRV_TYPE   _IOR(DSPIOCT_MOREMAGIC,  0xFF, int)
