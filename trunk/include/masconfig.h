#ifndef _MAS_CONFIG_H_
#define _MAS_CONFIG_H_

/* Routines for easy interaction with simple libconfig configuration files */
/* Programs using these functions must also link to libconfig. */

#include <libconfig.h>

/*
 Config files should have only root level, scalar entries, e.g.:
    bias = 5;
    command = "wb cc led";

  Arrays of ints and floats are allowed, but other aggregate types are
  forbidden:
     some_integers = [ 1, 2, 3];
  is ok but
     some_stuff = { "hi", 2.3 };
  is not.
*/

int mas_load(const char *filename, config_t *cfg);
int mas_save(const char *filename, config_t *cfg);

int mas_get_int(config_t *cfg, const char *name, int *dest);
int mas_get_dbl(config_t *cfg, const char *name, double *dest);
int mas_get_string(config_t *cfg, const char *name, const char **dest);

int mas_get_int_array(config_t *cfg, const char *name, int *dest, int max);
int mas_get_dbl_array(config_t *cfg, const char *name, int *dest, int max);

int mas_set_int(config_t *cfg, const char *name, int dest);
int mas_set_dbl(config_t *cfg, const char *name, double dest);
int mas_set_string(config_t *cfg, const char *name, char *dest);

int mas_set_int_array(config_t *cfg, const char *name, int *dest, int count);
int mas_set_dbl_array(config_t *cfg, const char *name, int *dest, int count);

#endif
