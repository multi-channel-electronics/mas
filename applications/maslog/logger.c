#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>

#include "logger.h"

#define CLIENTBUF 2048

#define ESCAPE  27
#define LOG_ESC "LOG:"

struct string_pair {
	int  key;
	char name[16];
};

enum {LOG_QUIT=0,
      LOG_FLUSH,
      LOG_REOPEN,
      LOG_LEVEL,
      LOG_CLIENT_NAME,
      LOG_CLIENT_FLUSH
};

#define N_LOG_KEY 7

struct string_pair log_key_pairs[N_LOG_KEY]  = {
	{LOG_QUIT        , "quit"},
	{LOG_QUIT        , "exit"},
	{LOG_FLUSH       , "flush"},
	{LOG_REOPEN      , "reopen"},
	{LOG_LEVEL       , "level"},
	{LOG_CLIENT_NAME , "client_name"},
	{LOG_CLIENT_FLUSH, "client_flush"},
};

int log_command(params_t *p, int client_idx, char *buf);

#define CHECKPTR    if (p==NULL) return -1

void log_init(params_t *p)
{
	if (p==NULL) return;
	log_setflag(p, FLAG_FLUSH);
	p->daemon = 0;
	p->level  = 3;
}


int log_openfile(params_t *p)
{
	CHECKPTR;
	if (p->out!=NULL) return -1;

	p->out = fopen(p->filename, "a");
	if (p->out==NULL) return -1;

	return 0;
}

int log_reopen(params_t *p)
{
	CHECKPTR;
	if (p->out!=NULL) {
		FILE* f = p->out;
		p->out = NULL;
		fclose(f);
	}
	p->out = fopen(p->filename, "a");
	if (p->out==NULL) return -1;
	return 0;
}

int log_string_now(params_t *p, int client_idx, char *str)
{
	CHECKPTR;

	if (p->out==NULL)
		return -1;

	char time_str[256];
	time_t t = time(NULL);
	struct tm *tb = localtime(&t);

	strftime(time_str, 256, "%x %X", tb);

	fprintf(p->out, "%s %s : %s\n",
		time_str, p->clients[client_idx].name, str);

	if (p->flags & FLAG_FLUSH)
		fflush(p->out);

	return 0;
}

int log_string(params_t *p, int client_idx, char *str)
{
	CHECKPTR;

	int delta = strlen(LOG_ESC);
	if (str[1]==ESCAPE && strncmp(str+2, LOG_ESC, delta)==0)
		return log_command(p, client_idx, str+delta+2);

	int level = str[0] - '0';
	if (level >= p->level)
		return log_string_now(p, client_idx, str+1);

	return 0;
}

int log_flush(params_t *p) {
	CHECKPTR;
	if (p->out!=NULL)
		fflush(p->out);
	return 0;
}

int log_closefile(params_t *p) {
	CHECKPTR;

	if (p->out==NULL)
		return -1;
	fclose(p->out);
	p->out = NULL;

	return 0;
}

void log_setflag(params_t *p, int flag) {
	p->flags |= flag;
}

void log_clearflag(params_t *p, int flag) {
	p->flags &= ~flag;
}


/* Command handling */

int lookup_key(struct string_pair *pairs, int n_pairs, char *name) {
	int i=0;
	while (i<n_pairs) {
		if (strcmp(name, pairs[i++].name)==0)
			return pairs[--i].key;
	}
	return -1;
}

int get_int(int *dest, char *src) {
	if (src==NULL || *src==0) return -1;
	char *end = src;
	int tmp = strtol(src, &end, 0);
	if (*end!=0) return -1;
	*dest = tmp;
	return 0;
}

int log_quit(params_t *p)
{
	log_setflag(p, FLAG_QUIT);
	return 0;
}

int log_command(params_t *p, int client_idx, char *buf)
{
	CHECKPTR;

	log_client_t *client = p->clients + client_idx;

	int n;
	char *lasts;
	char *keyword = strtok_r(buf, " ", &lasts);
	char *nextword = strtok_r(NULL, " ", &lasts);

	if (keyword==NULL) keyword = "";
	char tmp[256];
	
	int key = lookup_key(log_key_pairs, N_LOG_KEY, keyword);

	switch(key) {

	case LOG_QUIT:
		printf("Received quit from client[%i]\n", client_idx);
		log_quit(p);
		break;

	case LOG_FLUSH:
		printf("Received flush from client[%i]\n", client_idx);
		log_flush(p);
		break;

	case LOG_REOPEN:
		printf("Received reopen from client[%i]\n", client_idx);
		log_reopen(p);
		break;

	case LOG_LEVEL:
		if (nextword!=NULL && get_int(&n, nextword)==0) {
			printf("Received level->%i from client[%i]\n",
			       n, client_idx);
			p->level = n;
		} else {
			printf("Invalid level-> command from client[%s]\n",
			       nextword);
		}
		break;

	case LOG_CLIENT_NAME:
		if (nextword==NULL) client->name[0] = 0;
		else strcpy(client->name, nextword);			

		printf("Client[%i]: name=%s\n", client_idx, client->name);
		break;

	case LOG_CLIENT_FLUSH:
		if (nextword!=NULL && get_int(&n, nextword)==0) {
			if (n) log_setflag(p, FLAG_FLUSH);
			else log_clearflag(p, FLAG_FLUSH);
		}
		break;


	default:
		sprintf(tmp, "Invalid log command '%s'", keyword);
		log_string_now(p, client_idx, tmp);
		return -1;
	}
	return 0;

}


int log_client_process(params_t *p, int client_idx)
{
	CHECKPTR;

	client_t *l_client = p->listener.clients + client_idx;

	//Check for complete commands;
	char *buf   = l_client->recv_buf;
	int  size   = l_client->recv_idx;

	char *start = l_client->recv_buf;
	char *stop  = start;
	while (stop < buf + size) {
		if (*stop=='\n') *stop = 0;
		if ((*stop==0)) {
			if (stop != start)
				log_string(p, client_idx, start);
			start = stop+1;
			stop = start;
			continue;
		}
		stop++;
	}
	
	//Move unprocessed data to the beginning of the buffer

	l_client->recv_idx = stop-start;
	if (stop-start == MAX_MSG) {
		fprintf(stderr, "Message buffer exceeded, flushing.\n");
		char temp = *(stop-1);
		*(stop-1) = 0;
		log_string(p, client_idx, start);
		*buf = temp;
		l_client->recv_idx = 1;
		return -1;
	}

	while (start < stop)
		*(buf++) = *(start++);

	return 0;
}
