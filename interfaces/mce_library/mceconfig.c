#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libconfig.h>

#include "mcecmd.h"
#include "mceconfig.h"

/*
  With this library you have to be able to:

   - look up any system, system-component, card_type, or parameter set
     by name

   - traverse each of the above by index or list.

   - look up have form lookup(parent, name)

   - traversal have form item(parent, index)

   - decode form for simple items, e.g. cardtype(card)

   - root is mceconfig

   - options are card cardtype paramset param

   - Loop over all params in a card:

   card_t * card = mceconfig_lookup_card(mce_data, "cc");
   cardtype_t *cardtype = mceconfig_cardtype(card);
   
   for (i=0; i<cardtype.paramset_count; i++) {
     paramset_t *pset = mceconfig_item_paramset(cardtype, i);
     for (j=0; j<pset.param_count; j++) {
       param_properties_t *param = mceconfig_item_param(pset, j);
       //...
     }
   }

*/
   

/* Local prototypes... */

static config_setting_t*
get_setting_by_name(config_setting_t *parent, const char *name);

static config_setting_t*
get_setting(config_setting_t *parent, const char *name);

/* get_<type> ( <type> *dest, config_setting_t *parent, const char *name);
   - lookup key "name" in setting "parent", load result into "dest"        */

/* get_<type>_elem ( <type> *dest, config_setting_t *parent,
                     const char *name, int idx);
   - lookup key "name" in setting "parent", treat it as a list and load
     the idx-th element into "dest"                                        */


static int
get_string(char *dest, config_setting_t *parent, const char *name);

static int 
get_int(int *dest, config_setting_t *parent, const char *name);

static int
get_int_elem(int *dest, config_setting_t *parent, const char *name, int idx);

static int
get_string_elem(char *dest, config_setting_t *parent, const char *name, int idx);

int count_elem(config_setting_t *parent, char *name);


int load_as_cardtype(cardtype_t *ct, config_setting_t *cfg);
int load_as_card(card_t *card, config_setting_t *cfg);
int load_as_param(param_t *p, config_setting_t *cfg);
int load_as_paramset(paramset_t *ps, config_setting_t *cfg);


config_setting_t*
mceconfig_lookup_card(mce_data_t *mce, const char *card_str, int *card_id);

config_setting_t*
mceconfig_lookup_para(mce_data_t *mce, config_setting_t *card,
		      const char *para_str, int *para_id);


/* Basic look-up and indexing */

/*  mceconfig_cardtype_paramset - get paramset[idx] in cardtype ct */
int mceconfig_cardtype_paramset(mce_data_t *mce, 
				cardtype_t *ct,
				int idx,
				paramset_t *ps)
{
	char pset_name[MCE_SHORT];
	if (get_string_elem(pset_name, ct->cfg, "parameter_sets", idx))
		return -1;

	// Find parameter set called pset_name
	config_setting_t *ps_cfg = 
		get_setting_by_name(mce->parameter_sets, pset_name);
	if (ps_cfg==NULL) return -1;

	// Copy pset data for user
	return load_as_paramset(ps, ps_cfg);
}

/*  mceconfig_paramset_param - get param[idx] in paramset ps */
int mceconfig_paramset_param(mce_data_t *mce, 
			     paramset_t *ps,
			     int idx,
			     param_t *p)
{
	// Get list
	config_setting_t *plist =
		config_setting_get_member(ps->cfg, "parameters");
	if (plist==NULL) return -1;

	// Seek index
	config_setting_t *p_cfg = 
		config_setting_get_elem(plist, idx);
	if (p_cfg==NULL) return -1;

	// Copy p data for user
	return load_as_param(p, p_cfg);
}


int mceconfig_cardtype(mce_data_t *mce,
		       int idx,
		       cardtype_t *ct)
{
	config_setting_t *cfg =
		config_setting_get_elem(mce->parameter_sets, idx);
	if (cfg==NULL) return -1;

	return load_as_cardtype(ct, cfg);
}


int mceconfig_paramset(mce_data_t *mce,
		       int idx,
		       paramset_t *ps)
{
	config_setting_t *cfg =
		config_setting_get_elem(mce->parameter_sets, idx);
	if (cfg==NULL) return -1;

	return load_as_paramset(ps, cfg);
}


/*  mceconfig_card - get card[idx] in system/components      */
int mceconfig_card(mce_data_t *mce,
		   int idx,
		   card_t *c)
{
	config_setting_t *cfg =
		config_setting_get_elem(mce->components, idx);
	if (cfg==NULL) return -1;

	return load_as_card(c, cfg);
}

/*  mceconfig_card_cardtype - get cardtype associated with card */
int mceconfig_card_cardtype(mce_data_t *mce,
			    card_t *c,
			    cardtype_t *ct)
{
	char cardtype_str[MCE_SHORT];
	if (get_string(cardtype_str, c->cfg, "card_type")!=0) return -1;

	config_setting_t* cfg =
		get_setting_by_name(mce->card_types, cardtype_str);

	return load_as_cardtype(ct, cfg);
}

/* int mceconfig_lookup_cardtype(mce_data_t *mce, */
/* 			      const char *name, */
/* 			      cardtype_t *ct) */
/* { */
/* 	for  */
/* } */

int mceconfig_cfg_cardtype(cardtype_t *ct, config_setting_t *cfg)
{
	return load_as_cardtype(ct, cfg);
}

int mceconfig_cfg_paramset(paramset_t *ps, config_setting_t *cfg)
{
	return load_as_paramset(ps, cfg);
}

int mceconfig_cfg_param(param_t *p, config_setting_t *cfg)
{
	return load_as_param(p, cfg);
}

int mceconfig_cfg_card(card_t *card, config_setting_t *cfg)
{
	return load_as_card(card, cfg);
}


int load_as_cardtype(cardtype_t *ct, config_setting_t *cfg)
{
	ct->cfg = cfg;
	ct->name[0] = 0;
	get_string(ct->name, cfg, "name");
	ct->paramset_count = count_elem(cfg, "parameter_sets");

	return 0;
}

int load_as_paramset(paramset_t *ps, config_setting_t *cfg)
{
	ps->cfg = cfg;
	ps->name[0] = 0;
	get_string(ps->name, cfg, "name");
	ps->param_count = count_elem(cfg, "parameters");

	return 0;
}

int load_as_param(param_t *p, config_setting_t *cfg)
{
	if (cfg == NULL) return -1;

	// Fill with defaults
	p->cfg = cfg;
	p->id = -1;
	p->type = 0;
	p->count = 1;
	p->id_count = 1;
	p->flags = 0;
	p->name[0] = 0;

	get_string(p->name, cfg, "name");
	get_int(&p->id, cfg, "id");
	get_int(&p->count, cfg, "count");
	get_int(&p->id_count, cfg, "id_count");

	if (get_int(&p->min, cfg, "min")==0)
		p->flags |= PARAM_MIN;
	if (get_int(&p->max, cfg, "max")==0)
		p->flags |= PARAM_MAX;
	p->defaults = config_setting_get_member(cfg, "defaults");
	if (p->defaults != NULL)
		p->flags |= PARAM_DEF;

	return 0;
}

int load_as_card(card_t *card, config_setting_t *cfg)
{
	// Fill with defaults
	card->cfg = cfg;
	card->name[0] = 0;
	card->error_bits = 0;
	card->id = -1;
	
	// Update from key, maybe
	if (cfg == NULL) return -1;

	get_string(card->name, cfg, "name");
	get_string(card->type, cfg, "card_type");
	get_int((int*)&card->error_bits, cfg, "error_bits");
	get_int(&card->id, cfg, "id");

	return 0;
}

int mceconfig_get_card(mce_data_t *mce, card_t *card, int idx)
{
	if (mce==NULL || mce->components==NULL) return -1;
	
	config_setting_t *comps = mce->components;
	
	// Update from key, maybe
	
	card->cfg = config_setting_get_elem(comps, idx);
	if (card->cfg == NULL) return -1;

	load_as_card(card, card->cfg);

	return 0;
}


int mceconfig_get_cardtype(mce_data_t *mce, cardtype_t *type, int idx)
{
	if (mce==NULL || mce->card_types==NULL) return -1;

	type->cfg = NULL;
	type->name[0] = 0;
	type->paramset_count = 0;

	type->cfg = config_setting_get_elem(mce->card_types, idx);
	if (type->cfg==NULL) return -1;

	get_string(type->name, type->cfg, "name");
	type->paramset_count = count_elem(type->cfg, "parameter_sets");

	return 0;
}


int mceconfig_get_paramset(mce_data_t *mce, paramset_t *pset, int idx)
{
	if (mce==NULL || mce->parameter_sets==NULL) return -1;

	pset->cfg = NULL;
	pset->name[0] = 0;
	pset->param_count = 0;

	pset->cfg = config_setting_get_elem(mce->parameter_sets, idx);
	if (pset->cfg==NULL) return -1;

	get_string(pset->name, pset->cfg, "name");
	pset->param_count = count_elem(pset->cfg, "parameters");

	return 0;
}

int mceconfig_get_param(mce_data_t *mce, card_t *card, int idx)
{
/* 	int eaten = 0; */
/* 	config_setting_t *type_cfg = */
/* 		get_setting_by_name(card->cfg, card->type); */
	return 0;
	
}


/*! \fn mceconfig_load(char *filename, mce_data_t **mce)
 *
 *  \brief Loads hardware configuration data from the given filename
 *  into the given mce_data_t structure.
 *
 *  The call fails if filename is not a valid libconfig file, and also
 *  if the basic elements (hardware group and 3 sub-lists) are not
 *  found.
 *
 *  Returns 0 on success, non-zero on failure.
 */

int mceconfig_load(char *filename, mce_data_t **mce)
{
	*mce = (mce_data_t*)malloc(sizeof(mce_data_t));
	struct config_t *cfg = &(*mce)->cfg;

	config_init(cfg);
	if (!config_read_file(cfg, filename)) {
		printf("config_read_file '%s': line %i: %s\n",
		       filename, config_error_line(cfg),
		       config_error_text(cfg));
		return -1;
	}
		
	//Find hardware group
	config_setting_t *hardware = config_lookup(cfg, "hardware");
	if (hardware == NULL) {
		printf("Could not find hardware.\n");
		return 1;
	}
	
	(*mce)->parameter_sets =
		config_setting_get_member(hardware, "parameter_sets");
	(*mce)->card_types =
		config_setting_get_member(hardware, "card_types");
	config_setting_t *system =
		config_setting_get_member(hardware, "system");

	if ((*mce)->parameter_sets==NULL ||
	    (*mce)->card_types==NULL || system==NULL) {
		printf("Configuration file incomplete.\n");
		return 1;
	}

	(*mce)->components =
		config_setting_get_member(system, "components");
	if ((*mce)->components==NULL) {
		printf("System has no components!\n");
		return 1;
	}
	
	return 0;
}

int mceconfig_destroy(mce_data_t *mce)
{
	config_destroy(&mce->cfg);
	return 0;
}


/*********************************
  CHANGE ALL OF THESE  V
*********************************/

int mceconfig_lookup(param_properties_t *p, mce_data_t *mce,
		     const char *card_str, const char *para_str,
		     int para_idx) {

	if ((p==NULL) || (mce==NULL)) return -1;
		
	p->flags = 0;
	config_setting_t *card =
		mceconfig_lookup_card(mce, card_str,
				      &p->card_id);
	if (card==NULL) return -1;
	p->flags |= PARAM_CARD;	

	config_setting_t *para =
		mceconfig_lookup_para(mce, card, para_str, &p->para_id);

	if (para==NULL) return -2;

	int id_count = 1;
	get_int(&id_count, para, "id_count");
	if (para_idx>=id_count) return -3;
	
	p->card_count = 1;
	get_int(&p->card_count, card, "cards_count");

	p->para_id += para_idx;
	p->flags |= PARAM_PARAM;

	p->count = 1;
	get_int(&p->count, para, "count");

	if ( (get_int(&p->def_val, para, "default")==0) ||
	     (get_int_elem(&p->def_val, para, "default_items", para_idx)==0) )
		p->flags |= PARAM_DEF;
	
	if (get_int(&p->minimum, para, "min")==0)
		p->flags |= PARAM_MIN;
	if (get_int(&p->maximum, para, "max")==0)
	     p->flags |= PARAM_MAX;
	if (get_int(&p->maximum, para, "max")==0)
	     p->flags |= PARAM_MAX;
	
	return 0;
}

 
config_setting_t *mceconfig_lookup_card(mce_data_t *mce, const char *card_str, int *card_id)
{
	// Find card
	config_setting_t *card = get_setting_by_name(mce->components, card_str);
	if (card==NULL) {
		printf("Could not match card '%s'\n", card_str);
		return NULL;
	}

	if (get_int(card_id, card, "id")!=0) {
		printf("system: component has no id\n");
		return NULL;
	}
	//printf("card_id = %#0x\n", *card_id);
	return card;
}


config_setting_t *mceconfig_lookup_para(mce_data_t *mce,
					config_setting_t *card,
					const char *para_str, int *para_id)
{
	char card_t_name[256];
	if (get_string(card_t_name, card, "card_type") != 0) {
		printf("system: component has no card_type\n");
		return NULL;
	}

	//Find this card type in the cards list
	config_setting_t *card_t = 
		get_setting_by_name(mce->card_types, card_t_name);
	if (card_t==NULL) {
		printf("system: card_type '%s' not listed\n", card_t_name);
		return NULL;
	}
	
	config_setting_t *pset_list =
		config_setting_get_member(card_t, "parameter_sets");
	if (pset_list==NULL) {
		printf("system: component has no parameter sets\n");
		return NULL;
	}
	const char *pset_name;
	int sidx;	
	for (sidx = 0;
	     (pset_name=config_setting_get_string_elem(pset_list, sidx)) != NULL;
	     sidx++) {

		config_setting_t *pset =
			get_setting_by_name(mce->parameter_sets, pset_name);
		if (pset==NULL) {
			printf("Could not find parameter_set '%s'\n", pset_name);
			return NULL;
		}
		
		config_setting_t *params =
			config_setting_get_member(pset, "parameters");
		if (params==NULL) {
			printf("parameter_sets has no parameters list!\n");
			return NULL;
		}

		// Now search for our key
		config_setting_t *p = get_setting_by_name(params, para_str);
		if (p != NULL) {
			if (get_int(para_id, p, "id")!=0) {
				printf("param does not have id!\n");
			}
			//printf("param_id = %#02x\n", *para_id);
			return p;
		}
	}

	return NULL;
}

config_setting_t *get_setting_by_name(config_setting_t *parent,
				      const char *name) {

	// Find card
	config_setting_t *child;
	char child_name[256];
	int idx;
	for (idx=0; (child=config_setting_get_elem(parent, idx))
		     != NULL; idx++) {
		if (get_string(child_name, child, "name")!=0) {
			printf("expected name in item %i of list\n", idx);
			return NULL;
		}
		if (strcmp(name, child_name)==0)
			return child;
	}
	return NULL;
}

inline config_setting_t *get_setting(config_setting_t *parent, const char *name)
{
	config_setting_t *set =
		config_setting_get_member(parent, name);
/* 	if (set==NULL) { */
/* 		fprintf(stderr,  */
/* 			"key '%s' not found in config file\n", */
/* 			name); */
/* 		return NULL; */
/* 	} */
	return set;
}

int get_int_elem(int *dest, config_setting_t *parent,
		 const char *name, int idx)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	set = config_setting_get_elem(set, idx);
	if (set==NULL) return -1;
	*dest = config_setting_get_int(set);
	return 0;
}

int get_int(int *dest, config_setting_t *parent, const char *name)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	*dest = config_setting_get_int(set);
	return 0;
}

int get_string_elem(char *dest, config_setting_t *parent,
		    const char *name, int idx)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	set = config_setting_get_elem(set, idx);
	if (set==NULL) return -1;
	strcpy(dest, config_setting_get_string(set));
	return 0;
}

int get_string(char *dest, config_setting_t *parent, const char *name)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	strcpy(dest, config_setting_get_string(set));
	return 0;
}

int count_elem(config_setting_t *parent, char *name) {
	int count = 0;
	config_setting_t *list = config_setting_get_member(parent, name);
	if (list==NULL) return 0;
	
	while (config_setting_get_elem(list, count)!=NULL)
		count++;
	return count;
}

