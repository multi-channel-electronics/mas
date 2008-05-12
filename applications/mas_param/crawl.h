#ifndef _CRAWL_H_
#define _CRAWL_H_

/* config crawler actions */

typedef struct {

	int (*init)(unsigned user_data, const options_t* options);
	int (*item)(unsigned user_data, const mas_param_t *m);
	int (*cleanup)(unsigned user_data);

	unsigned user_data;
	int passes;

} crawler_t;


int crawl_now(options_t *options);
int crawl_festival(crawler_t *crawler, options_t *options);

#endif
