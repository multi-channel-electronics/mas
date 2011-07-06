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

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <sys/ioctl.h>


#define MCE_ETHER_PROTO      0xA0B0
#define MCE_ETHER_CODE_CMD   0x0001
#define MCE_ETHER_CODE_REP   0x0002
#define MCE_ETHER_CODE_DATA  0x0010


/* The MCE-over-ethernet header, based on real ethernet header
   (/usr/include/linux/if_ether.h) */

struct mce_ethhdr {
    unsigned char   h_dest[ETH_ALEN];       /* destination eth addr */
    unsigned char   h_source[ETH_ALEN];     /* source ether addr    */
    __be16          h_proto;                /* ethernet packet type ID field */
    __be16          h_type;                 /* MCE packet type field */
    __be32          h_uid;                  /* sender sub-ID */
} __attribute__((packed));

#define MCE_ETH_HLEN (sizeof(struct mce_ethhdr))


/* Our private data, stored in mce_context_t->transport */

struct _eth_transport {
    int cmd_socket;                // Socket for command-reply path
    int data_socket;               // Socket for data path
    int index;                     // Interface index
    int uid;                       // Our comm channel unique id (pid)
    struct mce_ethhdr header;      // Template packet header
    struct sockaddr_ll sockaddr;   // Template target description
};

/* eth_write - low-level shuttling of data in MCE-over-ethernet frame.

   The frame header is taken from the eth_transport default header.
   If proto, type or uid are non-zero, they will override the
   defaults.

   The count is the size of data payload in bytes.
*/

ssize_t eth_write(eth_transport *transport, int socket,
                  int proto, int type, int uid,
                  const void *data, int count)
{
    void *buffer = malloc(ETH_FRAME_LEN);
    struct mce_ethhdr *hdr = (struct mce_ethhdr*)buffer;

    memcpy(buffer, &transport->header, MCE_ETH_HLEN);
    if (proto != 0)
        hdr->h_proto = htons(proto);
    if (type != 0)
        hdr->h_type = htons(type);
    if (uid != 0)
        hdr->h_uid = htonl(uid);

    /* Check count */
    int eth_count = count + MCE_ETH_HLEN;
    if (eth_count > ETH_FRAME_LEN) {
        fprintf(stderr, "Maximum out-going ethernet frame size exceeded.\n");
        return -MCE_ERR_FORMAT;
    }

    /* Copy data and pad frame to minimum length */
    memcpy(buffer+MCE_ETH_HLEN, data, count);
    if (eth_count < ETH_ZLEN) {
        memset(buffer+eth_count, 0, ETH_ZLEN-eth_count);
        eth_count = ETH_ZLEN;
    }
    
    /* Send. */
    int send_result = sendto(socket, buffer, eth_count, 0, 
                             (struct sockaddr*)&transport->sockaddr,
                             sizeof(transport->sockaddr));
    free(buffer);

    if (send_result == -1) {
        fprintf(stderr, "Ethernet write failed with code %i\n", errno);
        return -MCE_ERR_INT_FAILURE;
    }
    /* All or nothing if you ask me. */
    if (send_result != eth_count ) {
        fprintf(stderr, "Ethernet partial write (%i of %i)\n", send_result, eth_count);
        return -MCE_ERR_INT_FAILURE;
    }

    return count;
}

/* eth_read - low-level getting of data in MCE-over-ethernet frame.

   Reads a frame from the socket associated with *transport into the
   buffer data, up to a maximum of count bytes.  Returns number of
   bytes copied.
*/

ssize_t eth_read(int socket, void *data, int count)
{
    /* Read directly into data if it's big enough for a whole frame. */
    void *buffer = NULL;
    void *target = data;
    if (count >= ETH_FRAME_LEN) {
        buffer = malloc(ETH_FRAME_LEN);
        target = buffer;
    }
    int n = recvfrom(socket, target, ETH_FRAME_LEN, 0, NULL, NULL);
    if (n == -1) {
        fprintf(stderr, "Ethernet read failed with code %i\n", errno);
        return -MCE_ERR_INT_FAILURE;
    }

    if (n < count)
        count = n;
    if (buffer != NULL) {
        if (count>0)
            memcpy(data, buffer, count);
        free(buffer);
    }
    return count;
}

/* eth_set_service - configure the transport as client or server.

   The service should be MCE_ETHER_CODE_CMD (client) or
   MCE_ETHER_CODE_REP (server).  Configures the socket filters and
   sets the default out-going MCE-over-ethernet packet type.
 */

int eth_set_service(eth_transport *transport, int service)
{
    int err;

    /* Note that data socket does not match on UID -- it will eat all data frames. */
    err = eth_set_filter(transport->data_socket, MCE_ETHER_PROTO,
                         MCE_ETHER_CODE_DATA, 0);
    switch(service) {
    case MCE_ETHER_CODE_CMD:
        transport->header.h_type = htons(MCE_ETHER_CODE_CMD);
        err = eth_set_filter(transport->cmd_socket, MCE_ETHER_PROTO,
                             MCE_ETHER_CODE_REP, transport->uid);
        return err;
    case MCE_ETHER_CODE_REP:
        transport->header.h_type = htons(MCE_ETHER_CODE_REP);
        err = eth_set_filter(transport->cmd_socket, MCE_ETHER_PROTO,
                             MCE_ETHER_CODE_CMD, 0);
        return err;
    }
    fprintf(stderr, "Invalid MCE-over-ethernet service request.\n");
    return -1;
}

/*
  The routines below are the interface to the main MCE library.
*/


/* CMD subsystem */
int mcecmd_eth_connect(mce_context_t context)
{
    int err = -1;
    struct ifreq ifr;

    // Allocate private data
    context->transport_data = NULL;
    eth_transport *transport = malloc(sizeof(*transport));
    
    // Create sockets for raw packet transfers
    transport->cmd_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    transport->data_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    // Determine network interface number for our device (dev_name)
    memset(&ifr, 0, sizeof(ifr));
    strncpy((char*)ifr.ifr_name, context->dev_name, IFNAMSIZ);
    err = ioctl(transport->cmd_socket, SIOCGIFINDEX, &ifr);
    if (err < 0) {
        fprintf(stderr, "Failed to determine ethernet interface number, (%i).\n", errno);
        abort();
    }
    transport->index = ifr.ifr_ifindex;

    // Determine mac address of our network interface
    err = ioctl(transport->cmd_socket, SIOCGIFHWADDR, &ifr);
    if (err < 0) {
        fprintf(stderr, "Failed to determine ethernet interface MAC address, (%i).\n", errno);
        abort();
    }
    memcpy(&transport->header.h_source, &ifr.ifr_hwaddr, ETH_ALEN);

    // Also store target mac address, which has been encoded as a uint64_t
    int i;
    for (i=0; i<ETH_ALEN; i++)
        transport->header.h_dest[i] = (context->dev_num >> (8*i)) & 0xff;

    // And the MCE ethernet protocol id.
    transport->header.h_proto = htons(MCE_ETHER_PROTO);

    // And our private token
    transport->uid = (uint32_t)getpid();
    printf("Got uid %x\n", transport->uid);
    transport->header.h_uid = htonl(transport->uid);

    // Make it good for commands by default
    eth_set_service(transport, MCE_ETHER_CODE_CMD);

    /* Fill out a sockaddr_ll (netpacket/packet.h) for later */
    struct sockaddr_ll *sa = &transport->sockaddr;
    sa->sll_family   = PF_PACKET;
    sa->sll_protocol = htons(ETH_P_ALL);
    sa->sll_ifindex  = transport->index;
    sa->sll_hatype   = ARPHRD_ETHER;
    sa->sll_pkttype  = PACKET_OTHERHOST;
    sa->sll_halen    = ETH_ALEN;
    memcpy(sa->sll_addr, transport->header.h_source, ETH_ALEN);
    
    // And we are good to go.
    context->transport_data = transport;

    return 0;
}

int mcecmd_eth_disconnect(mce_context_t context)
{
    eth_transport* transport = (eth_transport*)context->transport_data;
    if (transport != NULL) {
        if (transport->cmd_socket >= 0)
            shutdown(transport->cmd_socket, SHUT_RDWR);
        if (transport->data_socket >= 0)
            shutdown(transport->data_socket, SHUT_RDWR);
        free(transport);
        context->transport_data = NULL;
    }
    return 0;
}

int mcecmd_eth_ioctl(mce_context_t context, unsigned long int req, int arg)
{
    fprintf(stderr, "Unhandled IOCTL; %#lx %i\n", req, arg);
    return -1;
}

int mcecmd_eth_read(mce_context_t context, void *buf, size_t count)
{
    eth_transport* transport = (eth_transport*)context->transport_data;
    void* data = (void*)malloc(ETH_FRAME_LEN);

    //Read the request number of bytes, plus the header size.
    int n = eth_read(transport->cmd_socket, data, count+MCE_ETH_HLEN);
    if (n<0)
        return n;

    //Strip off the header and copy
    n -= MCE_ETH_HLEN;
    if (n > count)
        n = count;
    if (n < 0)
        n = 0;
    memcpy(buf, data+MCE_ETH_HLEN, n);
    if (n < count)
        memset(buf+n, 0, count-n);

    free(data);
    return count;
}

int mcecmd_eth_write(mce_context_t context, const void *buf, size_t count)
{
    eth_transport* transport = (eth_transport*)context->transport_data;
    return eth_write(transport, transport->cmd_socket, 0, 0, 0, buf, count);
}

/* DATA subsystem */
int mcedata_eth_connect(mce_context_t context)
{
    return 0;
}

int mcedata_eth_disconnect(mce_context_t context)
{
    return 0;
}

int mcedata_eth_ioctl(mce_context_t context, unsigned long int req, ...)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedata_eth_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedata_eth_write(mce_context_t context, const void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}


