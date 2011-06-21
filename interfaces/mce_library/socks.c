#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mce/socks.h"

//Address addr should be in host:port form

static int fill_sockaddr(struct sockaddr_in *sa, const char *addr)
{
	char *hostname;
	char addr_copy[1024];

	if (addr == NULL) {
    fprintf(stderr, "No address specified\n");
    return -1;
	}

	hostname = strtok(addr_copy, ":");
	if (hostname==NULL) {
		fprintf(stderr, "No host specified!\n");
		return -1;
	}

	struct hostent *host = gethostbyname(hostname);
	if (host==NULL) {
		fprintf(stderr, "Could not get host '%s'; error %i\n", hostname, h_errno);
		return -1;
	}
	
	memcpy(&sa->sin_addr.s_addr, host->h_addr, host->h_length);
	sa->sin_family = AF_INET;

	char *portname = strtok(NULL, ":");
	if (portname == NULL) {
    fprintf(stderr, "No port specified\n");
    return -1;
  }

  char *ender = portname;		
  int port = strtol(portname, &ender, 0);
  if (ender[0]!=0) {
    fprintf(stderr, "Expected integer port '%s'\n", portname);
    return -1;
  }
  sa->sin_port   = htons(port); 

  return 0;
}

int massock_connect(const char *addr)
{
  struct sockaddr_in sa;

  if (fill_sockaddr(&sa, addr)) return -1;

  int sock = socket(AF_INET, SOCK_STREAM, 0);

  int err =  connect(sock, (struct sockaddr*)&sa, sizeof(sa)); 

  if (err!=0) {
    fprintf(stderr, "%s: connect failed\n", __func__);
    return err;
  }

  return sock;
}


int massock_listen(const char *addr)
{
  struct sockaddr_in sa;

  if (fill_sockaddr(&sa, addr)) return -1;

  int sock = socket(AF_INET, SOCK_STREAM, 0);

  //Bind and listen
  int yes = 1;
  if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))\
      == -1) {
    fprintf(stderr, "%s: Could not set socket options.\n", __func__);
    return -1;
  }

  int bind_err = bind(sock, (struct sockaddr *)&sa,
      sizeof(struct sockaddr));

  if (bind_err!=0) {
    fprintf(stderr, "%s: failed to bind: %s\n", __func__, strerror(errno));
    return -2;
  }

  int list_err = listen(sock, 5); // 5 is backlog size

  if (list_err!=0) {
    fprintf(stderr, "%s: failed to listen: %s\n", __func__, strerror(errno));
    return -3;
  }

  return sock;
}


/* Client management functionality */

int massock_listener_init( listener_t *list, int clients_max,
    int recv_max, int send_max)
{
  if (list==NULL) return -1;

  memset((void*)list, 0, sizeof(*list));

  int mem = clients_max * ( sizeof(massock_client_t) + recv_max + send_max);

  list->clients = (massock_client_t*) malloc(mem);
  if (list->clients==NULL) {
    list->clients_max = 0;
    return -1;
  }
  list->clients_max = clients_max;
  memset((void*)list->clients, 0, mem);


  char *startr = (char*) (list->clients + clients_max);
  char *starts = startr + recv_max;
  int delta = recv_max + send_max;

  int i;
  for (i=0; i<clients_max; i++) {
    massock_client_t *c = list->clients + i;
    c->recv_buf = startr + i*delta;
    c->send_buf = starts + i*delta;
    c->recv_max = recv_max;
    c->send_max = send_max;
  }


  return 0;
}

int massock_listener_cleanup( listener_t *list) {
  if (list==NULL) return -1;
  if (list->clients != NULL)
    free (list->clients);

  list->clients_max = 0;
  list->clients_count = 0;
  return 0;
}


int massock_listener_listen( listener_t *list, const char *addr )
{
  int sock = massock_listen(addr);
  if (sock<0) {
    fprintf(stderr, "%s: could not listen on address '%s'\n",
        __func__, addr);
    return sock;
  }
  list->sock = sock;
  strcpy(list->address, addr);

  FD_ZERO(&list->rfd_master);
  FD_SET(list->sock, &list->rfd_master);

  return 0;
}

int massock_listener_close( listener_t *list )
{
  if (list->sock > 0) {
    shutdown(list->sock, SHUT_RDWR);
  }

  return 0;
}


#define SUBNAME "listener_select: "

static int accept_now( int sock )
{
  struct sockaddr_in client;
  socklen_t sin_size = sizeof(struct sockaddr);

  int fd = accept(sock, (struct sockaddr*)&client, &sin_size);
  if (fd<0)
    fprintf(stderr, SUBNAME
        "could not accept connection [%i]\n", errno);
  return fd;
}

massock_listen_flags massock_listener_select( listener_t *list )
{
  struct timeval tv = {
    .tv_sec  = 1,
    .tv_usec = 0,
  };

  fd_set rfd = list->rfd_master;

  int n = select(FD_SETSIZE, &rfd, NULL, NULL, &tv);

  if (n < 0) {
    fprintf(stderr, SUBNAME "select error [%i]\n", errno);
    return -1;
  }

  if (n==0) return 0;

  // Check for new connections
  massock_listen_flags ret_flags = 0;

  if (FD_ISSET(list->sock, &rfd)) {
    int fd;
    massock_client_t *client;

    if ( (fd = accept_now(list->sock)) <= 0)
      return LISTENER_ERR;

    if ( (client = massock_client_add(list, fd)) == NULL) {
      close(fd);
      return LISTENER_ERR;
    }

    // New client accepted, add to polling list
    FD_SET(client->fd, &list->rfd_master);

    ret_flags |= LISTENER_CONNECT;
  }

  int i;
  for (i=0; i<list->clients_max; i++) {
    massock_client_t *cl = &list->clients[i];
    if (FD_ISSET(cl->fd, &rfd)) {
      cl->flags = massock_client_recv(cl);
      ret_flags |= LISTENER_DATA;
    } else
      cl->flags &= ~LISTENER_DATA;
  }

  return ret_flags;
}


massock_client_t* massock_client_add( listener_t *list, int fd )
{
  if (list==NULL) return NULL;
  if (fd<=0) return NULL;

  int i;
  for (i=0; i<list->clients_max; i++) {
    massock_client_t *cl = &list->clients[i];
    if (cl->fd <=0) {
      cl->fd = fd;
      cl->owner = list;
      cl->flags |= LISTENER_CONNECT;
      return cl;
    }
  }
  return NULL;
}

int massock_client_delete( massock_client_t *client )
{
  listener_t *list = client->owner;

  if (list==NULL || client->fd <= 0)
    return -1;

  //Remove client's fd from poll list, close
  FD_CLR(client->fd, &list->rfd_master);
  close(client->fd);

  client->flags = 0;
  client->fd = 0;

  return 0;
}


massock_listen_flags massock_client_send( massock_client_t *client, char *buf, int count )
{
  if (client==NULL || client->recv_buf==NULL)
    return LISTENER_ERR;

  massock_listen_flags retf = 0;

  /* 	if (count > client->send_max - client->send_idx) */
  /* 		return LISTENER_ERR; */

  /* 	memcpy(client->send_buf + client_send_idx, buf, count); */

  /* 	int count = client->recv_max - client->recv_idx; */

  ssize_t err = send(client->fd,
      buf,
      count, 0);

  if (err<0) {
    if (errno != EAGAIN) {
      fprintf(stderr, "send: errno=%i\n", errno);
      retf |= LISTENER_ERR;
    }
  } else if (err==0) {
    retf |= LISTENER_CLOSE;
    count = err;
  } else {
    retf |= LISTENER_DATA;
    count = err;
  }

  return retf;	
}


massock_listen_flags massock_client_recv( massock_client_t *client )
{
  if (client==NULL || client->recv_buf==NULL)
    return LISTENER_ERR;

  massock_listen_flags retf = 0;

  int count = client->recv_max - client->recv_idx;

  ssize_t err = recv(client->fd,
      client->recv_buf + client->recv_idx,
      count, 0);

  if (err<0) {
    if (errno != EAGAIN) {
      fprintf(stderr, "recv: errno=%i\n", errno);
      retf |= LISTENER_ERR;
    }
  } else if (err==0) {
    retf |= LISTENER_CLOSE;
    count = err;
  } else {
    retf |= LISTENER_DATA;
    count = err;
  }

  client->recv_idx += count;

  return retf;	
}
