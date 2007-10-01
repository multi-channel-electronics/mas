#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <expat.h>

#include "mcecmd.h"
#include "mce_xml.h"

#define MAX_PARA 2048 /*Maximum parameter occupation, in bytes*/

#define BUFFSIZE 8192 /*Size of basic fread buffer*/

#define DEFAULT_FILENAME "mce.xml"

#define DEFAULT_KEY "MCE"

#define DEFAULT_MEMSIZE 16000

#define debug_printf(A...) //printf(A)

/******************************************
  Error recording system
******************************************/

char mce_xml_error[1024] = "";

void put_error_string(char *src)
{
	int n = strlen(src);
	if (n>=1023) n = 1023;
	mce_xml_error[n] = 0;
	strncpy(mce_xml_error, src, n);
}


/******************************************
  XML parsing; will pack MCE configuration
   into card_head linked list.
******************************************/

int memsize = 0;
int current = 0;
int overflow = 0;

int depth = 0;   // Tracks depth inside tree
int inside = 0;  // Indicates we're currently processing tags

mce_data_t *this_mce = NULL; // Pointer to the top of our tree
card_t *this_card = NULL; // Pointer to active card
para_t *this_para = NULL; // Pointer to active parameter

int extract_ints(int *dest, const char *src) {
	int n = 0;
	int d = 0;
	while (1==sscanf(src, "%i%n", dest+n, &d)) {
		src+=d;
		n++;
	}
	return n;
}

int find_key(char *key, const char **attr) {
	int i;
	for (i=0; attr[i]; i+=2)
		if (strcmp(key, attr[i])==0) return i;
	return -1;
}

int str_to_type(const char *str) {
	if (0==strcmp(str, "int")) return MCE_PARAM_INT;
	if (0==strcmp(str, "cmd")) return MCE_PARAM_CMD;
	if (0==strcmp(str, "rst")) return MCE_PARAM_RST;
	return -1;
}

char *type_to_str(int typ) {
	switch (typ) {
	case MCE_PARAM_INT:
		return "int";
	case MCE_PARAM_CMD:
		return "cmd";
	case MCE_PARAM_RST:
		return "rst";
	}
	return "";
}

int str_to_int(const char *str) {
	return strtol(str, NULL, 0);
}

inline int* para_tail(para_t *para) {
	return (int*)para->next;
}

inline void para_tail_inc(para_t *para, int step) {
	para->next = (para_t*) (para_tail(para) + step);
}

int stow_ids(para_t *para, const char **attr) {
	int idx;
 
	idx = find_key("ParId", attr);
	if (idx<0) idx = find_key("ParIds", attr);
	if (idx<0) return -1;
	para->n_ids = extract_ints(para_tail(para), attr[idx+1]);
	para_tail_inc(para, para->n_ids);

	idx = find_key("Default", attr);
	if (idx<0) return -1;
	para->n_defaults = extract_ints(para_tail(para), attr[idx+1]);
	para_tail_inc(para, para->n_defaults);

	return 0;
}

#define GET_FUNC(KEY,FUNC,DEF) ((k=find_key(KEY,attr))>=0) ? \
                         FUNC(attr[k+1]) : DEF;

int init_para(para_t *para, const char *el, const char **attr)
{
	int k;
	strcpy(para->name, el);
	para->count = GET_FUNC("Count", atoi, 0);
	para->type = GET_FUNC("Type", str_to_type, -1);
	para->next = (para_t*)(&para->data_start);
	stow_ids(para, attr);
	return 0;
}

int init_card(card_t *card, const char *el, const char **attr)
{
	int k;
	strcpy(card->name, el);
	card->id = GET_FUNC("CardId", str_to_int, -1);
	card->next = (card_t*)&(card->data_start);
	card->para_head = NULL;
	return 0;
}

int check_mem() {
/*  	printf("CHECK: %i/%i\n", current, memsize);  */
	if (memsize - current < MAX_PARA)  return -1;
	return 0;
}



void start(void *data, const char *el, const char **attr)
{
	if (check_mem()!=0) overflow = 1;
	if (overflow) return;

	depth++;

/* 	if (depth==1) { */
/* 		printf("KEY='%s'\n", el); */
/* 	} */
	if (!inside) {
		if ( depth!=1 || strcmp(el, DEFAULT_KEY)!=0 )
			return;
		inside = 1;
	}

	switch(depth) {

	case 2:
    
		if (this_card==NULL) {
			this_card = (card_t*)&this_mce->data_start;
			this_mce->card_head = this_card;
		} else
			this_card = this_card->next;

		debug_printf("Card!   %i\n", (int)this_card-(int)this_mce);

		init_card(this_card, el, attr);

		break;

	case 3:

		if (this_para==NULL) {
			this_para = (para_t*)(this_card->next);
			this_card->para_head = this_para;
		}
		else this_para = this_para->next;

 		debug_printf("  Para! %i\n", (int)this_para-(int)this_card);

		init_para(this_para, el, attr);
		this_card->next = (card_t*)(this_para->next);

	}
}

void end(void *data, const char *el)
{
	if (overflow) return;

	if (!inside) return;
	switch(depth) {

	case 1:
		current = (int)( (char*)this_card->next - (char*)this_mce);
		this_card->next = NULL;
		this_card = NULL;
		break;

	case 2:
		if (this_para!=NULL) {
			current = (int)( (char*)this_para->next
					 - (char*)this_mce);
			this_para->next = NULL;
			this_para = NULL;
		}
		break;
	case 3:
		current = (int)( (char*)this_para->next - (char*)this_mce);
	}

	depth--;

	if (depth==0) inside=0;
}

void print_para(FILE *out, para_t* para)
{
	int i;
	fprintf(out, "\tPARA '%s', type=%i, count=%i\n",
		para->name, para->type, para->count);

	fprintf(out, "\t\t id=");
	for (i=0; i<para->n_ids; i++)
		fprintf(out, " %#x", *( &para->data_start + i));
	fprintf(out, "\t default=");
	for (i=para->n_ids; i<para->n_ids+para->n_defaults; i++)
		fprintf(out, " %#x", *( &para->data_start + i));
	fprintf(out, "\n");

}


void print_card(FILE *out, card_t* card)
{
	fprintf(out,"CARD '%s', id=%#x\n",
		card->name, card->id);

	para_t *q = card->para_head;
	while (q!=NULL) {
		print_para(out, q);
		q = q->next;
	}
}


/*******************************
  Exposed routines
*******************************/

void mcexml_print(FILE *out, mce_data_t* mce)
{
	card_t *c = mce->card_head;
	while (c!=NULL) {
		print_card(out, c);
		c = c->next;
	}
}

int mcexml_load(char *filename, mce_data_t **mce, int _memsize) {
	int err = -1;
	FILE *inf = NULL;

	XML_Parser xmlp = XML_ParserCreate(NULL);
	if (!xmlp) {
		put_error_string("XML_ParserCreate failure (no mem?).");
		goto close_out;
	}
	XML_SetElementHandler(xmlp, start, end);

	char *l_file = (filename==NULL || strlen(filename)==0) ?
		DEFAULT_FILENAME : filename;

	if ( (inf = fopen(l_file, "r")) == NULL) {
		put_error_string("Could not open xml file.");
		goto close_out;
	}

	memsize = (_memsize > 0) ? _memsize : DEFAULT_MEMSIZE;
	this_mce = (mce_data_t*) malloc(memsize);
	if (this_mce==NULL) {
		put_error_string("Could not allocate mce_data_t memory.");
		goto close_out;
	}
	this_card = NULL;
        this_para = NULL;

	strcpy(this_mce->filename, l_file);
	this_mce->card_head = (card_t*)(this_mce + 1);

	int done = 0;
	while (!done) {
		char buf[BUFFSIZE];
		int len = fread(buf, 1, BUFFSIZE, inf);
		if (ferror(inf)) {
			put_error_string("File read error.");
			goto close_out;
		}
		done = feof(inf);

		if (! XML_Parse(xmlp, buf, len, done)) {
			char err[256];
			sprintf(err, "XML parse error at line %d: %s",
				XML_GetCurrentLineNumber(xmlp),
				XML_ErrorString(XML_GetErrorCode(xmlp)));
			put_error_string(err);
			goto close_out;
		}
		if (overflow) {
			put_error_string("Base memory allocation too small\n");
/* 			printf(" overflow\n\n"); */
			goto close_out;
		}
	}
	err = 0;

 close_out:

	if (inf!=NULL) fclose(inf);

	if (err==0) {
		*mce = this_mce;
	} else {
		*mce = NULL;
		if (this_mce != NULL) free(this_mce);
	}

	return err;
}


char* mcexml_errstr(void)
{
	return (char*)mce_xml_error;
}


para_t* mcexml_lookup_para(card_t *card, char *para_str,
			   int *para_id, int index)
{
	para_t *para = card->para_head;
	
	for (; para != NULL; para = para->next) {
		if (strcmp(para_str, para->name)==0) {
			if (index >=0 && index < para->n_ids) {
				*para_id = *(&para->data_start + index);
				return para;
			} else return NULL;
		}
	}
	return NULL;
}


card_t* mcexml_lookup_card(mce_data_t *mce, char *card_str, int *card_id)
{
	card_t *card = mce->card_head;

	for (; card != NULL; card = card->next) {
		if (card_str==NULL && card->id == *card_id)
			return card;
		else if (strcmp(card_str, card->name)==0) {
			*card_id = card->id;
			return card;
		}
	}		
	return NULL;
}

int mcexml_lookup(mce_data_t *mce,
		  int *card_id, int *para_id, int *para_type, int *para_count,
		  char *card_str, char *para_str, int para_index)
{
	char err[256];
	card_t *card = mcexml_lookup_card(mce, card_str, card_id);
	if (card==NULL) {
		sprintf(err, "Card '%s' not found", card_str);
		put_error_string(err);
		return -1;
	}
	
	para_t *para = mcexml_lookup_para(card, para_str,
					  para_id, para_index);
	if (para==NULL) {
		sprintf(err, "Para '%s' not found", para_str);

		// Try the "any" listing
		int dummy;
		card_t *any = mcexml_lookup_card(mce, "any", &dummy);
		if (any==NULL) {
			put_error_string(err);
			return -1;
		}
		para = mcexml_lookup_para(any, para_str,
					  para_id, para_index);
		if (para==NULL) {
			put_error_string(err);
			return -1;
		}
	}
	*para_type  = para->type;
	*para_count = para->count;	

	return 0;
}
