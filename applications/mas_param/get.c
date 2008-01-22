#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mas_param.h"

int param_save(options_t *options)
{
	mas_param_t m;
	char *buf;
	int i,j;

	config_setting_t *c =
		config_setting_get_member(options->root, options->param_name);
	if (c == NULL) {
		fprintf(stderr, "Parameter '%s' not found in file '%s'.\n", 
			options->param_name, options->source_file);
		return -1;
	}
	
	if (param_get(c, &m))
		return 1;

	switch (m.type) {
	case CFG_STR:
		j=0;
		for (i=0; i<options->data_count; i++) {
			j += 1 + strlen(options->data_source[i]);
		}
		buf = malloc(j);
		j = 0;
		for (i=0; i<options->data_count; i++) {
			strcpy(buf + j, options->data_source[i]);
			j += 1 + strlen(options->data_source[i]);
			buf[j-1] = ' ';
		}
		buf[j-1] = 0;
		config_setting_set_string(m.cfg, buf);
		free(buf);
		break;

	case CFG_INT:
		if (m.array)
			for (i=0; i<options->data_count; i++)
				config_setting_set_int_elem(c, i, strtol(options->data_source[i],0,0));
		else
			config_setting_set_int(c, strtol(options->data_source[0],0,0));
		break;

	case CFG_DBL:
		if (m.array)
			for (i=0; i<options->data_count; i++)
				config_setting_set_float_elem(c, i, strtod(options->data_source[i],0));
		else 
			config_setting_set_float(c, strtod(options->data_source[0],0));

		break;
	}

	return 0;
}


			

int param_report(options_t *options)
{
	int i;
	mas_param_t m;
	config_setting_t *c =
		config_setting_get_member(options->root, options->param_name);
	if (c == NULL) {
		fprintf(stderr, "Parameter '%s' not found in file '%s'.\n", 
			options->param_name, options->source_file);
		return -1;
	}
	
	if (param_get(c, &m))
		return 1;

	switch (m.type) {
	case CFG_STR:
		printf("\"%s\"", m.data_s);
		break;

	case CFG_INT:
		for (i=0; i<m.count; i++)
			printf("%i ", m.data_i[i]);
		break;

	case CFG_DBL:
		for (i=0; i<m.count; i++)
			printf("%le ", m.data_d[i]);
		break;

	default:
		printf("?");
	}
		
	printf("\n");
	return 0;
}

// Report format:  <present|absent> : (type) : <array|simple> : count

int param_report_info(options_t *options)
{
	mas_param_t m;
	config_setting_t *c =
		config_setting_get_member(options->root, options->param_name);
	if (c == NULL) {
		printf("absent");
	} else {
		
		printf("present : ");
		
		if (param_get(c, &m)) {
			printf("error");
		} else {
			
			switch (m.type) {
			case CFG_STR:
				printf("string : ");
				break;
			case CFG_INT:
				printf("integer : ");
				break;
			case CFG_DBL:
				printf("float : ");
				break;
			default:
				printf("?");
			}
			printf("%s : %i", (m.array ? "array" : "simple"), m.count);
		}
	}
	printf("\n");
	return 0;
}


int param_get(config_setting_t *cfg, mas_param_t *m)
{
	int i;
	
	memset(m, 0, sizeof(*m));
	m->cfg = cfg;
	m->data_name = config_setting_name(cfg);
	m->array = 0;

	int cfg_type = config_setting_type(cfg);
	if (cfg_type == CONFIG_TYPE_GROUP ||
	    cfg_type == CONFIG_TYPE_LIST) {
		fprintf(stderr, "Key '%s': 'array' (\"[ val1, val2, ... ]\")"
			" is the only aggregate type supported!\n", m->data_name);
		return 1;
	} else if (cfg_type == CONFIG_TYPE_ARRAY) {
		m->array = 1;
		m->count = config_setting_length(cfg);
		if (m->count == 0) {
			fprintf(stderr, "Key '%s': empty array, assuming integer type.\n",
				m->data_name);
			m->type = CFG_INT;
		} else {
			cfg_type = config_setting_type(
				config_setting_get_elem(cfg, 0) );
		}
	} else {
		m->count = 1;
	}
		
	switch(cfg_type) {

	case CONFIG_TYPE_STRING:
		m->type = CFG_STR;
		if (m->array) {
			fprintf(stderr, "Key '%s': arrays of strings "
				"are not supported.\n", m->data_name);
			return 1;
		}
		m->data_s = config_setting_get_string(cfg);
		break;
			
	case CONFIG_TYPE_INT:
		m->type = CFG_INT;
		m->data_i = malloc(m->count * sizeof(int));
		if (m->array) {
			for (i=0; i<m->count; i++)
				m->data_i[i] = config_setting_get_int_elem(cfg, i);
		} else {
			*m->data_i = config_setting_get_int(cfg);
		}
		break;

	case CONFIG_TYPE_FLOAT:
		m->type = CFG_DBL;
		m->data_d = malloc(m->count * sizeof(double));
		if (m->array) {
			for (i=0; i<m->count; i++) {
				m->data_d[i] =
					config_setting_get_float_elem(cfg, i);
			}
		} else {
			*m->data_d = config_setting_get_float(cfg);
		}
		break;

	default:
		fprintf(stderr, "Unsupported libconfig data type.\n");
		return -1;			
	}
	return 0;
}

void param_destroy(mas_param_t *m)
{
	if (m->data_i != NULL) free(m->data_i);
	if (m->data_d != NULL) free(m->data_d);
	memset(m,0,sizeof(*m));
}
