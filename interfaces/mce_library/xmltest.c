#include <stdio.h>
#include <stdlib.h>

#include "mce_xml.h"

mce_data_t *mce_data;


int main()
{
	int err;

	err = mcexml_load("../mcexml/mce.xml",  &mce_data, 0);
	if (err!=0) {
		printf("Error %i: %s\n", err, mcexml_errstr());
		return 1;
	}

	mcexml_print(stdout, mce_data);

	char card_str[256] = "";
	char para_str[256] = "";
	int  index = 0;

	printf("Enter card and param: ");
	scanf("%s %s", card_str, para_str);

	//If present, remove param index from end of param_str
        char *s = para_str;
	while (*s!=0 && ( *s<'0' || *s>'9')) s++;
	sscanf(s, "%i", &index);
	*s = 0;
	  

	int card_id, para_id, para_type, para_count;
	err = mcexml_lookup(mce_data,
			    &card_id, &para_id, &para_type, &para_count,
			    card_str, para_str, index);
	printf("lookup returned err=%i, card=%#x, para=%#x [t=%i,n=%i]\n",
	       err, card_id, para_id, para_type, para_count);

	free(mce_data);

	return 0;
}
