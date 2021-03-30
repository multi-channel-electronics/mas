/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _CRAWL_H_
#define _CRAWL_H_

/* config crawler actions */

typedef struct {

    int (*init)(unsigned long user_data, const options_t* options);
    int (*item)(unsigned long user_data, const mas_param_t *m);
    int (*cleanup)(unsigned long user_data);

    unsigned long user_data;
    int passes;

} crawler_t;


int crawl_now(options_t *options);
int crawl_festival(crawler_t *crawler, options_t *options);

#endif
