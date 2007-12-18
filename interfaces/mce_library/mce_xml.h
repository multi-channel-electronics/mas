#ifndef _MCE_XML_H_
#define _MCE_XML_H_

#include <mce.h>
#include <stdio.h>

#define NAME_LENGTH   32
#define XML_PATHLEN   1024


typedef struct para_type
{
	struct para_type *next;
	char name[NAME_LENGTH];
	int count;
	int type;
	int n_ids;
	int n_defaults;
	//Following this, store
	//   int ids[n_ids]
	//   int def[n_defaults]
	int data_start;
  
} para_t;

typedef struct card_type
{
	struct card_type *next;
	para_t *para_head;
	char name[NAME_LENGTH];
	int id;
	int data_start;

} card_t;


typedef struct mce_data_type
{
	card_t *card_head;
	char filename[XML_PATHLEN];
	int data_start;

} mce_data_t;


/* Prototypes */

int     mcexml_load(char *filename, mce_data_t **mce, int _memsize);

void    mcexml_print(FILE *out, mce_data_t* mce);

char*   mcexml_errstr(void);

para_t* mcexml_lookup_para(card_t *card, char *para_str,
			   int *para_id, int index);

card_t* mcexml_lookup_card(mce_data_t *mce, char *card_str, int *card_id);

int     mcexml_lookup(mce_data_t *mce,
		      int *card_id, int *para_id, int *para_type, int *para_count,
		      char *card_str, char *para_str, int para_index);

#endif
