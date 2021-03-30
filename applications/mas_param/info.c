/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include "mas_param.h"
#include "info.h"

typedef struct {
    FILE *out;
    options_t *options;
    int show_type;
    int show_data;
} info_t;

int  info_init(unsigned long user_data, const options_t *options);
int  info_item(unsigned long user_data, const mas_param_t *p);
int  info_cleanup(unsigned long user_data);

int fullinfo_crawler(options_t *options, crawler_t *crawler, int full)
{
    info_t *info = (info_t*)malloc(sizeof(info_t));
    if (info==NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return -1;
    }
    memset(info, 0, sizeof(*info));
    info->options = options;
    info->show_type = 1;
    info->show_data = full;

    crawler->init = info_init;
    crawler->cleanup = info_cleanup;
    crawler->item = info_item;

    crawler->passes = 1;
    crawler->user_data = (unsigned long) info;

    return 0;
}

int full_crawler(options_t *options, crawler_t *crawler)
{
    return fullinfo_crawler(options, crawler, 1);
}

int info_crawler(options_t *options, crawler_t *crawler)
{
    return fullinfo_crawler(options, crawler, 0);
}

// Report format:
//    (name) : (type) : <array|simple> : count [: data]

int print_info(char *text, const mas_param_t *m)
{
    char *s = text;
    s += sprintf(s, "%s : ", m->data_name);
    switch (m->type) {
        case CFG_STR:
            s += sprintf(s, "string : ");
            break;
        case CFG_INT:
            s += sprintf(s, "integer : ");
            break;
        case CFG_DBL:
            s += sprintf(s, "float : ");
            break;
        default:
            s += sprintf(s, "?");
    }
    s += sprintf(s, "%s : %i", (m->array ? "array" : "simple"), m->count);
    return s - text;
}

int  info_init(unsigned long user_data, const options_t *options)
{
    info_t *info = (info_t*)user_data;
    info->out = stdout;

    if (info->options->output_on &&
            (info->out=fopen(info->options->output_file, "w")) == NULL) {
        fprintf(stderr, "mceparam could not open '%s' for output.\n",
                info->options->output_file);
        return -1;
    }

    return 0;
}

int info_cleanup(unsigned long user_data)
{
    info_t *info = (info_t*) user_data;
    if (info->options->output_on && info->out != NULL)
        fclose(info->out);
    free(info);
    return 0;
}

int  info_item(unsigned long user_data, const mas_param_t *p)
{
    info_t *info = (info_t*) user_data;
    char text[TEXT_SIZE];
    char *s = text;
    if (info->show_type) {
        s += print_info(s, p);
        if (info->show_data)
            s += sprintf(s, " : ");
    }
    if (info->show_data)
        s += print_data(s, p);
    fprintf(info->out, "%s\n", text);
    return 0;
}


int param_info(options_t *options)
{
    mas_param_t m;
    config_setting_t *c =
        config_setting_get_member(options->root, options->param_name);
    if (c == NULL) {
        printf("%s : absent\n", options->param_name);
    } else {
        if (param_get(c, &m)) {
            printf("%s : error\n", options->param_name);
        } else {
            char text[256];
            print_info(text, &m);
            printf("%s\n", text);
        }
    }
    return 0;
}


