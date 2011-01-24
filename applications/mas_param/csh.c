/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include "mas_param.h"
#include "csh.h"

typedef struct {
	FILE *out;
	options_t *options;
	int error_count;
	int echo_only;
} csh_t;

int  csh_init(unsigned long user_data, const options_t *options);
int  csh_item(unsigned long user_data, const mas_param_t *p);
int  csh_cleanup(unsigned long user_data);

int csh_crawler(options_t *options, crawler_t *crawler)
{
	csh_t *csh = (csh_t*)malloc(sizeof(csh_t));
	if (csh==NULL) {
		fprintf(stderr, "Memory allocation error\n");
		return -1;
	}
	memset(csh, 0, sizeof(*csh));
	csh->options = options;
	csh->echo_only = 0;

	crawler->init = csh_init;
	crawler->cleanup = csh_cleanup;
	crawler->item = csh_item;

	crawler->passes = 1;
	crawler->user_data = (unsigned long) csh;

	return 0;	
}

int  csh_init(unsigned long user_data, const options_t *options)
{
	csh_t *csh = (csh_t*)user_data;
	csh->out = stdout;
	
	if (csh->options->output_on && 
	    (csh->out=fopen(csh->options->output_file, "w")) == NULL) {
		fprintf(stderr, "mce_param could not open '%s' for output.\n",
			csh->options->output_file);
		return -1;
	}
		
	fprintf(csh->out, "# START mas_param output\n");
	return 0;
}

int csh_cleanup(unsigned long user_data)
{
	csh_t *csh = (csh_t*) user_data;
	fprintf(csh->out, "# END mas_param output\n");
	free(csh);
	return 0;
}

int  csh_item(unsigned long user_data, const mas_param_t *p)
{
	csh_t *csh = (csh_t*) user_data;
	int i;

	switch(p->type) {

	case CFG_INT:
		if (p->array) {
			fprintf(csh->out, "set %s = (", p->data_name);
			for (i=0; i<p->count; i++) {
				fprintf(csh->out, " %i", p->data_i[i]);
			}
			fprintf(csh->out, " )\n");
		} else {
			fprintf(csh->out, "set %s = %i\n", p->data_name, *p->data_i);
		}
		
		break;

	case CFG_DBL:
		if (p->array) {
			fprintf(csh->out, "set %s = (", p->data_name);
			for (i=0; i<p->count; i++) {
				fprintf(csh->out, " %lf", p->data_d[i]);
			}
			fprintf(csh->out, " )\n");
		} else {
			fprintf(csh->out, "set %s = %lf\n", p->data_name, *p->data_d);
		}
		break;

	case CFG_STR:
        if (p->array) {
            fprintf(csh->out, "set %s = (", p->data_name);
            for (i=0; i<p->count; i++) {
                fprintf(csh->out, " \"%s\"", p->data_s[i]);
            }
            fprintf(csh->out, " )\n");
        } else {
            fprintf(csh->out, "set %s = \"%s\"\n", p->data_name, (*p->data_s));
        }
		break;

	default:
		fprintf(csh->out, "!%i\n", p->type);
	}
	return 0;
}
