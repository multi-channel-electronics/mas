/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

#include <stdio.h>
#include <stdlib.h>

#include <mcenetd.h>
#include <mce_library.h>
#include "context.h"

#include "net.h"

/* mcenetd generic functions */

/* read a message from descriptor d */
ssize_t mcenet_readmsg(int d, unsigned char *msg)
{
    ssize_t n, extra = 0;
    size_t l;

    /* read the opcode */
    if ((n = read(d, msg, 1)) <= 0)
        return n;

    /* corresponding length */
    l = MCENETD_MSGLEN(msg[0]) - 1;

    if (l == -1) { /* variable length message, the next byte tells us the
                      length */
        if ((n = read(d, ++msg, 1)) <= 0)
            return n;
        l = msg[1];
        extra = 1;
    }

    if (l == 0) /* no more to read */
        n = 0;
    else if ((n = read(d, msg + 1, l)) <= 0)
        return n;

    return 1 + extra + n;
}

/* mcenetd server side */

/* CMD subsystem */
int mcecmd_net_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcecmd_net_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcecmd_net_ioctl(mce_context_t context, unsigned long int req, ...)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcecmd_net_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcecmd_net_write(mce_context_t context, const void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

/* DSP subsystem */
int mcedsp_net_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedsp_net_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedsp_net_ioctl(mce_context_t context, unsigned long int req, ...)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedsp_net_read(mce_context_t context, void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

ssize_t mcedsp_net_write(mce_context_t context, const void *buf, size_t count)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

/* DATA subsystem */
int mcedata_net_connect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
}

int mcedata_net_disconnect(mce_context_t context)
{
    fprintf(stderr, "Some work is needed on line %i of %s\n", __LINE__,
            __FILE__);
    abort();
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
