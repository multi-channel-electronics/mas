/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
 /******************************************************
  mceconfig.h - header file for MCE API, config module

  Matthew Hasselfield, 08.01.02

******************************************************/

#ifndef _MCECONFIG_H_
#define _MCECONFIG_H_

#include <stdint.h>

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

#include <libconfig.h>


/* Parameter type enumerator */

#define MCE_CMD_MEM 0
#define MCE_CMD_CMD 1
#define MCE_CMD_RST 2
#define MCE_CMD_OTH 4


/* Card nature enumerator */
typedef enum {
	MCE_NATURE_PHYSICAL,
	MCE_NATURE_VIRTUAL
} mce_cardnature_t;


/* Parameter flag bits */

#define MCE_PARAM_MIN    0x00000001 /* has minimum */
#define MCE_PARAM_MAX    0x00000002 /* has maximum */
#define MCE_PARAM_DEF    0x00000004 /* has defaults */
#define MCE_PARAM_PARAM  0x00000010
#define MCE_PARAM_CARD   0x00000020
#define MCE_PARAM_STAT   0x00000040 /* should be included in status */
#define MCE_PARAM_WONLY  0x00000080 /* write only */
#define MCE_PARAM_RONLY  0x00000100 /* read only */
#define MCE_PARAM_SIGNED 0x00000200 /* is naturally signed */
#define MCE_PARAM_HEX    0x00000400 /* is naturally hexadecimal */
#define MCE_PARAM_MULTI  0x00000800 /* multi-card alias */
#define MCE_PARAM_MAPPED 0x00001000 /* virtual parameter */
#define MCE_PARAM_MANIP  0x00002000 /* data manipulation */

/* Maximum number of grouped cards */

#define MCE_MAX_CARDSET  10


/* Default config file keys */

#define MCECFG_HARDWARE           "hardware"          /* top-level default */

#define MCECFG_PARAMSETS          "parameter_sets"    /* 2nd level */
#define MCECFG_CARDTYPES          "card_types"        /* 2nd level */
#define MCECFG_MAPPINGS           "mappings"          /* 2nd level */
#define MCECFG_SYSTEM             "system"            /* 2nd level */

#define MCECFG_COMPONENTS         "components"        /* Child of SYSTEM */


/* Structures for config file entry information  */

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

	char name[MCE_SHORT];
	int param_count;

} mapping_t;


typedef struct {

	int start;
	int count;
	char card_name[MCE_SHORT];
	char param_name[MCE_SHORT];
	int offset;

} maprange_t;

typedef struct {
	
	const config_setting_t *cfg;

	int id;
	char name[MCE_SHORT];
	int type;

	int count;

	int flags;
    uint32_t min;
    uint32_t max;
	config_setting_t *defaults;

	int map_count;
	config_setting_t *maps;

	int op_xor;
} param_t;


typedef struct {

	const config_setting_t *cfg;

	int id[MCE_MAX_CARDSET];
	mce_cardnature_t nature;

	char name[MCE_SHORT];
	char type[MCE_SHORT];
    uint32_t error_bits;

	int flags;
	int card_count;
	
} card_t;


/* Container for param/card pairs */

typedef struct {
	
	card_t card;
	param_t param;
	
} mce_param_t;



/* Open and close of configuration file */

int mceconfig_open(mce_context_t* context, 
		   const char *filename, const char *keyname);

int mceconfig_close(mce_context_t* context);
		    
		    
/* Lookup routine, for decoding "cc fw_rev" type string-pairs */
		    
int mceconfig_lookup(const mce_context_t *mce,
		     const char *card_name, const char *para_name,
		     card_t *c, param_t *p);

		    
/* Static function for verifying that data obeys parameter constraints */

int mceconfig_check_data(const card_t *c, const param_t *p, int count,
        const uint32_t *data, unsigned block_flags, char *errmsg);


/* Loading of root data from configuration */

int mceconfig_cardtype_count(const mce_context_t* context);

int mceconfig_paramset_count(const mce_context_t* context);

int mceconfig_card_count    (const mce_context_t* context);
		    
int mceconfig_cardtype(const mce_context_t *context, int index,
		       cardtype_t *ct);

int mceconfig_paramset(const mce_context_t *context, int index,
		       paramset_t *ps);

int mceconfig_card    (const mce_context_t *context, int index,
		       card_t *c);
		    

/* Loading of children from parents */

int mceconfig_card_mapping     (const mce_context_t *context,
			        const card_t *c,
				mapping_t *m);

int mceconfig_mapping_param    (const mce_context_t* context, 
			        const mapping_t *m,
				int index,
				param_t *p);

int mceconfig_param_maprange   (const mce_context_t* context, 
			        const param_t *p,
				int index,
				maprange_t *mr);

int mceconfig_card_cardtype    (const mce_context_t *context,
			        const card_t *c,
				cardtype_t *ct);

int mceconfig_cardtype_paramset(const mce_context_t *context, 
				const cardtype_t *ct, int index,
				paramset_t *ps);

int mceconfig_paramset_param   (const mce_context_t *context, 
				const paramset_t *ps, int index,
				param_t *p);

int mceconfig_card_param       (const mce_context_t *context,
				const card_t *c, int index,
				param_t *p);
				
int mceconfig_card_paramcount(const mce_context_t *context,
			      const card_t *c);


/* mceconfig_cfg_<T> - attempt to load data into <T> from a config_setting_t */

int mceconfig_cfg_mapping (const config_setting_t *cfg, mapping_t  *m);

int mceconfig_cfg_cardtype(const config_setting_t *cfg, cardtype_t *ct);

int mceconfig_cfg_paramset(const config_setting_t *cfg, paramset_t *ps);

int mceconfig_cfg_param   (const config_setting_t *cfg, param_t    *p);

int mceconfig_cfg_card    (const config_setting_t *cfg, card_t     *c);

int mceconfig_cfg_maprange(const config_setting_t *cfg, maprange_t *mr);


#endif
