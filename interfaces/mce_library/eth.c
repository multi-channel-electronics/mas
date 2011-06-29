/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <mce_library.h>
#include "context.h"

#include "eth.h"

#include <arpa/inet.h>
#include <sys/ioctl.h>
//#include <net/if.h>

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <sys/ioctl.h>

/* Our private data */

typedef struct {
    int socket;
    struct sockaddr source_mac;
    struct sockaddr dest_mac;
} eth_transport;



/* CMD subsystem */
int mcecmd_eth_connect(mce_context_t context)
{
    int err = -1;
    struct ifreq ifr;

    // Allocate private data
    context->transport_data = NULL;
    eth_transport *transport = malloc(sizeof(*transport));
    
    // Create socket for raw packet transfers
    transport->socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    // Determine network interface number for our device (dev_name)
    memset(&ifr, 0, sizeof(ifr));
    strncpy((char*)ifr.ifr_name, context->dev_name, IFNAMSIZ);
    err = ioctl(transport->socket, SIOCGIFINDEX, &ifr);
    if (err < 0) {
        fprintf(stderr, "Failed to determine ethernet interface number, (%i).\n", errno);
        abort();
    }

    // Determine mac address of our network interface
    err = ioctl(transport->socket, SIOCGIFHWADDR, &ifr);
    if (err < 0) {
        fprintf(stderr, "Failed to determine ethernet interface MAC address, (%i).\n", errno);
        abort();
    }
    memcpy(&transport->source_mac, &ifr.ifr_hwaddr, sizeof(ifr.ifr_hwaddr));

    // Also store target mac address, which has been encoded as a uint64_t
    int i;
    for (i=0; i<sizeof(context->dev_num); i++)
        transport->dest_mac.sa_data[i] = (context->dev_num >> (8*i)) & 0xff;

    // Is that good enough?
    context->transport_data = transport;

    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcecmd_eth_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcecmd_eth_ioctl(mce_context_t context, unsigned long int req, int arg)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcecmd_eth_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcecmd_eth_write(mce_context_t context, const void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

/* DATA subsystem */
int mcedata_eth_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedata_eth_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedata_eth_ioctl(mce_context_t context, unsigned long int req, ...)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedata_eth_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedata_eth_write(mce_context_t context, const void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}
