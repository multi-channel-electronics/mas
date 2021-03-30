/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mce_status.h"
#include "mas.h"

typedef struct {
    FILE *out;
    time_t time;
    options_t *options;
    int error_count;
    int echo_only;
} mas_t;

int  mas_init(unsigned long user_data, const options_t *options);
int  mas_item(unsigned long user_data, const mce_param_t *p);
int  mas_cleanup(unsigned long user_data);

int mas_crawler(options_t *options, crawler_t *crawler)
{
    mas_t *mas = (mas_t*)malloc(sizeof(mas_t));
    if (mas==NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return -1;
    }
    memset(mas, 0, sizeof(*mas));
    mas->options = options;
    mas->echo_only = 0;

    crawler->init = mas_init;
    crawler->cleanup = mas_cleanup;
    crawler->card = NULL;
    crawler->item = mas_item;

    crawler->user_data = (unsigned long) mas;

    return 0;
}

int  mas_init(unsigned long user_data, const options_t *options)
{
    mas_t *mas = (mas_t*)user_data;
    mas->time = time(NULL);
    mas->out = stdout;

    if (mas->options->output_on &&
            (mas->out=fopen(mas->options->output_file, "w")) == NULL) {
        fprintf(stderr, "MAS mcestatus could not open '%s' for output.\n",
                mas->options->output_file);
        return -1;
    }

    fprintf(mas->out, "# Begin snapshot, ctime=%u\n", (int)mas->time);
    // Note that ctime string carries its own \n
    fprintf(mas->out, "# Date: %s", ctime(&mas->time));
    return 0;
}

int mas_cleanup(unsigned long user_data)
{
    mas_t *mas = (mas_t*) user_data;
    fprintf(mas->out, "# End snapshot, ctime=%u\n", (int)mas->time);
    free(mas);
    return 0;
}

int  mas_item(unsigned long user_data, const mce_param_t *p)
{
    mas_t *mas = (mas_t*) user_data;
    uint32_t buf[MAX_MCE_READ];

    if (!(p->card.flags & MCE_PARAM_STAT) ||
            !(p->param.flags & MCE_PARAM_STAT) ||
            (p->param.flags & MCE_PARAM_WONLY) )
        return 0;

    if (p->param.type != MCE_CMD_MEM)
        return 0;

    fprintf(mas->out, "%s %s :", p->card.name, p->param.name);
    if (mas->echo_only) {

        fprintf(mas->out, "\n");
        return 0;
    }

    // Read some data
    int err = mcecmd_read_block(mas->options->context,
            p, p->param.count, buf);
    if ( err ) {
        fprintf(mas->out, " ERROR");
        mas->error_count++;
    } else {
        int i;
        for (i=0; i < p->param.count*p->card.card_count; i++) {
            if (p->param.flags & MCE_PARAM_HEX) {
                fprintf(mas->out, " %#x", buf[i]);
            } else {
                fprintf(mas->out, " %i", buf[i]);
            }
        }
    }

    fprintf(mas->out, "\n");

    return 0;
}
