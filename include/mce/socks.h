#ifndef _MCESOCKS_H_
#define _MCESOCKS_H_

#include <netinet/in.h>

#define SOCKS_STR 1024

typedef unsigned int massock_listen_flags;

/* massock error values */
#define MASSOCK_BAD_ADDR -1 /* invalid address specified */
#define MASSOCK_NSLOOKUP -2 /* host lookup failed */

/* in the case of the following errors, consulting errno might
 * be informative */
#define MASSOCK_SOCKET   -13 /* unable to create socket */
#define MASSOCK_CONNECT  -14 /* unable to connect to port */
#define MASSOCK_BIND     -15 /* unable to bind to port */
#define MASSOCK_LISTEN   -16 /* unable to listen on port */
#define MASSOCK_ACCEPT   -17 /* unable to accept inbound connection */
#define MASSOCK_SELECT   -18 /* select() error */

#define	LISTENER_ERR       0x8000
#define	LISTENER_OK        0x0001
#define	LISTENER_CONNECT   0x0002
#define	LISTENER_DATA      0x0004
#define	LISTENER_CLOSE     0x0008

struct massock_listener_t;

typedef struct {

	int  fd;

	char *recv_buf;
	int  recv_idx;
	int  recv_max;

	char *send_buf;
	int  send_idx;
	int  send_max;

	massock_listen_flags flags;

	int  new_connection;

	struct massock_listener_t *owner;

} massock_client_t;


typedef struct massock_listener_t {

	char address[SOCKS_STR];
	int  sock;

	int  quit;

	massock_client_t *clients;
	int clients_count;
	int clients_max;

	fd_set rfd_master;

} listener_t;

const char *massock_error(int err, int syserr);
int massock_connect(const char *addr, int port);
int massock_listen(const char *addr);

/* Client management functionality */

int massock_listener_init( listener_t *list, int clients_max,
		   int recv_max, int send_max);
int massock_listener_cleanup( listener_t *list);

int massock_listener_close( listener_t *list );

int massock_listener_listen( listener_t *list, const char *addr );
massock_listen_flags massock_listener_select( listener_t *list );

massock_client_t *massock_client_add( listener_t *list, int fd );
int massock_client_delete( massock_client_t *client );

massock_listen_flags massock_client_recv( massock_client_t *client );
massock_listen_flags massock_client_send( massock_client_t *client, char *buf,
    int count );

#endif
