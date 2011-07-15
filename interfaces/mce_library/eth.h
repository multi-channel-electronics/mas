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
extern int mcecmd_eth_read(mce_context_t, void*, size_t);
extern int mcecmd_eth_write(mce_context_t, const void*, size_t);

extern int mcedata_eth_connect(mce_context_t);
extern int mcedata_eth_disconnect(mce_context_t);
extern int mcedata_eth_ioctl(mce_context_t, unsigned long int, int);
extern int mcedata_eth_read(mce_context_t, void*, size_t);
#define mcedata_eth_write NULL

/* Ethernet packet filtering and service direction*/
struct _eth_transport;
typedef struct _eth_transport eth_transport;

extern int eth_set_service(eth_transport *transport, int service);
extern int eth_set_filter(int socket, int protocol, int type, int uid);
extern ssize_t eth_write(eth_transport *transport, int socket,
                  int proto, int type, int uid,
                  const void *data, int count);
extern ssize_t eth_read(int socket, void *data, int count);

#endif
