/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include "ioctl_U0106_dsp.h"
#include "ioctl_U0106_mce.h"
#include "ioctl_U0106_data.h"
#include "ioctl_U0107.h"

#define DSPIOCT_MOREMAGIC 'M'
#define DSPIOCT_GET_DRV_TYPE   _IOR(DSPIOCT_MOREMAGIC,  0xFF, int)
