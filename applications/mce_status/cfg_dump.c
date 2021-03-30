/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include "mce_status.h"
#include "cfg_dump.h"

typedef struct {
    FILE *out;
    options_t *options;
    int error_count;
    int echo_only;
    int xtend;
} cfg_t;

int cfg_init(unsigned long user_data, const options_t *options);
int cfg_item(unsigned long user_data, const mce_param_t *p);
int cfg_cleanup(unsigned long user_data);

int cfg_crawler(options_t *options, crawler_t *crawler)
{
    cfg_t *cfg = (cfg_t*)malloc(sizeof(cfg_t));
    if (cfg==NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return -1;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->options = options;

    crawler->init = cfg_init;
    crawler->cleanup = cfg_cleanup;
    crawler->card = NULL;
    crawler->item = cfg_item;

    crawler->user_data = (unsigned long) cfg;

    return 0;
}

int  cfg_init(unsigned long user_data, const options_t *options)
{
    cfg_t *cfg = (cfg_t*)user_data;
    cfg->out = stdout;
    cfg->xtend = (options->mode == CRAWLER_CFX) ? 1 : 0;

    if (cfg->options->output_on &&
            (cfg->out=fopen(cfg->options->output_file, "w")) == NULL) {
        fprintf(stderr, "CFG mcestatus could not open '%s' for output.\n",
                cfg->options->output_file);
        return -1;
    }

    fprintf(cfg->out, "Hardware file: %s\n",
            cfg->options->hardware_file);
    return 0;
}

int cfg_cleanup(unsigned long user_data)
{
    cfg_t *cfg = (cfg_t*) user_data;
    fclose(cfg->out);
    free(cfg);
    return 0;
}

int cfg_item(unsigned long user_data, const mce_param_t *p)
{
    int i;
    char flag = ' ';
    cfg_t *cfg = (cfg_t*) user_data;
    maprange_t mr;

    switch (p->card.nature) {
        case MCE_NATURE_PHYSICAL:

            if (cfg->xtend) {
                if (!(p->card.flags & MCE_PARAM_STAT) ||
                        !(p->param.flags & MCE_PARAM_STAT) ||
                        (p->param.flags & MCE_PARAM_WONLY) )
                    flag = '!';

                if (p->param.type != MCE_CMD_MEM)
                    flag = '!';
                fprintf(cfg->out,
                        "physical   %-10s %-20s x%02i %c %#04x %2i cards:",
                        p->card.name, p->param.name, p->param.count, flag,
                        p->param.id, p->card.card_count);
            } else
                fprintf(cfg->out, "physical   %-10s %-20s %#04x %2i cards:",
                        p->card.name, p->param.name, p->param.id,
                        p->card.card_count);

            for (i=0; i<p->card.card_count; i++) {
                fprintf(cfg->out, " %#04x", p->card.id[i]);
            }
            break;

        case MCE_NATURE_VIRTUAL:

            if (cfg->xtend)
                fprintf(cfg->out, "virtual    %-10s %-20s x%02i maps:",
                        p->card.name, p->param.name, p->param.count);
            else
                fprintf(cfg->out, "virtual    %-10s %-20s maps:",
                        p->card.name, p->param.name);

            for (i=0; i<p->param.map_count; i++) {
                mceconfig_param_maprange(cfg->options->context,
                        &p->param, i, &mr);
                fprintf(cfg->out, " [(%i,%i)->('%s %s'+%2i)]",
                        mr.start, mr.count,
                        mr.card_name, mr.param_name, mr.offset);
            }
            break;

        default:
            fprintf(cfg->out, "unknown    %2i", p->card.nature);
    }

    fprintf(cfg->out, "\n");

    return 0;
}
