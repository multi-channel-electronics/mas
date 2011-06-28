/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <mcenetd.h>
#include <mce_library.h>
#include <poll.h>
#include "context.h"

/* poll a socket for an event */
int mcenet_poll(int fd, short event, const char *name)
{
    struct pollfd pfd = { fd, event, 0 };

    int r = poll(&pfd, 1, 1000);
    if (r == -1) {
        perror("mcenet: poll");
        return 1;
    } else if (r == 0) {
        fprintf(stderr, "mcenet: timout waiting for %s\n", name);
        return 1;
    } else if (pfd.revents & POLLERR) {
        fprintf(stderr, "mcenet: error waiting for %s\n", name);
        return 1;
    } else if (pfd.revents & POLLHUP) {
        fprintf(stderr, "mcenet: connection dropped to %s\n", name);
        return 1;
    }

    return 0;
}

/* send a request on the control channel, the response is stored in message */
ssize_t mcenet_req(mce_context_t context, unsigned char *message, size_t len)
{
    ssize_t n;

#if 1
    size_t i;
    fprintf(stderr, "mcenet: req -> %s:", context->dev_name);
    for (i = 0; i < len; ++i)
        fprintf(stderr, " %02hhx", message[i]);
    fprintf(stderr, "\n");
#endif

    /* wait for the net.socket to become ready */
    if (mcenet_poll(context->net.sock, POLLOUT, context->dev_name))
        return -1;

    /* write the request */
    n = write(context->net.sock, message, len);
    if (n < len) {
        fprintf(stderr, "mcenet: short write to %s\n", context->dev_name);
        return -1;
    }

    /* wait for a response */
    if (mcenet_poll(context->net.sock, POLLIN, context->dev_name))
        return -1;

    /* read the response */
    n = mcenet_readmsg(context->net.sock, message);

#if 1
    fprintf(stderr, "mcenet: rsp <- %s:", context->dev_name);
    for (i = 0; i < (size_t)n; ++i)
        fprintf(stderr, " %02hhx", message[i]);
    fprintf(stderr, "\n");
#endif

    return n;
}

/* initialise a connection */
int mcenet_hello(mce_context_t context)
{
    int l;
    unsigned char message[256];

    message[0] = MCENETD_HELLO;
    message[1] = MCENETD_MAGIC1;
    message[2] = MCENETD_MAGIC2;
    message[3] = MCENETD_MAGIC3;
    message[4] = context->udepth;
    message[5] = context->dev_num;
    
    l = mcenet_req(context, message, 6);

    if (l < 0)
        return 1;
    else if (l == 0) {
        fprintf(stderr, "mcenet: server %s unexpectedly dropped connection.\n",
                context->url);
        return 1;
    }


    if (message[0] == MCENETD_STOP) {
        fprintf(stderr, "mcenet: server reports device %s not ready.\n",
                context->url);
        return 1;
    } else if (message[0] != MCENETD_READY) {
        fprintf(stderr, "mcenet: unexpected response (%02x) from server %s\n",
                message[0], context->url);
        return 1;
    } else if (l < MCENETD_MSGLEN(MCENETD_READY)) {
        fprintf(stderr, "mcenet: short read from %s.\n", context->dev_name);
        return 1;
    }

    /* otherwise we should be good to go; record the data */
    context->net.proto = message[1];
    if (context->net.proto > 1) {
        fprintf(stderr, "mcenet: unknown protocol version (%i) reported by "
                "server %s\n", context->net.proto, context->dev_name);
    }
    context->net.token = message[2];
    context->net.ddepth = message[3];
    context->dev_endpoint = message[4];
    context->net.flags = message[5];
    if (~context->net.flags & MCENETD_F_MAS_OK) {
        fprintf(stderr, "mcenet: server reports device %s not ready.\n",
                context->url);
        return 1;
    }

    return 0;
}
