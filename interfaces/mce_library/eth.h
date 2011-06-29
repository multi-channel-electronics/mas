/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef MCELIB_ETH_H
#define MCELIB_ETH_H

#include <mce_library.h>

/* ethernet methods */
extern int mcecmd_eth_connect(mce_context_t);
extern int mcecmd_eth_disconnect(mce_context_t);
extern int mcecmd_eth_ioctl(mce_context_t, unsigned long int, int);
extern ssize_t mcecmd_eth_read(mce_context_t, void*, size_t);
extern ssize_t mcecmd_eth_write(mce_context_t, const void*, size_t);

extern int mcedata_eth_connect(mce_context_t);
extern int mcedata_eth_disconnect(mce_context_t);
extern int mcedata_eth_ioctl(mce_context_t, unsigned long int, ...);
extern ssize_t mcedata_eth_read(mce_context_t, void*, size_t);
extern ssize_t mcedata_eth_write(mce_context_t, const void*, size_t);

extern int mcedsp_eth_connect(mce_context_t);
extern int mcedsp_eth_disconnect(mce_context_t);
extern int mcedsp_eth_ioctl(mce_context_t, unsigned long int, ...);
extern ssize_t mcedsp_eth_read(mce_context_t, void*, size_t);
extern ssize_t mcedsp_eth_write(mce_context_t, const void*, size_t);

#endif
