/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _MCE_STATUS_H_
#define _MCE_STATUS_H_

#include <mce_library.h>
#include <mce/defaults.h>

#define PROGRAM_NAME           "mce_status"

/* There is, in principle, no upper limit to the number of words a
   read could return. */
#define MAX_MCE_READ 1024

enum {
    CRAWLER_DAS = 0,
    CRAWLER_MAS,
    CRAWLER_CFG,
    CRAWLER_CFX,
    CRAWLER_DRF,
};


/* options structure and processor */

typedef struct {
    int fibre_card;
    char *config_file;
    char *hardware_file;
    char output_path[MCE_LONG];

    char output_file[MCE_LONG];
    int  output_on;

    int mode;

    mce_context_t* context;

} options_t;

int process_options(options_t *options, int argc, char **argv);


/* config crawler actions */

typedef struct {

    int (*init)(unsigned long user_data, const options_t* options);
    int (*card)(unsigned long user_data, const card_t *c);
    int (*item)(unsigned long user_data, const mce_param_t *m);
    int (*cleanup)(unsigned long user_data);

    unsigned long user_data;

} crawler_t;

/* in dirfile.c */
extern int dirfile_crawler(options_t *options, crawler_t *crawler);

#endif
