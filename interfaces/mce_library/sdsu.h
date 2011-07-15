/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef MCELIB_SDSU_H
#define MCELIB_SDSU_H

#include <mce_library.h>

/* sdsu fibre card methods */
extern int mcecmd_sdsu_connect(mce_context_t);
extern int mcecmd_sdsu_disconnect(mce_context_t);
extern int mcecmd_sdsu_ioctl(mce_context_t, unsigned long int, int);
extern int mcecmd_sdsu_read(mce_context_t, void*, size_t);
extern int mcecmd_sdsu_write(mce_context_t, const void*, size_t);

extern int mcedata_sdsu_connect(mce_context_t);
extern int mcedata_sdsu_disconnect(mce_context_t);
extern int mcedata_sdsu_ioctl(mce_context_t, unsigned long int, int);
extern int mcedata_sdsu_read(mce_context_t, void*, size_t);
#define mcedata_sdsu_write NULL

extern int mcedsp_sdsu_connect(mce_context_t);
extern int mcedsp_sdsu_disconnect(mce_context_t);
extern int mcedsp_sdsu_ioctl(mce_context_t, unsigned long int, int);
extern int mcedsp_sdsu_read(mce_context_t, void*, size_t);
extern int mcedsp_sdsu_write(mce_context_t, const void*, size_t);

#endif
