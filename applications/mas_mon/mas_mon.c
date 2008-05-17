#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <mce_library.h>

#define MAX_BARLEN 400

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


int main()
{
	int head, tail, count;
	int last_usage = -1;
	mce_context_t *mce;
	int barlen = 40;

	if ((mce=mcelib_create())==NULL ||
	    (mcedata_open(mce, DEFAULT_DATAFILE)!=0)) {
		fprintf(stderr, "Failed to connect data device.\n");
		exit(1);
	}

	while (1) {
		mcedata_buffer_query(mce, &head, &tail, &count);
		int usage = (head - tail + count) % count;
		if (usage != last_usage)
			printf("[%4i/%4i] [%s]\n", usage, count, bar(barlen, barlen*usage/count));
		last_usage = usage;
		sleep(1);
	}

}
