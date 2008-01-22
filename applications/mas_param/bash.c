#include <stdlib.h>
#include <string.h>
#include "mas_param.h"
#include "bash.h"

typedef struct {
	FILE *out;
	options_t *options;
	char *prefix;
	int error_count;
	int echo_only;
} bash_t;

int  bash_init(unsigned user_data, const options_t *options);
int  bash_item(unsigned user_data, const mas_param_t *p);
int  bash_cleanup(unsigned user_data);

int bash_crawler(options_t *options, crawler_t *crawler)
{
	bash_t *bash = (bash_t*)malloc(sizeof(bash_t));
	if (bash==NULL) {
		fprintf(stderr, "Memory allocation error\n");
		return -1;
	}
	memset(bash, 0, sizeof(*bash));
	bash->options = options;
	bash->echo_only = 0;
	bash->prefix = options->output_word;

	crawler->init = bash_init;
	crawler->cleanup = bash_cleanup;
	crawler->item = bash_item;

	crawler->passes = 1;
	crawler->user_data = (unsigned) bash;

	return 0;	
}

int  bash_init(unsigned user_data, const options_t *options)
{
	bash_t *bash = (bash_t*)user_data;
	bash->out = stdout;
	
	if (bash->options->output_on && 
	    (bash->out=fopen(bash->options->output_file, "w")) == NULL) {
		fprintf(stderr, "mceparam could not open '%s' for output.\n",
			bash->options->output_file);
		return -1;
	}
		
	fprintf(bash->out, "# START mas_param output\n");
	return 0;
}

int bash_cleanup(unsigned user_data)
{
	bash_t *bash = (bash_t*) user_data;
	fprintf(bash->out, "# END mas_param output\n");
	free(bash);
	return 0;
}

int  bash_item(unsigned user_data, const mas_param_t *p)
{
	bash_t *bash = (bash_t*) user_data;
	int i;

	switch(p->type) {

	case CFG_INT:
		if (p->array) {
			fprintf(bash->out, "%s%s=(", bash->prefix, p->data_name);
			for (i=0; i<p->count; i++) {
				fprintf(bash->out, " %i", p->data_i[i]);
			}
			fprintf(bash->out, " )\n");
		} else {
			fprintf(bash->out, "%s%s=%i\n", 
				bash->prefix, p->data_name, *p->data_i);
		}
		
		break;

	case CFG_DBL:
		if (p->array) {
			fprintf(bash->out, "%s%s=(", bash->prefix, p->data_name);
			for (i=0; i<p->count; i++) {
				fprintf(bash->out, " %lf", p->data_d[i]);
			}
			fprintf(bash->out, " )\n");
		} else {
			fprintf(bash->out, "%s%s=%lf\n",
				bash->prefix, p->data_name, *p->data_d);
		}
		break;

	case CFG_STR:
		fprintf(bash->out, "%s%s=\"%s\"\n",
			bash->prefix, p->data_name, (p->data_s));
		break;

	default:
		fprintf(bash->out, "!%i\n", p->type);
	}
	return 0;
}
