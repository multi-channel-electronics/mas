/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _INFO_H_
#define _INFO_H_

/* mas_param handler - output type description along with (maybe) data */

int info_crawler(options_t *options, crawler_t *crawler);
int full_crawler(options_t *options, crawler_t *crawler);

int param_info(options_t *options);

#endif
