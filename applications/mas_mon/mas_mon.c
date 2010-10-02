#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "../../defaults/config.h"
#include <mce_library.h>

#define MAX_BARLEN 400

#define MON_LOG     0x01
#define MON_PRINT   0x02

char *bar(int len, int fill)
{
	static char b[MAX_BARLEN];
	int i;
	for (i=0; i<fill; i++)
		b[i] = '*';
	for (i=fill; i<len; i++)
		b[i] = ' ';
	b[len] = 0;
	return b;
}


int main(int argc, char **argv)
{
	int head, tail, count;
	int last_usage = -1;
	mce_context_t *mce;
	int barlen = 40;
	char ts[1024];
	time_t t;

#if MAX_FIBRE_CARD != 1
  if (argc > 1)
    sprintf(ts, "/dev/mce_data%i", atoi(argv[1]));
  else 
#endif
    sprintf(ts, "/dev/mce_data%i", DEFAULT_FIBRE_CARD);

	if ((mce=mcelib_create())==NULL ||
	    (mcedata_open(mce, ts)!=0)) {
		fprintf(stderr, "%s: failed to connect data device %s\n", argv[0], ts);
		exit(1);
	}

	while (1) {
		mcedata_buffer_query(mce, &head, &tail, &count);
		int usage = (head - tail + count) % count;
		if (usage != last_usage) {
			t = time(NULL);
			strcpy(ts, ctime(&t));
			ts[strlen(ts)-1] = 0;
			printf("%s [%4i/%4i] [%s]\n",
			       ts, usage, count, bar(barlen, barlen*usage/count));
			last_usage = usage;
		}
		usleep(100000);
	}

	return 0;
}
