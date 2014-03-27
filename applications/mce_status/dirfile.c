/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mce_status.h"

typedef struct {
    FILE *out;
    options_t *options;
    int error_count;
} dirfile_t;

static int dirfile_init(unsigned long user_data, const options_t *options)
{
    char hostname[MCE_LONG];
    char pretty_time[MCE_LONG];
    time_t t = time(NULL);

    dirfile_t *dirfile = (dirfile_t*)user_data;
    dirfile->out = stdout;

    if (dirfile->options->output_on && 
            (dirfile->out=fopen(dirfile->options->output_file, "w")) == NULL) {
        fprintf(stderr, "MAS mcestatus could not open '%s' for output.\n",
                dirfile->options->output_file);
        return -1;
    }

    if (gethostname(hostname, MCE_LONG))
        strcpy(hostname, "(unknown)");
    else
        hostname[MCE_LONG - 1] = 0;

    strftime(pretty_time, MCE_LONG, "%a %e %b %T UTC %Y", gmtime(&t));

    /* Dirfile Standards Version >= 8 required for CARRAY */
    fprintf(dirfile->out, "/VERSION 8\n\n");

    fprintf(dirfile->out, "mce_status         STRING      \"mce status\"\n");
    fprintf(dirfile->out, "mce_status/ctime   CONST UINT64 %u\n", (unsigned)t);
    fprintf(dirfile->out, "mce_status/date    STRING      \"%s\"\n",
            pretty_time);
    fprintf(dirfile->out, "mce_status/author  STRING      \"%s\"\n",
            getenv("USER"));
    fprintf(dirfile->out, "mce_status/host    STRING      \"%s\"\n", hostname);
    fprintf(dirfile->out, "mce_status/version STRING      \"%s\"\n",
            VERSION_STRING);
    return 0;
}

static int dirfile_cleanup(unsigned long user_data)
{
    dirfile_t *dirfile = (dirfile_t*) user_data;
    free(dirfile);
    return 0;
}

static int dirfile_card(unsigned long user_data, const card_t *c)
{
    dirfile_t *dirfile = (dirfile_t*) user_data;
    fprintf(dirfile->out, "\n%s STRING \"%s\"\n", c->name, c->type);
    return 0;
}

static int dirfile_item(unsigned long user_data, const mce_param_t *p)
{
    dirfile_t *dirfile = (dirfile_t*) user_data;
    uint32_t buf[MCE_REP_DATA_MAX];

    if (!(p->card.flags & MCE_PARAM_STAT) ||
            !(p->param.flags & MCE_PARAM_STAT) ||
            (p->param.flags & MCE_PARAM_WONLY) )
        return 0;

    if (p->param.type != MCE_CMD_MEM)
        return 0;

    // Read some data
    int err = mcecmd_read_block(dirfile->options->context,
            p, p->param.count, buf);
    if (err) {
        fprintf(dirfile->out, "# %s/%s ERROR\n", p->card.name, p->param.name);
        dirfile->error_count++;
    } else {
        int i;
        fprintf(dirfile->out, "%s/%s ", p->card.name, p->param.name);

        if (p->param.count * p->card.card_count == 1)
            fprintf(dirfile->out, "CONST");
        else
            fprintf(dirfile->out, "CARRAY");

        if (p->param.flags & MCE_PARAM_HEX)
            fprintf(dirfile->out, " UINT16");
        else
            fprintf(dirfile->out, " INT16");

        for (i = 0; i < p->param.count * p->card.card_count; i++) {
            if (p->param.flags & MCE_PARAM_HEX) {
                fprintf(dirfile->out, " %u", buf[i]);
            } else {
                fprintf(dirfile->out, " %i", buf[i]);
            }
        }

        fprintf(dirfile->out, "\n");
    }

    return 0;
}

int dirfile_crawler(options_t *options, crawler_t *crawler)
{
    dirfile_t *dirfile = (dirfile_t*)malloc(sizeof(dirfile_t));
    if (dirfile==NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return -1;
    }
    memset(dirfile, 0, sizeof(*dirfile));
    dirfile->options = options;

    crawler->init = dirfile_init;
    crawler->cleanup = dirfile_cleanup;
    crawler->card = dirfile_card;
    crawler->item = dirfile_item;

    crawler->user_data = (unsigned long)dirfile;

    return 0;
}
