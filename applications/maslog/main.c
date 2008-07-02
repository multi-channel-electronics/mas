/* main.c - entry point and initialization for mce logger */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <mce/socks.h>
#include "logger.h"
#include "init.h"

#define APP "maslog_server"


int main(int argc, char **argv)
{
	params_t p;
	
	printf(APP " - logging server for MAS\n");

	if (init(&p, argc, argv) != 0)
		return 1;

	if (p.daemon) {
		int err = 0;
		printf("Launching daemon...\n");
		if ( (err=daemon(0,0)) != 0 ) {
			printf("Could not spawn daemon: %i\n", errno);
			exit(1);
		}
	}


	// Main message loop
	
	listener_t *list = &p.listener;

	while (! (p.flags & FLAG_QUIT)) {

		listener_flags ret = listener_select(list);

		//We only care about data
		if ( (ret & LISTENER_ERR) ) {
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

				log_client_process(&p, i);

			}
			if (flags & LISTENER_CLOSE) {
				printf("Client[%i]: closed.\n", i);
				client_delete( list->clients + i );
			}			       
		}		
	}

	log_closefile(&p);

	listener_close(list);
	listener_cleanup(list);

	return 0;

}

