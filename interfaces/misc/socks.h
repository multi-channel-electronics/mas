#ifndef _MCESOCKS_H_
#define _MCESOCKS_H_

#include <netinet/in.h>

#define SOCKS_STR 1024

typedef unsigned int listener_flags;

#define	LISTENER_ERR       0x8000
#define	LISTENER_OK        0x0001
#define	LISTENER_CONNECT   0x0002
#define	LISTENER_DATA      0x0004
#define	LISTENER_CLOSE     0x0008

struct listener_struct;

typedef struct {

	int  fd;
	
	char *recv_buf;
	int  recv_idx;
	int  recv_max;

	char *send_buf;
	int  send_idx;
	int  send_max;

	listener_flags flags;

	int  new_connection;

	struct listener_struct *owner;

} client_t;


typedef struct listener_struct {

	char address[SOCKS_STR];
	int  sock;

	int  quit;

	client_t *clients;
	int clients_count;
	int clients_max;

	fd_set rfd_master;

} listener_t;

//FIX?
extern char default_addr[SOCKS_STR];
extern int  default_port;

int fill_sockaddr(struct sockaddr_in *sa, const char *addr);
int connect_to_addr(const char *addr);
int listen_on_addr(const char *addr);

/* Client management functionality */

int listener_init( listener_t *list, int clients_max,
		   int recv_max, int send_max);
int listener_cleanup( listener_t *list);

int listener_close( listener_t *list );

int listener_listen( listener_t *list, const char *addr );
listener_flags listener_select( listener_t *list );

client_t *client_add( listener_t *list, int fd );
int client_delete( client_t *client );

listener_flags client_recv( client_t *client );
listener_flags client_send( client_t *client, char *buf, int count );

#endif
