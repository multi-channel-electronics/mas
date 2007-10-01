/* main.c - entry point and initialization for mce data appliation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <libconfig.h>
#include <libmaslog.h>
#include <mcecmd.h>

#include <socks.h>
#include "masdata.h"
#include "init.h"
#include "data.h"

int main(int argc, char **argv)
{
	char msg[MEDLEN];

	params_t params;
	params_t *p = &params;

	if (init(p, argc, argv) != 0)
		return 1;

	data_get_directory(p, msg);
	printf("%s\n", msg);
	logger_print(&p->logger, msg);

	if (p->server.daemon) {
		int err = 0;
		printf("Launching daemon...\n");
		logger_print_level(&p->logger,
				   "Launching daemon\n", LOGGER_INFO);
		if ( (err=daemon(1,0))!=0 ) {
			printf("Could not spawn daemon: %i\n", errno);
			exit(1);
		}
	}


        //Main message loop

	listener_t *list = &p->server.listener;

	while (! (p->pflags & PFLAG_QUIT)) {

		int ret = listener_select(list);

		// It's all about the data
		if (data_checkflags(&p->data_data, FLAG_DONE)) {
			printf("Acquisition completed\n");
			logger_print(&p->logger, "Acquisition completed\n");
			data_reset(p);
		}

		if ( ret & LISTENER_ERR ) {
			fprintf(stderr, "Listener error!\n");
			continue;
		}

		if (! (ret & (LISTENER_CONNECT | LISTENER_DATA)) )
			continue;

		int i;
		for (i=0; i<MAX_CLIENTS; i++) {

			listener_flags flags = list->clients[i].flags;

			if (flags & LISTENER_CONNECT) {
				printf("Client[%i]: connected.\n", i);
				list->clients[i].flags &= ~LISTENER_CONNECT;
			}

			if (flags & LISTENER_ERR) {
				printf("Client[%i]: error! Closing.\n", i);
				client_delete( list->clients + i );
			}
			else if (flags & LISTENER_DATA) {

				data_client_process(p, i);

			}
			if (flags & LISTENER_CLOSE) {
				printf("Client[%i]: closed.\n", i);
				client_delete( list->clients + i );
			}			       

		}		
	
	}

	logger_print(&p->logger, "Shutting down.\n");

	data_closefile(&p->datafile, msg);

	listener_close(list);
	listener_cleanup(list);

	return 0;
}


int lookup_key(struct string_pair *pairs, int n_pairs, char *name) {
	int i=0;
	while (i<n_pairs) {
		if (strcmp(name, pairs[i++].name)==0)
			return pairs[--i].key;
	}
	return -1;
}

char* key_list(struct string_pair *pairs, int n_pairs, char *separator)
{
	static char dest[MEDLEN];
	char *s = dest;
	int i;
	for (i=0; i<n_pairs; i++) {
		s += sprintf(s, "%s%s",
			     pairs[i].name, separator);
	}
	return dest;
}

char* key_list_marker(struct string_pair *pairs, int n_pairs, char *separator,
		      int mark_index, char *marker)
{
	static char dest[MEDLEN];
	char *s = dest;
	int i;
	for (i=0; i<n_pairs; i++) {
		if (pairs[i].key==mark_index)
			s+= sprintf(s, "%s", marker);
		s += sprintf(s, "%s%s",
			     pairs[i].name, separator);
	}
	return dest;
}
