/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

#include <stdio.h>
#include <stdlib.h>

#include <mce_library.h>
#include "context.h"

#include "eth.h"

/* CMD subsystem */
int mcecmd_eth_connect(mce_context_t context)
{
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
