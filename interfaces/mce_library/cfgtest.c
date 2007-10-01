#include <stdio.h>
#include <stdlib.h>
#include "mceconfig.h"

void pr_int_or_blank(char *result, int value, int flag, int width) {
	char format[8];
	sprintf(format, "%%%i%s ", width, flag ? "i" : "s");
	if (flag)
		sprintf(result, format, value);
	else
		sprintf(result, format, "");
}


int main()
{
	// Load MCE.CFG file

	mce_data_t *mce;
	char *filename = "../mce.cfg";
	if (mceconfig_load(filename, &mce)) {
		printf("Could not load %s\n", filename);
		return -1;
	}

	// Base listing
	printf("BASE-LISTING\n");

	printf(" Cards:\n");

	int c_idx;
	card_t card;
	for (c_idx=0; mceconfig_card(mce, c_idx, &card)==0; c_idx++) {
		printf("  %2i %-5s\n", c_idx, card.name);
	}

	printf("\n Card types:\n");

	int cs_idx;
	cardtype_t cardtype;
	for (cs_idx=0; mceconfig_cardtype(mce, cs_idx, &cardtype)==0; cs_idx++) {
		printf("  %2i %-20s %2i\n", cs_idx, cardtype.name, cardtype.paramset_count);
	}

	printf("\n Parameter sets:\n");
	int ps_idx;
	paramset_t ps;
	for (ps_idx=0; mceconfig_paramset(mce, ps_idx, &ps)==0; ps_idx++) {
		printf("  %2i %-30s %4i\n", ps_idx, ps.name, ps.param_count);
	}
	return 0;



	// Tree scanning
	printf("TREE-TRAVERSAL\n");

	for (c_idx=0; mceconfig_card(mce, c_idx, &card)==0; c_idx++) {

		cardtype_t cardtype;
		if (mceconfig_card_cardtype(mce, &card, &cardtype)) {
			printf("No card type!\n");
			continue;
		}

		printf("%2i %-5s %-20s\n", c_idx, card.name, cardtype.name);

		int ps_idx;
		paramset_t ps;
		for (ps_idx=0; ps_idx<cardtype.paramset_count; ps_idx++) {
			if (mceconfig_cardtype_paramset(mce, &cardtype,
							ps_idx, &ps) != 0) {
				printf("No param set!\n");
				continue;
			}
			printf("%10s %-20s [%2i]\n", "", ps.name,
			       ps.param_count);

			int p_idx;
			param_t p;
			for (p_idx=0; p_idx<ps.param_count; p_idx++) {
				if (mceconfig_paramset_param(mce, &ps, p_idx,
							     &p)!=0) {
					printf("No param!\n");
					continue;
				}
				printf("%21s %15s %2x ", "", p.name, p.id);

				char max[16];
				char min[16];
				pr_int_or_blank(max, p.max, p.flags & PARAM_MAX, 6);
				pr_int_or_blank(min, p.min, p.flags & PARAM_MIN, 6);

				printf("%2i %2i | %s %s %i\n",
				       p.id_count, p.count, max, min,
				       (p.flags & PARAM_DEF) ? 1 : 0 );

			}
		}

	}

	mceconfig_destroy(mce);
	return 0;
}
