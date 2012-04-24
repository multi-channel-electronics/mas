#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libmaslog.h"


#define LINE 1024

int main(int argc, char **argv) {

	maslog_t logger;

	int ret = 0;
	if (argc>1)
		ret = maslog_connect(&logger, argv[1], "notes");
	else 
		ret = maslog_connect(&logger, NULL, "notes");

	if (ret<0)
		exit(1);

	char *line = (char*)malloc(LINE);

	while (!feof(stdin) ) {
		size_t nin = LINE-1;
		int nout = getline(&line, &nin, stdin);
		if (nout>0) {
			if (line[nout]!=0) line[nout]=0;
			if (line[nout-1]=='\n')
				line[--nout]=0;
			maslog_print(&logger, line);
		}
	}

	maslog_close(&logger);

	return 0;
}
