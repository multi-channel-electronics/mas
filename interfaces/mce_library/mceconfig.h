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


#define MCE_CMD_INT 0
#define MCE_CMD_CMD 1
#define MCE_CMD_RST 2

#define MCE_PARAM_MIN    0x00000001 /* Parameter has minimum */
#define MCE_PARAM_MAX    0x00000002 /* Parameter has maximum */
#define MCE_PARAM_DEF    0x00000004
#define MCE_PARAM_PARAM  0x00000010
#define MCE_PARAM_CARD   0x00000020
#define MCE_PARAM_NOSTAT 0x00000040 /* Not to be included in mce status */
#define MCE_PARAM_WONLY  0x00000080 /* Write only */
#define MCE_PARAM_RONLY  0x00000100 /* Read only */
#define MCE_PARAM_SIGNED 0x00000200
#define MCE_PARAM_GO     0x00010000
#define MCE_PARAM_ST     0x00020000

typedef struct {

	struct config_t cfg;

	const config_setting_t *parameter_sets;
	const config_setting_t *card_types;
	const config_setting_t *components;

	int card_count;
	int cardtype_count;
	int paramset_count;

} mceconfig_t;


typedef struct {

	const config_setting_t *cfg;

	char name[MCE_SHORT];
	int param_count;

} paramset_t;


typedef struct {

	const config_setting_t *cfg;

	char name[MCE_SHORT];
	int paramset_count;

} cardtype_t;


typedef struct {
	
	const config_setting_t *cfg;

	int id;
	char name[MCE_SHORT];
	int type;

	int count;
	int card_count;

	int flags;
	u32 min;
	u32 max;
	config_setting_t *defaults;

} param_t;


typedef struct {

	const config_setting_t *cfg;

	int id;
	char name[MCE_SHORT];
	char type[MCE_SHORT];
	u32 error_bits;

	int flags;
	int card_count;

} card_t;


typedef struct {

	card_t card;
	param_t param;

} mce_param_t;



/* Basic access; most users only need these functions */

int mceconfig_load(const char *filename, mceconfig_t **mce);

int mceconfig_destroy(mceconfig_t *mce);

void mceconfig_print(const mceconfig_t* mce, FILE *out);

char* mceconfig_errstr(void);

int mceconfig_lookup(const mceconfig_t *mce,
		     const char *card_name, const char *para_name,
		     card_t *c, param_t *p);

int mceconfig_check_data(const card_t *c, const param_t *p, int count,
			 const u32 *data, unsigned block_flags,
			 char *errmsg);


/* mceconfig_cfg_<T> - attempt to load data into <T> from a config_setting_t */

int mceconfig_cfg_cardtype(const config_setting_t *cfg, cardtype_t *ct);

int mceconfig_cfg_paramset(const config_setting_t *cfg, paramset_t *ps);

int mceconfig_cfg_param(const config_setting_t *cfg, param_t *p);

int mceconfig_cfg_card(const config_setting_t *cfg, card_t *c);


/* Loading of root data from configuration */

int mceconfig_cardtype(const mceconfig_t *mce,
		       int index,
		       cardtype_t *ct);

int mceconfig_paramset(const mceconfig_t *mce,
		       int index,
		       paramset_t *ps);

int mceconfig_card(const mceconfig_t *mce,
		   int index,
		   card_t *c);


/* Loading of children from parents */

int mceconfig_card_cardtype(const mceconfig_t *mce,
			    const card_t *c,
			    cardtype_t *ct);

int mceconfig_cardtype_paramset(const mceconfig_t *mce, 
				const cardtype_t *ct,
				int index,
				paramset_t *ps);

int mceconfig_paramset_param(const mceconfig_t *mce, 
			     const paramset_t *ps,
			     int index,
			     param_t *p);


#endif
