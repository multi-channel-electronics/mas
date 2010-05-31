#include <linux/proc_fs.h>

#include "driver.h"

int read_proc(char *buf, char **start, off_t offset, int count, int *eof,
	      void *data)
{
	int i = 0, j;
	int len = 0;
	int limit = count - 80;
	char system[32];

	if (len < limit) {
		len += sprintf(buf+len,"\nmce_dsp driver version %s\n",
			       VERSION_STRING);
		len += sprintf(buf+len,"    fakemce:  "

#ifdef FAKEMCE
			       "yes\n"
#else
			       "no\n"
#endif
			);
		len += sprintf(buf+len,"    realtime: "
#ifdef REALTIME
			       "yes\n"
#else
			       "no\n"
#endif
			);
		len += sprintf(buf+len,"    bigphys:  "
#ifdef BIGPHYS
			       "yes\n"
#else
			       "no\n"
#endif
			);
	}

	for(i=0; i<MAX_CARDS; i++) {
		if (len < limit)
			len += sprintf(buf+len,"\nCARD: %d\n", i);
		for (j=0; j<3; j++) {
			proc_f p = NULL;
			switch (j) {
			case 0:
				p = mceds_proc[i].data;
				strcpy(system, "  data buffer:\n");
				break;
			case 1:
				p = mceds_proc[i].mce;
				strcpy(system, "  mce commander:\n");
				break;
			case 2:
				p = mceds_proc[i].dsp;
				strcpy(system, "  dsp commander:\n");
				break;
			}
			if (len >= limit || p == NULL) continue;
			len += sprintf(buf+len,system);
			len += p(buf+len, limit-len, i);
		}
		*eof = 1;
	}
	
	if (len < limit) {
	  len += sprintf(buf+len,"\n");
	}

	return len;
}

mceds_proc_t mceds_proc[MAX_CARDS];


