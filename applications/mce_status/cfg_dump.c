#include <stdlib.h>
#include <string.h>
#include "mce_status.h"
#include "cfg_dump.h"

typedef struct {
	FILE *out;
	options_t *options;
	int error_count;
	int echo_only;
} cfg_t;

int cfg_init(unsigned user_data, const options_t *options);
int cfg_item(unsigned user_data, const mce_param_t *p);
int cfg_cleanup(unsigned user_data);

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
	crawler->item = cfg_item;

	crawler->user_data = (unsigned) cfg;

	return 0;	
}

int  cfg_init(unsigned user_data, const options_t *options)
{
	cfg_t *cfg = (cfg_t*)user_data;
	cfg->out = stdout;
	
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

int cfg_cleanup(unsigned user_data)
{
	cfg_t *cfg = (cfg_t*) user_data;
	fclose(cfg->out);
	free(cfg);
	return 0;
}

int cfg_item(unsigned user_data, const mce_param_t *p)
{
	int i;
	cfg_t *cfg = (cfg_t*) user_data;

	fprintf(cfg->out, "%-10s %-20s %#04x %2i",
		p->card.name, p->param.name, p->param.id, p->card.card_count);
	
	for (i=0; i<p->card.card_count; i++) {
		fprintf(cfg->out, " %#04x", p->card.id[i]);
	}

	fprintf(cfg->out, "\n");

	return 0;
}
