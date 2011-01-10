#ifndef _GET_H_
#define _GET_H_


int param_save(options_t *options);
int param_report(options_t *options);
int param_get(config_setting_t *cfg, mas_param_t *m);

int print_data(char *text, const mas_param_t *m);

#endif
