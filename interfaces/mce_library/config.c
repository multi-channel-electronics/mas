/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "context.h"
#include <mce/defaults.h>

/* Local prototypes... */

static config_setting_t*
get_setting_by_name(const config_setting_t *parent, const char *name);

inline config_setting_t*
get_setting(const config_setting_t *parent, const char *name);

static int
get_string(char *dest, const config_setting_t *parent, const char *name);

static int
get_int(int *dest, const config_setting_t *parent, const char *name);

/* Unused
static int
get_int_elem(int *dest, const config_setting_t *parent, const char *name, int index);
*/

static int
get_int_array(int *dest, const config_setting_t *parent,
	      const char *name, int max);

static int
get_string_elem(char *dest, const config_setting_t *parent, const char *name, int index);

static int
count_elem(const config_setting_t *parent, const char *name);

/* String table functionality */

typedef struct {
	int id;
	char *name;
} string_table_t;

static
int st_get_id(const string_table_t *st, const char *name, int *id);

static
int st_index(const string_table_t *st, const char *name);


/* String tables */

string_table_t param_types_st[] = {
	{ MCE_CMD_MEM, "mem" },
	{ MCE_CMD_CMD, "cmd" },
	{ MCE_CMD_RST, "rst" },
	{ -1, NULL }
};


string_table_t card_natures_st[] = {
	{ MCE_NATURE_PHYSICAL, "physical" },
	{ MCE_NATURE_VIRTUAL , "virtual" },
	{ -1, NULL }
};


/*
  mceconfig_open
  mceconfig_close

  Make and break connections on the config module.
*/

static
int try_get_child(config_setting_t *parent, const char* name,
		  config_setting_t **child)
{
	*(child) = config_setting_get_member(parent, name);
	if (*(child)==NULL) {
		fprintf(stderr,
			"Section '%s' missing in config file.\n", name);
		return 1;
	}
	return 0;
}


int mceconfig_open(mce_context_t* context,
		   const char *filename, const char *keyname)
{
	const char* key = MCECFG_HARDWARE;
	struct config_t *cfg = &C_config.cfg;

    /* Get default hardware file, if necessary */
    if (filename == NULL)
        context->config.filename = mcelib_default_hardwarefile(context);
    else
        context->config.filename = strdup(filename);

    if (context->config.filename == NULL) {
        fprintf(stderr, "error setting hardware config file path.\n");
        return -1;
    }

    config_init(cfg);
    if (!config_read_file(cfg, context->config.filename)) {
        fprintf(stderr, "config_read_file '%s': line %i: %s\n",
                context->config.filename, config_error_line(cfg),
                config_error_text(cfg));
		return -1;
	}

	//Find hardware group
	if (keyname != NULL) key = keyname;
	config_setting_t *hardware = config_lookup(cfg, key);
	if (hardware == NULL) {
        fprintf(stderr, "Could not find key '%s' in file '%s'.\n",
                key, context->config.filename);
		return 1;
	}

	config_setting_t *system;

	if (try_get_child(hardware, MCECFG_PARAMSETS, &C_config.parameter_sets) ||
	    try_get_child(hardware, MCECFG_CARDTYPES, &C_config.card_types) ||
	    try_get_child(hardware, MCECFG_MAPPINGS , &C_config.mappings) ||
	    try_get_child(hardware, MCECFG_SYSTEM, &system)) {
		return 1;
	}

	if (try_get_child(system, MCECFG_COMPONENTS, &C_config.components)) {
		printf("System has no components!\n");
		return 1;
	}

	C_config.paramset_count = count_elem(hardware, MCECFG_PARAMSETS);
	C_config.cardtype_count = count_elem(hardware, MCECFG_CARDTYPES);
	C_config.mapping_count = count_elem(hardware, MCECFG_MAPPINGS);
	C_config.card_count = count_elem(system, MCECFG_COMPONENTS);

	C_config.connected = 1;
	return 0;
}

int mceconfig_close(mce_context_t* context)
{
	C_config_check;

	config_destroy(&C_config.cfg);
    free(context->config.filename);
	C_config.connected = 0;

	return 0;
}


/*

  mceconfig_<type>_count(mce_context_t *context)

   - Return a count of basic elements in hardware configuration

*/

int mceconfig_cardtype_count(const mce_context_t* context)
{
	C_config_check;
	return C_config.cardtype_count;
}

int mceconfig_paramset_count(const mce_context_t* context)
{
	C_config_check;
	return C_config.paramset_count;
}

int mceconfig_card_count    (const mce_context_t* context)
{
	C_config_check;
	return C_config.card_count;
}


/*
  mceconfig_<type>(mce_context_t *context, int index, <type>_t *d)

   - Loads basic elements from the configuration "mce"
   - The element at index "index" is loaded.
   - The <type> is one of cardtype, paramset, card

*/

int mceconfig_mapping(const mce_context_t* context,
		      int index, mapping_t *mapping)
{
	config_setting_t *cfg =
		config_setting_get_elem(C_config.mappings, index);
	if (cfg==NULL) return -1;
	return mceconfig_cfg_mapping(cfg, mapping);
}

int mceconfig_cardtype(const mce_context_t* context,
		       int index,
		       cardtype_t *ct)
{
	config_setting_t *cfg =
		config_setting_get_elem(C_config.card_types, index);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_cardtype(cfg, ct);
}

int mceconfig_paramset(const mce_context_t* context,
		       int index,
		       paramset_t *ps)
{
	config_setting_t *cfg =
		config_setting_get_elem(C_config.parameter_sets, index);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_paramset(cfg, ps);
}

int mceconfig_card(const mce_context_t* context,
		   int index,
		   card_t *c)
{
	config_setting_t *cfg =
		config_setting_get_elem(C_config.components, index);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_card(cfg, c);
}


/*

  mceconfig_<parent>_<child>(mce_context_t* context, <parent>_t *p, [int index,] <child>_t *c)

   - Loads children of type <child> from the parent of type <parent>
   - Parent : child relationships are:
      card : cardtype (1-1, no index argument)
      cardtype : paramset
      paramset : param

*/

int mceconfig_card_mapping(const mce_context_t* context,
			   const card_t *c,
			   mapping_t *m)
{
	char child_name[MCE_SHORT];

	if (get_string(child_name, c->cfg, "mapping")!=0) {
		fprintf(stderr, "Card does not have 'mapping' entry.\n");
		return -1;
	}

	config_setting_t* cfg =
		get_setting_by_name(C_config.mappings, child_name);
	if (cfg==NULL)  {
		fprintf(stderr, "Mapping '%s' not found.\n", child_name);
		return -1;
	}

	return mceconfig_cfg_mapping(cfg, m);
}

int mceconfig_mapping_param(const mce_context_t* context,
			    const mapping_t *m,
			    int index,
			    param_t *p)
{
	config_setting_t *list =
		config_setting_get_member(m->cfg, "parameters");
	if (list==NULL) return -1;

	// Seek index
	config_setting_t *cfg = config_setting_get_elem(list, index);
	if (cfg==NULL) return -1;

	// Copy p data for user
	return mceconfig_cfg_param(cfg, p);
}

int mceconfig_param_maprange(const mce_context_t* context,
			     const param_t *p,
			     int index,
			     maprange_t *mr)
{
	config_setting_t *list =
		config_setting_get_member(p->cfg, "maps");
	if (list==NULL) return -1;

	// Seek index
	config_setting_t *cfg = config_setting_get_elem(list, index);
	if (cfg==NULL) return -1;

	// Copy p data for user
	return mceconfig_cfg_maprange(cfg, mr);
}

int mceconfig_card_cardtype(const mce_context_t* context,
			    const card_t *c,
			    cardtype_t *ct)
{
	char child_name[MCE_SHORT];

	if (get_string(child_name, c->cfg, "card_type")!=0) {
		fprintf(stderr, "Card does not have 'card_type' entry.\n");
		return -1;
	}

	config_setting_t* cfg =
		get_setting_by_name(C_config.card_types, child_name);
	if (cfg==NULL)  {
		fprintf(stderr, "Card type '%s' not found.\n", child_name);
		return -1;
	}

	return mceconfig_cfg_cardtype(cfg, ct);
}

int mceconfig_cardtype_paramset(const mce_context_t* context,
				const cardtype_t *ct,
				int index,
				paramset_t *ps)
{
	char child_name[MCE_SHORT];
	if (get_string_elem(child_name, ct->cfg, "parameter_sets", index))
		return -1;

	// Find parameter set called child_name
    config_setting_t *cfg =
		get_setting_by_name(C_config.parameter_sets, child_name);
	if (cfg==NULL) return -1;

	return mceconfig_cfg_paramset(cfg, ps);
}

int mceconfig_paramset_param(const mce_context_t* context,
			     const paramset_t *ps,
			     int index,
			     param_t *p)
{
	config_setting_t *list =
		config_setting_get_member(ps->cfg, "parameters");
	if (list==NULL) return -1;

	// Seek index
	config_setting_t *cfg = config_setting_get_elem(list, index);
	if (cfg==NULL) return -1;

	// Copy p data for user
	return mceconfig_cfg_param(cfg, p);
}

int mceconfig_card_param(const mce_context_t *context,
			 const card_t *c, int index,
			 param_t *p)
{
	int i;
	C_config_check;
	cardtype_t ct;
	paramset_t ps;
	mapping_t m;

	switch(c->nature) {

	case MCE_NATURE_PHYSICAL:

		if (mceconfig_card_cardtype(context, c, &ct)) return -3;

		for (i=0; i < ct.paramset_count; i++) {
			if (mceconfig_cardtype_paramset(context, &ct, i, &ps))
				return -4;

			if (index < ps.param_count) {
				return mceconfig_paramset_param(
					context, &ps, index, p);
			}
			index -= ps.param_count;
		}
		return -1;

	case MCE_NATURE_VIRTUAL:

		if (mceconfig_card_mapping(context, c, &m)) return -3;

		return mceconfig_mapping_param(context, &m, index, p);

	default:
		fprintf(stderr, "Unhandled card nature!\n");
		return -1;
	}

}


int mceconfig_card_paramcount(const mce_context_t *context,
			      const card_t *c)
{
	int i;
	C_config_check;
	cardtype_t ct;
	paramset_t ps;
	mapping_t m;
	int count = 0;

	switch(c->nature) {

	case MCE_NATURE_PHYSICAL:

		if (mceconfig_card_cardtype(context, c, &ct)) return -3;
		for (i=0; i < ct.paramset_count; i++) {
			if (mceconfig_cardtype_paramset(context, &ct, i, &ps))
				return -4;
			count += ps.param_count;
		}
		break;

	case MCE_NATURE_VIRTUAL:

		if (mceconfig_card_mapping(context, c, &m)) return -3;
		count += m.param_count;
		break;

	default:
		fprintf(stderr, "Unhandled card nature!\n");
		return -1;
	}
	return count;
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
	c->card_count = 0;
	c->flags = 0;
	int status = 1;

	// Update from key, maybe
	if (cfg == NULL) return -1;

	get_string(c->name, cfg, "name");

	int nature = MCE_NATURE_PHYSICAL;
	char nature_s[MCE_SHORT];
	if ((get_string(nature_s, cfg, "nature") == 0) &&
	    (st_get_id(card_natures_st, nature_s, &nature) != 0)) {
		fprintf(stderr, "card '%s' has invalid 'nature'!\n",
			c->name);
		return -1;
	}

	c->nature = nature;
	switch(c->nature) {
	case MCE_NATURE_PHYSICAL:
		get_string(c->type, cfg, "card_type");
		get_int((int*)&c->error_bits, cfg, "error_bits");

		c->card_count =	get_int_array(c->id, cfg, "id", MCE_MAX_CARDSET);
		if (c->card_count < 0) {
			fprintf(stderr, "card '%s' has invalid 'id' entry.\n",
				c->name);
			return -1;
		}
		break;

	case MCE_NATURE_VIRTUAL:
		get_string(c->type, cfg, "mapping");
		c->card_count = 1;
		break;

	default:
		fprintf(stderr, "Unhandled card nature '%s'!\n", nature_s);
		return -1;
	}

	get_int(&status, cfg, "status");
    c->flags |=
		( status ? MCE_PARAM_STAT : 0 );

	return 0;
}

int mceconfig_cfg_mapping(const config_setting_t *cfg, mapping_t *m)
{
	m->cfg = cfg;
	m->name[0] = 0;
	get_string(m->name, cfg, "name");
	m->param_count = count_elem(cfg, "parameters");

	return 0;
}

int mceconfig_cfg_maprange(const config_setting_t *cfg, maprange_t *mr)
{
	return ( get_int(&mr->start, cfg, "start") ||
		 get_int(&mr->count, cfg, "count") ||
		 get_int(&mr->offset, cfg, "offset") ||
		 get_string(mr->card_name, cfg, "card") ||
		 get_string(mr->param_name, cfg, "param") ) ?
		-1 : 0;
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

	char type_str[MCE_SHORT];

	// Fill with defaults
	p->cfg = cfg;
	p->id = -1;
	p->type = MCE_CMD_MEM;
	p->count = 1;
	p->flags = 0;
	p->name[0] = 0;

	int status = 1;
	int sign   = 0;
	int hex    = 0;
	int wr_only= 0;
	int rd_only= 0;

	get_string(p->name, cfg, "name");
	get_int(&p->id, cfg, "id");
	get_int(&p->count, cfg, "count");

	// Discover maximum and minimum
	if (get_int((int*)&p->min, cfg, "min")==0)
		p->flags |= MCE_PARAM_MIN;
	if (get_int((int*)&p->max, cfg, "max")==0)
		p->flags |= MCE_PARAM_MAX;

	// Discover defaults
	p->defaults = config_setting_get_member(cfg, "defaults");
	if (p->defaults != NULL)
		p->flags |= MCE_PARAM_DEF;

	// Maybe this is a mapped parameter...
	p->maps = config_setting_get_member(cfg, "maps");
	if (p->maps != NULL) {
        p->flags |= MCE_PARAM_MAPPED;
		p->map_count = config_setting_length(p->maps);
	}

	// Discover parameter type
	if ((get_string(type_str, cfg, "type") == 0) &&
	    (st_get_id(param_types_st, type_str, &p->type) != 0)) {
		fprintf(stderr,
			"parameter '%s' has unrecognized type specifier '%s'\n",
			p->name, type_str);
	}

	// Data manipulation instructions?
	if (get_int(&p->op_xor, cfg, "op_xor")==0)
		p->flags |= MCE_PARAM_MANIP;

	// Set additional flags?
	get_int(&status , cfg, "status");
	get_int(&sign   , cfg, "signed");
	get_int(&hex    , cfg, "hex");
	get_int(&wr_only, cfg, "write_only");
	get_int(&rd_only, cfg, "read_only");

    p->flags |=
		(status  ? MCE_PARAM_STAT   : 0) |
		(sign    ? MCE_PARAM_SIGNED : 0) |
		(hex     ? MCE_PARAM_HEX    : 0) |
		(wr_only ? MCE_PARAM_WONLY  : 0) |
		(rd_only ? MCE_PARAM_RONLY  : 0);

	return 0;
}


/*

  mceconfig_lookup_<child>(const mce_context_t* context, [const <parent>_t *p, ]
                           const char *child_name, <child>_t *c)

  - Lookup a config setting by name
  - in the case of param, the parent param_set must also be passed

*/

int  mceconfig_lookup_card(const mce_context_t* context, const char *card_name,
			   card_t *card)
{
	return mceconfig_cfg_card(
		get_setting_by_name(C_config.components, card_name),
		card );
}

int  mceconfig_lookup_cardtype(const mce_context_t* context, const char *cardtype_name,
			       cardtype_t *cardtype)
{
	return mceconfig_cfg_cardtype(
		get_setting_by_name(C_config.card_types, cardtype_name),
		cardtype );
}

int  mceconfig_lookup_paramset(const mce_context_t* context, const char *paramset_name,
			       paramset_t *paramset)
{
	return mceconfig_cfg_paramset(
		get_setting_by_name(C_config.parameter_sets, paramset_name),
		paramset );
}

int  mceconfig_lookup_param(const mce_context_t* context, const paramset_t *paramset,
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

int mceconfig_lookup(const mce_context_t* context,
		     const char *card_name, const char *para_name,
		     card_t *c, param_t *p)
{
	cardtype_t ct;
	paramset_t ps;
	mapping_t m;
	int index;

	C_config_check;

	if ((p==NULL) || (c==NULL)) return -1;

	p->flags = 0;

	if (mceconfig_lookup_card(context, card_name, c)) return -2;

	switch(c->nature) {

	case MCE_NATURE_PHYSICAL:

		if (mceconfig_card_cardtype(context, c, &ct)) return -3;

		for (index=0; index < ct.paramset_count; index++) {
			if (mceconfig_cardtype_paramset(context, &ct, index, &ps))
				return -4;

			if (mceconfig_lookup_param(context, &ps, para_name, p)==0)
				return 0;
		}
		break;

	case MCE_NATURE_VIRTUAL:

		if (mceconfig_card_mapping(context, c, &m)) return -3;

		config_setting_t *pcfg = get_setting(m.cfg, "parameters");
		if (pcfg == NULL) return -4;

		config_setting_t *cfg = get_setting_by_name(pcfg, para_name);
		if (cfg == NULL) return -5;

		if (mceconfig_cfg_param(cfg, p) != 0)
			return -6;

		return 0;

	default:
		fprintf(stderr, "Unhandled card nature!\n");
		return -3;
	}

	return -10;
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

/* Unused
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
*/

int get_int_array(int *dest, const config_setting_t *parent,
		  const char *name, int max)
{
	int i, count = 0;
	config_setting_t *set = get_setting(parent, name);
	if (set==NULL) return -1;

	count = config_setting_length(set);
	if (count < 0 || count >= max) {
		return -1;
	}

	for (i=0; i<count; i++) {
		dest[i] = config_setting_get_int_elem(set, i);
	}
	return count;
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
	config_setting_t *list = config_setting_get_member(parent, name);
	if (list==NULL) return 0;
	return config_setting_length(list);
}

/* String table functions */

int st_index(const string_table_t *st, const char *name)
{
	int i = 0;
	while (st[i].name != NULL)
		if (strcmp(name, st[i++].name)==0)
			return i-1;
	return -1;
}

int st_get_id(const string_table_t *st, const char *name, int *id)
{
	int index = st_index(st, name);
	if (index < 0) return -1;

	*id = st[index].id;
	return 0;
}
