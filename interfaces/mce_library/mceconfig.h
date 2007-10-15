/*! \file mceconfig.h
 *
 *  \brief Routines for parsing the hardware configuration file using
 *  libconfig.
 *
 *  The hardware configuration file must be a valid libconfig file.
 *  The file may be shared with other applications as long as it has
 *  a group called "hardware" at the lowest level.
 *
 *  Below the hardware key, the configuration file must have lists
 *  called  "parameter_sets", "card_types", and "system". *
 */

#ifndef _MCECONFIG_H_
#define _MCECONFIG_H_

#include <stdio.h>
#include <libconfig.h>
#include <mce.h>
#include <mcecmd.h>

#define NAME_LENGTH   32
#define CFG_PATHLEN   1024

typedef struct {

	struct config_t cfg;

	config_setting_t *parameter_sets;
	config_setting_t *card_types;
	config_setting_t *components;

} mce_data_t;


typedef struct {

	config_setting_t *cfg;

	char name[MCE_SHORT];
	int param_count;

} paramset_t;

typedef struct {

	config_setting_t *cfg;

	char name[MCE_SHORT];
	int paramset_count;

} cardtype_t;

typedef struct {
	
	config_setting_t *cfg;

	int id;
	char name[MCE_SHORT];
	int type;

	int count;
	int id_count;

	int flags;
	int min;
	int max;
	config_setting_t *defaults;

} param_t;

typedef struct {

	config_setting_t *cfg;

	int id;
	char name[MCE_SHORT];
	char type[MCE_SHORT];
	u32 error_bits;
	
} card_t;



/* Pre-prototypes */

int mceconfig_get_card(mce_data_t *mce, card_t *card, int idx);



/* New stuff :) */

int mceconfig_cardtype_paramset(mce_data_t *mce, 
				cardtype_t *ct,
				int idx,
				paramset_t *ps);

int mceconfig_paramset_param(mce_data_t *mce, 
			     paramset_t *ps,
			     int idx,
			     param_t *p);


int mceconfig_cardtype(mce_data_t *mce,
		       int idx,
		       cardtype_t *ct);

int mceconfig_paramset(mce_data_t *mce,
		       int idx,
		       paramset_t *ps);

int mceconfig_card(mce_data_t *mce,
		   int idx,
		   card_t *c);


int mceconfig_card_cardtype(mce_data_t *mce,
			    card_t *c,
			    cardtype_t *ct);

/* Prototypes */

int     mceconfig_load(char *filename, mce_data_t **mce);

int     mceconfig_destroy(mce_data_t *mce);

void    mceconfig_print(FILE *out, mce_data_t* mce);

char*   mceconfig_errstr(void);

/* para_t* mceconfig_lookup_para(card_t *card, char *para_str, */
/* 			   int *para_id, int index); */

/* card_t* mceconfig_lookup_card(mce_data_t *mce, char *card_str, int *card_id); */

int     mceconfig_lookup(param_properties_t *p, mce_data_t *mce,
				const char *card_str, const char *para_str,
				int para_idx);

int mceconfig_cfg_cardtype(cardtype_t *ct, config_setting_t *cfg);

int mceconfig_cfg_paramset(paramset_t *ps, config_setting_t *cfg);

int mceconfig_cfg_param(param_t *p, config_setting_t *cfg);

int mceconfig_cfg_card(card_t *card, config_setting_t *cfg);

#endif
