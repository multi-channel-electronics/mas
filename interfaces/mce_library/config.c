#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libconfig.h>

#include "mcecmd.h"
#include "mceconfig.h"

/*
  With this library you have to be able to:

  - raw lookup by name (load any card, cardtype, paramset, param)

  - traverse cards, cardtypes, paramsets, params one-by-one

  - quick load of "card", "param" pairs.

*/
   

/* Local prototypes... */

static config_setting_t*
get_setting_by_name(const config_setting_t *parent, const char *name);

static config_setting_t*
get_setting(const config_setting_t *parent, const char *name);

static int
get_string(char *dest, const config_setting_t *parent, const char *name);

static int 
get_int(int *dest, const config_setting_t *parent, const char *name);

static int
get_int_elem(int *dest, const config_setting_t *parent, const char *name, int index);

static int
get_string_elem(char *dest, const config_setting_t *parent, const char *name, int index);

static int
count_elem(const config_setting_t *parent, const char *name);


/*! \fn mceconfig_load(char *filename, mceconfig_t **mce)
 *
 *  \brief Loads hardware configuration data from the given filename
 *  into the given mceconfig_t structure.
 *
 *  The call fails if filename is not a valid libconfig file, and also
 *  if the basic elements (hardware group and 3 sub-lists) are not
3~ *  found.
 *
 *  Returns 0 on success, non-zero on failure.
 */

int mceconfig_load(const char *filename, mceconfig_t **mce)
{
	(*mce) = (mceconfig_t*)malloc(sizeof(mceconfig_t));
	struct config_t *cfg = &(*mce)->cfg;
	mceconfig_t *_mce = *mce;

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
	
	_mce->parameter_sets =
		config_setting_get_member(hardware, "parameter_sets");
	_mce->card_types =
		config_setting_get_member(hardware, "card_types");
	config_setting_t *system =
		config_setting_get_member(hardware, "system");

	if (_mce->parameter_sets==NULL ||
	    _mce->card_types==NULL || system==NULL) {
		printf("Configuration file incomplete.\n");
		return 1;
	}

	_mce->components = config_setting_get_member(system, "components");
	if (_mce->components==NULL) {
		printf("System has no components!\n");
		return 1;
	}

	_mce->paramset_count = count_elem(hardware, "parameter_sets");
	_mce->cardtype_count = count_elem(hardware, "card_types");
	_mce->card_count = count_elem(system, "components");
	
	return 0;
}

int mceconfig_destroy(mceconfig_t *mce)
{
	config_destroy(&mce->cfg);
	return 0;
}


/*

  mceconfig_<type>(mceconfig_t *mce, int index, <type>_t *d)

   - Loads basic elements from the configuration "mce"
   - The element at index "index" is loaded.
   - The <type> is one of cardtype, paramset, card

*/

int mceconfig_cardtype(const mceconfig_t *mce,
		       int index,
		       cardtype_t *ct)
{
	config_setting_t *cfg =
		config_setting_get_elem(mce->parameter_sets, index);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_cardtype(cfg, ct);
}

int mceconfig_paramset(const mceconfig_t *mce,
		       int index,
		       paramset_t *ps)
{
	config_setting_t *cfg =
		config_setting_get_elem(mce->parameter_sets, index);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_paramset(cfg, ps);
}

int mceconfig_card(const mceconfig_t *mce,
		   int index,
		   card_t *c)
{
	config_setting_t *cfg =
		config_setting_get_elem(mce->components, index);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_card(cfg, c);
}


/*

  mceconfig_<parent>_<child>(mceconfig_t *mce, <parent>_t *p, [int index,] <child>_t *c)

   - Loads children of type <child> from the parent of type <parent>
   - Parent : child relationships are:
      card : cardtype (1-1, no index argument)
      cardtype : paramset
      paramset : param

*/

int mceconfig_card_cardtype(const mceconfig_t *mce,
			    const card_t *c,
			    cardtype_t *ct)
{
	char child_name[MCE_SHORT];

	if (get_string(child_name, c->cfg, "card_type")!=0) {
		fprintf(stderr, "Card does not have 'card_type' entry.\n");
		return -1;
	}

	config_setting_t* cfg =
		get_setting_by_name(mce->card_types, child_name);
	if (cfg==NULL)  {
		fprintf(stderr, "Card type '%s' not found.\n", child_name);
		return -1;
	}

	return mceconfig_cfg_cardtype(cfg, ct);
}

int mceconfig_cardtype_paramset(const mceconfig_t *mce, 
				const cardtype_t *ct,
				int index,
				paramset_t *ps)
{
	char child_name[MCE_SHORT];
	if (get_string_elem(child_name, ct->cfg, "parameter_sets", index))
		return -1;

	// Find parameter set called child_name
	config_setting_t *cfg = 
		get_setting_by_name(mce->parameter_sets, child_name);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_paramset(cfg, ps);
}

int mceconfig_paramset_param(const mceconfig_t *mce, 
			     const paramset_t *ps,
			     int index,
			     param_t *p)
{
	config_setting_t *plist =
		config_setting_get_member(ps->cfg, "parameters");
	if (plist==NULL) return -1;
	
	// Seek index
	config_setting_t *cfg = config_setting_get_elem(plist, index);
	if (cfg==NULL) return -1;
	
	// Copy p data for user
	return mceconfig_cfg_param(cfg, p);
}


/*

  mceconfig_cfg_<type>(config_setting_t *cfg, <type>_t *d)

  - Attempt to load the config data in cfg into the special <type> structure.
  - Valid <type> are card, cardtype, paramset, param.

*/

int mceconfig_cfg_card(const config_setting_t *cfg, card_t *c)
{
	// Fill with defaults
	c->cfg = cfg;
	c->name[0] = 0;
	c->error_bits = 0;
	c->id = -1;
	c->card_count = 1;
	
	// Update from key, maybe
	if (cfg == NULL) return -1;

	get_string(c->name, cfg, "name");
	get_string(c->type, cfg, "card_type");
	get_int((int*)&c->error_bits, cfg, "error_bits");
	get_int(&c->id, cfg, "id");
	get_int(&c->card_count, cfg, "card_count");

	return 0;
}

int mceconfig_cfg_cardtype(const config_setting_t *cfg, cardtype_t *ct)
{
	ct->cfg = cfg;
	ct->name[0] = 0;
	get_string(ct->name, cfg, "name");
	ct->paramset_count = count_elem(cfg, "parameter_sets");

	return 0;
}

int mceconfig_cfg_paramset(const config_setting_t *cfg, paramset_t *ps)
{
	ps->cfg = cfg;
	ps->name[0] = 0;
	get_string(ps->name, cfg, "name");
	ps->param_count = count_elem(cfg, "parameters");

	return 0;
}

int mceconfig_cfg_param(const config_setting_t *cfg, param_t *p)
{
	if (cfg == NULL) return -1;

	// Fill with defaults
	p->cfg = cfg;
	p->id = -1;
	p->type = 0;
	p->count = 1;
	p->id_count = 1;
	p->card_count = 1;
	p->flags = 0;
	p->name[0] = 0;

	get_string(p->name, cfg, "name");
	get_int(&p->id, cfg, "id");
	get_int(&p->count, cfg, "count");
	get_int(&p->id_count, cfg, "id_count");
	get_int(&p->card_count, cfg, "card_count");

	if (get_int((int*)&p->min, cfg, "min")==0)
		p->flags |= MCE_PARAM_MIN;
	if (get_int((int*)&p->max, cfg, "max")==0)
		p->flags |= MCE_PARAM_MAX;
	p->defaults = config_setting_get_member(cfg, "defaults");
	if (p->defaults != NULL)
		p->flags |= MCE_PARAM_DEF;

	return 0;
}


/*

  mceconfig_lookup_<child>(const mceconfig_t *mce, [const <parent>_t *p, ]
                           const char *child_name, <child>_t *c)

  - Lookup a config setting by name
  - in the case of param, the parent param_set must also be passed

*/

int  mceconfig_lookup_card(const mceconfig_t *mce, const char *card_name,
			   card_t *card)
{
	return mceconfig_cfg_card(
		get_setting_by_name(mce->components, card_name),
		card );
}

int  mceconfig_lookup_cardtype(const mceconfig_t *mce, const char *cardtype_name,
			       cardtype_t *cardtype)
{
	return mceconfig_cfg_cardtype(
		get_setting_by_name(mce->card_types, cardtype_name),
		cardtype );
}

int  mceconfig_lookup_paramset(const mceconfig_t *mce, const char *paramset_name,
			       paramset_t *paramset)
{
	return mceconfig_cfg_paramset(
		get_setting_by_name(mce->parameter_sets, paramset_name),
		paramset );
}

int  mceconfig_lookup_param(const mceconfig_t *mce, const paramset_t *paramset,
			    const char *param_name, param_t *param)
{
	config_setting_t *p_list =
		config_setting_get_member(paramset->cfg, "parameters");

	return mceconfig_cfg_param(
		get_setting_by_name(p_list, param_name),
		param);
}



/*

  mceconfig_lookup - easy way to lookup "card" "param" combination.

*/

int mceconfig_lookup(const mceconfig_t *mce,
		     const char *card_name, const char *para_name,
		     card_t *c, param_t *p)
{
	cardtype_t ct;
	paramset_t ps;
	int index;

	if ((p==NULL) || (mce==NULL)) return -1;
		
	p->flags = 0;

	if (mceconfig_lookup_card(mce, card_name, c)) return -2;
	if (mceconfig_card_cardtype(mce, c, &ct)) return -3;

	for (index=0; index < ct.paramset_count; index++) {
		if (mceconfig_cardtype_paramset(mce, &ct, index, &ps))
			return -4;

		if (mceconfig_lookup_param(mce, &ps, para_name, p)==0)
			return 0;
	}
	return -5;
}

int mceconfig_check_data(const card_t *c, const param_t *p, int count,
			 const u32 *data, unsigned block_flags,
			 char *errmsg)
{
	int i;

	int *datai = (int *)data;

	if (count > p->count) {
		sprintf(errmsg, "too many data");
		return -1;
	}

	if ( (block_flags & MCE_PARAM_WONLY) && (p->flags & MCE_PARAM_WONLY) ) {
		sprintf(errmsg, "parameter is write only");
		return -1;
	}

	if ( (block_flags & MCE_PARAM_RONLY) && (p->flags & MCE_PARAM_RONLY) ) {
		sprintf(errmsg, "parameter is read only");
		return -1;
	}

	for (i=0; (p->flags & MCE_PARAM_MIN) && (i < count); i++) {
		if ( !(p->flags & MCE_PARAM_SIGNED) && (data [i] < p->min) ) {
			sprintf(errmsg, "minimum of %u exceeded", p->min);
			return -1;
		} else if ( (p->flags & MCE_PARAM_SIGNED) && (datai[i] < p->min)) {
			sprintf(errmsg, "minimum of %u exceeded", p->min);
			return -1;
		}
	}

	for (i=0; (p->flags & MCE_PARAM_MAX) && (i < count); i++) {
		if ( !(p->flags & MCE_PARAM_SIGNED) && (data [i] > p->max) ) {
			sprintf(errmsg, "maximum of %u exceeded", p->max);
			return -1;
		} else if ( (p->flags & MCE_PARAM_SIGNED) && (datai[i] > p->max)) {
			sprintf(errmsg, "maximum of %u exceeded", p->max);
			return -1;
		}
	}

	return 0;
}


/*

  Helper functions for libconfig

*/

config_setting_t *get_setting_by_name(const config_setting_t *parent,
				      const char *name)
{
	// Find card
	config_setting_t *child;
	char child_name[256];
	int index;
	for (index=0; (child=config_setting_get_elem(parent, index))
		     != NULL; index++) {
		if (get_string(child_name, child, "name")!=0) {
			fprintf(stderr, "'name' not found in item %i of list\n", index);
			return NULL;
		}
/* 		printf("%s %s\n", name, child_name); */
		if (strcmp(name, child_name)==0)
			return child;
	}
	return NULL;
}

inline config_setting_t *get_setting(const config_setting_t *parent,
				     const char *name)
{
	return config_setting_get_member(parent, name);
}

int get_int_elem(int *dest, const config_setting_t *parent,
		 const char *name, int index)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	set = config_setting_get_elem(set, index);
	if (set==NULL) return -1;
	*dest = config_setting_get_int(set);
	return 0;
}

int get_int(int *dest, const config_setting_t *parent, const char *name)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	*dest = config_setting_get_int(set);
	return 0;
}

int get_string_elem(char *dest, const config_setting_t *parent,
		    const char *name, int index)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	set = config_setting_get_elem(set, index);
	if (set==NULL) return -1;
	strcpy(dest, config_setting_get_string(set));
	return 0;
}

int get_string(char *dest, const config_setting_t *parent, const char *name)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	strcpy(dest, config_setting_get_string(set));
	return 0;
}

int count_elem(const config_setting_t *parent, const char *name)
{
	int count = 0;
	config_setting_t *list = config_setting_get_member(parent, name);
	if (list==NULL) return 0;
	
	while (config_setting_get_elem(list, count)!=NULL)
		count++;
	return count;
}

