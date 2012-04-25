/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdlib.h>

#include "masconfig.h"

/* File load and save */

int mas_load(const char *filename, config_t *cfg)
{
	config_init(cfg);

	if (config_read_file(cfg, filename) != CONFIG_TRUE) {
		fprintf(stderr, "libconfig parsing error in '%s': line %i: '%s'\n",
			filename, config_error_line(cfg), config_error_text(cfg));
		return -1;
	}
	return 0;
}

int mas_save(const char *filename, config_t *cfg)
{
	return config_write_file(cfg, filename) == CONFIG_TRUE ? 0 : -1;
}


/* Get scalar config settings */

static 
config_setting_t *get_setting(config_t *cfg, const char *name, int type)
{
	config_setting_t *c = config_lookup(cfg, name);
	if (c==NULL) return NULL;

	if (config_setting_type(c) != type) {
		fprintf(stderr, "Setting '%s' does not have the expected type.\n",
			name);
		return NULL;
	}

	return c;
}

static 
config_setting_t *get_element(config_setting_t *cfg, int index, int type)
{
	config_setting_t *c = config_setting_get_elem(cfg, index);
	if (c==NULL) return NULL;

	if (config_setting_type(c) != type) {
		fprintf(stderr, "Array element has unexpected type.\n");
		return NULL;
	}

	return c;
}

int mas_get_int(config_t *cfg, const char *name, int *dest)
{
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_INT);
	if (c==NULL) return -1;
	*dest = config_setting_get_int(c);
	return 0;
}

int mas_get_dbl(config_t *cfg, const char *name, double *dest)
{
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_FLOAT);
	if (c==NULL) return -1;
	*dest = config_setting_get_float(c);
	return 0;
}

int mas_get_string(config_t *cfg, const char *name, const char **dest)
{
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_STRING);
	if (c==NULL) return -1;
	*dest = config_setting_get_string(c);
	return 0;
}


/* Get array settings */

int mas_get_int_array(config_t *cfg, const char *name, int *dest, int max)
{
	int i;
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_ARRAY);
	config_setting_t *d = get_element(c, 0, CONFIG_TYPE_INT);
	if (d==NULL) return -1;
	for (i=0; i<max && i<config_setting_length(c); i++) {
		dest[i] = config_setting_get_int_elem(c, i);
	}
	return i;
}

int mas_get_dbl_array(config_t *cfg, const char *name, int *dest, int max)
{
	int i;
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_ARRAY);
	config_setting_t *d = get_element(c, 0, CONFIG_TYPE_FLOAT);
	if (d==NULL) return -1;
	for (i=0; i<max && i<config_setting_length(c); i++) {
		dest[i] = config_setting_get_float_elem(c, i);
	}
	return i;
}


/* Set scalar elements (settings are not created if they're not found) */

int mas_set_int(config_t *cfg, const char *name, int src)
{
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_INT);
	// Should optionally create it if not found...
	if (c == NULL) return -1;

	return config_setting_set_int(c, src) == CONFIG_TRUE ? 0 : -1;
}

int mas_set_dbl(config_t *cfg, const char *name, double src)
{
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_FLOAT);
	// Should optionally create it if not found...
	if (c == NULL) return -1;

	return config_setting_set_float(c, src) == CONFIG_TRUE ? 0 : -1;
}

int mas_set_string(config_t *cfg, const char *name, char *src)
{
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_INT);
	// Should optionally create it if not found...
	if (c == NULL) return -1;

	return config_setting_set_string(c, src) == CONFIG_TRUE ? 0 : -1;
}


/* Set array elements.  These routines don't allow you to change the array size! */

int mas_set_int_array(config_t *cfg, const char *name, int *src, int count)
{
	int i;
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_ARRAY);
	config_setting_t *d = get_element(c, 0, CONFIG_TYPE_INT);
	if (d==NULL) return -1;
	for (i=0; i<count; i++) {
		config_setting_set_int_elem(c, i, src[i]);
	}
	return i;
}


int mas_set_dbl_array(config_t *cfg, const char *name, int *src, int count)
{
	int i;
	config_setting_t *c = get_setting(cfg, name, CONFIG_TYPE_ARRAY);
	config_setting_t *d = get_element(c, 0, CONFIG_TYPE_FLOAT);
	if (d==NULL) return -1;
	for (i=0; i<count; i++) {
		config_setting_set_float_elem(c, i, src[i]);
	}
	return i;
}
