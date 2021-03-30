/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
/* main.c - entry point and initialization for mce logger */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <mce/socks.h>
#include "logger.h"
#include "init.h"

#define APP "maslog_server"
#define PIDFILE "/var/run/mas.pid"

int kill_switch = 0;

void die(int sig)
{
    kill_switch = 1;
}

int daemonize()
{
    int err = 0;
    if ( (err=daemon(0,0)) != 0 ) {
        return -1;
    }
    FILE *f = fopen(PIDFILE, "w");
    if (f==NULL)
        return 1;
    fprintf(f, "%i", getpid());
    fclose(f);
    return 0;
}

int undaemonize()
{
    return remove(PIDFILE);
}

int main(int argc, char **argv)
{
    params_t p;
    log_client_t me;
    strcpy(me.name, APP);

    printf(APP " - logging server for MAS\n");

    if (init(&p, argc, argv) != 0)
        return 1;

    if (p.daemon) {
        printf("Launching daemon...\n");
        if (daemonize() < 0) {
            printf("Could not spawn daemon: %i\n", errno);
            exit(1);
        }
    }

    // Install kill signal handler
    signal(SIGTERM, die);
    signal(SIGINT, die);

    // Talk
    log_text(&p, APP, "Starting.");

    // Main message loop

    listener_t *list = &p.listener;

    while (! (p.flags & FLAG_QUIT) && !kill_switch) {

        massock_listen_flags ret = massock_listener_select(list);

        //We only care about data
        if ( (ret & LISTENER_ERR) ) {
            fprintf(stderr, "Listener error!\n");
            continue;
        }

        if (! (ret & (LISTENER_CONNECT | LISTENER_DATA)) )
            continue;

        int i;
        for (i=0; i<MAX_CLIENTS; i++) {

            massock_listen_flags flags = list->clients[i].flags;

            if (flags & LISTENER_CONNECT) {
                printf("Client[%i]: connected.\n", i);
                list->clients[i].flags &= ~LISTENER_CONNECT;
            }

            if (flags & LISTENER_ERR) {
                printf("Client[%i]: error! Closing.\n", i);
                massock_client_delete( list->clients + i );
            }
            else if (flags & LISTENER_DATA) {

                log_client_process(&p, i);

            }
            if (flags & LISTENER_CLOSE) {
                printf("Client[%i]: closed.\n", i);
                massock_client_delete( list->clients + i );
            }
        }
    }

    log_text(&p, APP, "Stopping.");

    log_closefile(&p);

    massock_listener_close(list);
    massock_listener_cleanup(list);

    if (p.daemon)
        undaemonize();

    return 0;
}
