/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mce_library.h"
#include "libmaslog.h"

#define LINE 1024

int main(int argc, char **argv) {
    mce_context_t mce;
    maslog_t maslog;

    mce = mcelib_create(0, (argc > 1) ? argv[1] : NULL);
    if (mce == NULL)
        exit(1);

    if ((maslog = maslog_connect(mce, "notes")) == NULL)
		exit(1);

	char *line = (char*)malloc(LINE);

	while (!feof(stdin) ) {
		size_t nin = LINE-1;
		int nout = getline(&line, &nin, stdin);
		if (nout>0) {
			if (line[nout]!=0) line[nout]=0;
			if (line[nout-1]=='\n')
				line[--nout]=0;
            maslog_print(maslog, line);
		}
	}

    maslog_close(maslog);

	return 0;
}
