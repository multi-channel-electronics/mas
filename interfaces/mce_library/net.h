/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef MCELIB_NET_H
#define MCELIB_NET_H

#include <mce_library.h>

/* context negotation */
int mcenet_hello(mce_context_t context);

/* mcenet client methods */
extern int mcecmd_net_connect(mce_context_t);
extern int mcecmd_net_disconnect(mce_context_t);
extern int mcecmd_net_ioctl(mce_context_t, unsigned long int, int);
extern ssize_t mcecmd_net_read(mce_context_t, void*, size_t);
extern ssize_t mcecmd_net_write(mce_context_t, const void*, size_t);

extern int mcedata_net_connect(mce_context_t);
extern int mcedata_net_disconnect(mce_context_t);
extern int mcedata_net_ioctl(mce_context_t, unsigned long int, ...);
extern ssize_t mcedata_net_read(mce_context_t, void*, size_t);
extern ssize_t mcedata_net_write(mce_context_t, const void*, size_t);

extern int mcedsp_net_connect(mce_context_t);
extern int mcedsp_net_disconnect(mce_context_t);
extern int mcedsp_net_ioctl(mce_context_t, unsigned long int, ...);
extern ssize_t mcedsp_net_read(mce_context_t, void*, size_t);
extern ssize_t mcedsp_net_write(mce_context_t, const void*, size_t);

#endif
