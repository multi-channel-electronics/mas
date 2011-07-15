/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <mce/socks.h>
#include <mcenetd.h>
#include <mce_library.h>

#include "context.h"
#include "net.h"

/* generic functions */

/* read a message from descriptor d */
ssize_t mcenet_readmsg(int d, unsigned char *msg, size_t l)
{
    ssize_t n, extra = 0;

    /* read the opcode */
    if ((n = read(d, msg, 1)) <= 0)
        return n;

    /* corresponding length */
    if (l == -1)
        l = MCENETD_MSGLEN(msg[0]) - 1;
    else
        l--;

    if (l == -1) { /* variable length message, the next byte tells us the
                      length */
        if ((n = read(d, ++msg, 1)) <= 0)
            return n;
        l = *msg - 2;
        extra = 1;
    }

    if (l == 0) /* no more to read */
        n = 0;
    else if ((n = recv(d, msg + 1, l, MSG_WAITALL)) <= 0)
        return n;

    return 1 + extra + n;
}

/* client stuff */

/* poll a socket for an event */
static int mcenet_poll(int fd, short event, const char *name)
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

/* send a request on the a socket, the response is stored in message */
static ssize_t mcenet_req(mce_context_t context, int d, unsigned char *message,
        size_t len, size_t rsplen, int flags)
{
    ssize_t n;

#if 1
    size_t i;
    fprintf(stderr, "mcenet: req@%i -> %s:", d, context->dev_name);
    for (i = 0; i < len; ++i)
        fprintf(stderr, " %02hhx", message[i]);
    fprintf(stderr, "\n");
#endif

    /* wait for the socket to become ready */
    if (mcenet_poll(d, POLLOUT, context->dev_name))
        return -1;

    /* write the request */
    n = write(d, message, len);
    if (n < len) {
        fprintf(stderr, "mcenet: short write to %s\n", context->dev_name);
        return -1;
    }

    if (flags & MSG_DONTWAIT)
        return n;

    /* wait for a response */
    if (mcenet_poll(d, POLLIN, context->dev_name))
        return -1;

    /* read the response */
    n = mcenet_readmsg(d, message, rsplen);

#if 1
    fprintf(stderr, "mcenet: rsp@%i <- %s:", d, context->dev_name);
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
    
    l = mcenet_req(context, context->net.ctl_sock, message, 6, -1, 0);

    if (l < 0)
        return 1;
    else if (l == 0) {
        fprintf(stderr, "mcenet: server %s unexpectedly dropped connection.\n",
                context->dev_name);
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
    if (context->net.proto != MCENETD_PROTO_VERSION) {
        fprintf(stderr, "mcenet: unsupported protocol version (%i) reported by "
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

/* generic client stuff */

static int dev_connect(mce_context_t context, int port, const char *what)
{
    int sock;
    int l;
    unsigned char message[256];

    /* connect to the port */
    sock = massock_connect(context->dev_name, port);
    if (sock < 0) {
        fprintf(stderr, "mcelib: Unable to connect to %s port at %s\n",
                what, context->dev_name);
        return -1;
    }

    /* identify ourselves */
    message[0] = context->net.token;
    message[1] = MCENETD_MAGIC1;
    message[2] = MCENETD_MAGIC2;
    message[3] = MCENETD_MAGIC3;

    /* send the id string, and get confirmation */
    l = mcenet_req(context, sock, message, 4, 5, 0);

    if (l < 0)
        return -1;
    else if (l == 0) {
        fprintf(stderr, "mcenet: server %s unexpectedly dropped connection.\n",
                context->dev_name);
        return -1;
    } else if (l < 5) {
        fprintf(stderr, "mcenet: short read on %s port from %s.\n", what,
                context->dev_name);
        return -1;
    } else if (message[0] != context->net.token || message[1] != 5 ||
            message[2] != MCENETD_MAGIC1 || message[3] != MCENETD_MAGIC2 ||
            message[4] != MCENETD_MAGIC3)
    {
        fprintf(stderr,
                "mcenet: unexpected response on %s port from %s.\n", what,
                context->dev_name);
        return -1;
    }

    return sock;
}

static int dev_read(int fd, void *buf, size_t count, const char *dev_name)
{
    ssize_t n, done;
    int32_t result;
    /* read the a response from the server.  This consists of a 32-bit
     * return value from the upstream read, followed by a reply packet
     * (which is bogus, if the return value indicates an error, but must
     * still be flushed from the network queue). We shove it into buf,
     * since that's a convenient place to discard it. */

    /* wait for the socket to become ready */
    if (mcenet_poll(fd, POLLIN, dev_name))
        return -1;

    n = read(fd, &result, 4);

    if (n < 0)
        return -MCE_ERR_DEVICE;
    else if (n == 0) {
        fprintf(stderr, "mcenet: server %s unexpectedly dropped connection.\n",
                dev_name);
        return -MCE_ERR_DEVICE;
    }

#if 1
        int i;
        fprintf(stderr, "mcenet: res@%i <- %s:", fd, dev_name);
        for (i = 0; i < (size_t)n; ++i)
            fprintf(stderr, " %02hhx", ((const char*)&result)[i]);
        fprintf(stderr, "\n");
#endif

    for (done = 0; done < count; done += n) {
        /* wait for the socket to become ready */
        if (mcenet_poll(fd, POLLIN, dev_name))
            return -1;

        n = read(fd, (char*)buf + done, count - done);

        if (n < 0)
            return -MCE_ERR_DEVICE;
        else if (n == 0) {
            fprintf(stderr,
                    "mcenet: server %s unexpectedly dropped connection.\n",
                    dev_name);
            return -MCE_ERR_DEVICE;
        }
    }

#if 1
        fprintf(stderr, "mcenet: res@%i <- %s:", fd, dev_name);
        for (i = 0; i < count; ++i)
            fprintf(stderr, " %02hhx", ((const char*)buf)[i]);
        fprintf(stderr, "\n");
#endif

    return (int)result;
}

static int dev_write(int fd, const void *buf, size_t count,
        const char *dev_name)
{
    ssize_t n, done;
    int32_t result;

    /* send all the data, when possible */
    for (done = 0; done < count; done += n) {
        /* wait for the socket to become ready */
        if (mcenet_poll(fd, POLLOUT, dev_name))
            return -1;

        n = write(fd, (const char*)buf + done, count - done);

        if (n < 0)
            return -MCE_ERR_DEVICE;
        else if (n == 0) {
            fprintf(stderr,
                    "mcenet: server %s unexpectedly dropped connection.\n",
                    dev_name);
            return -MCE_ERR_DEVICE;
        }

#if 1
        int i;
        fprintf(stderr, "mcenet: req@%i -> %s:", fd, dev_name);
        for (i = 0; i < (size_t)n; ++i)
            fprintf(stderr, " %02hhx", ((const char*)buf + done)[i]);
        fprintf(stderr, "\n");
#endif

    }

    /* wait for the response */
    if (mcenet_poll(fd, POLLIN, dev_name))
        return -1;

    n = read(fd, &result, 4);

    if (n < 0)
        return -MCE_ERR_DEVICE;
    else if (n == 0) {
        fprintf(stderr, "mcenet: server %s unexpectedly dropped connection.\n",
                dev_name);
        return -MCE_ERR_DEVICE;
    }

#if 1
    int i;
    fprintf(stderr, "mcenet: rsp@%i <- %s:", fd, dev_name);
    for (i = 0; i < (size_t)n; ++i)
        fprintf(stderr, " %02hhx", ((char*)(&result))[i]);
    fprintf(stderr, "\n");
#endif

    return (int)result;
}

static int dev_disconnect(int sock)
{
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        if (close(sock))
            return -MCE_ERR_DEVICE;
    }

    return 0;
}
/* CMD subsystem */
int mcecmd_net_connect(mce_context_t context)
{
    /* check that the command port is available */
    if (~context->net.flags & MCENETD_F_CMD_OK) {
        fprintf(stderr,
                "mcelib: server reports command device of %s unavailable.\n",
                context->url);
        return -MCE_ERR_ATTACH;
    }

    int sock = dev_connect(context, MCENETD_CMDPORT, "command");

    if (sock < 0)
        return -MCE_ERR_ATTACH;

    context->net.cmd_sock = sock;

    return 0;
}

int mcecmd_net_disconnect(mce_context_t context)
{
    return dev_disconnect(context->net.cmd_sock);
}

int mcecmd_net_ioctl(mce_context_t context, unsigned long int req, int arg)
{
    int l;
    unsigned char message[256];
    void *ptr = message + 1;

    /* send the request, and get the response */
    message[0] = MCENETD_CMDIOCTL; /* ioctl request */
    *(uint32_t*)ptr = (uint32_t)req;
    *(int32_t*)(ptr + 4) = (int32_t)arg;
    l = mcenet_req(context, context->net.ctl_sock, message,
            MCENETD_MSGLEN(MCENETD_CMDIOCTL), -1, 0);

    if (l < 0)
        return -1;
    else if (l == 0) {
        fprintf(stderr, "mcenet: server %s unexpectedly dropped connection.\n",
                context->dev_name);
        return -1;
    } else if (l < 4) {
        fprintf(stderr, "mcenet: short read on command port from %s.\n",
                context->dev_name);
        return -1;
    } else if (message[0] != MCENETD_IOCTLRET || message[1] != 6) {
        fprintf(stderr, "mcenet: unexpected ioctl response from %s.\n",
                context->dev_name);
        return -1;
    }

    return *(int*)ptr;
}

int mcecmd_net_read(mce_context_t context, void *buf, size_t count)
{
    return dev_read(context->net.cmd_sock, buf, count, context->dev_name);
}

int mcecmd_net_write(mce_context_t context, const void *buf, size_t count)
{
    return dev_write(context->net.cmd_sock, buf, count, context->dev_name);
}

/* DSP subsystem */
int mcedsp_net_connect(mce_context_t context)
{
    /* check that the dsp port is available */
    if (~context->net.flags & MCENETD_F_DSP_OK) {
        fprintf(stderr,
                "mcelib: server reports dsp device of %s unavailable.\n",
                context->url);
        return -MCE_ERR_ATTACH;
    }

    int sock = dev_connect(context, MCENETD_DSPPORT, "dsp");

    if (sock < 0)
        return -MCE_ERR_ATTACH;

    context->net.dsp_sock = sock;

    return 0;
}

int mcedsp_net_disconnect(mce_context_t context)
{
    return dev_disconnect(context->net.dsp_sock);
}

int mcedsp_net_ioctl(mce_context_t context, unsigned long int req, ...)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedsp_net_read(mce_context_t context, void *buf, size_t count)
{
    return dev_read(context->net.dsp_sock, buf, count, context->dev_name);
}

ssize_t mcedsp_net_write(mce_context_t context, const void *buf, size_t count)
{
    return dev_write(context->net.dsp_sock, buf, count, context->dev_name);
}

/* DATA subsystem */
int mcedata_net_connect(mce_context_t context)
{
    /* check that the data port is available */
    if (~context->net.flags & MCENETD_F_DAT_OK) {
        fprintf(stderr,
                "mcelib: server reports data device of %s unavailable.\n",
                context->url);
        return -MCE_ERR_ATTACH;
    }

    int sock = dev_connect(context, MCENETD_DATPORT, "data");

    if (sock < 0)
        return -MCE_ERR_ATTACH;

    context->net.dat_sock = sock;

    return 0;
}

int mcedata_net_disconnect(mce_context_t context)
{
    return dev_disconnect(context->net.dat_sock);
}

int mcedata_net_ioctl(mce_context_t context, unsigned long int req, ...)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedata_net_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedata_net_write(mce_context_t context, const void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}
