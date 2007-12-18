#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <libconfig.h>

#include <mcecmd.h>
#include "server.h"
#include "masdata.h"


#define ESCAPE 27

enum {DATA_QUIT=0,
      DATA_SET,
      DATA_GET,
      DATA_FLUSH,
      DATA_OPENFILE,
      DATA_CLOSEFILE,
      DATA_OPENDIR,
      DATA_GO,
      DATA_STOP,
      DATA_FAKESTOP,
      DATA_BLOCK,
      DATA_REPORT,
};

enum {DATA_SET_DIR,
      DATA_SET_CARD,
      DATA_SET_SEQ,
      DATA_SET_FORMAT,
      DATA_SET_BASENAME,
      DATA_SET_FILEMODE,
      DATA_SET_FRAMEINT,
};

#define N_DATA_KEY 11

struct string_pair data_key_pairs[N_DATA_KEY]  = {
	{DATA_QUIT     , "kill"},
	{DATA_SET      , "set"},
	{DATA_FLUSH    , "flush"},
 	{DATA_BLOCK    , "block"}, 
	{DATA_OPENFILE , "openfile"},
	{DATA_CLOSEFILE, "closefile"},
	{DATA_OPENDIR  , "opendir"},
	{DATA_GO       , "go"},
	{DATA_STOP     , "stop"},
	{DATA_FAKESTOP , "fakestop"},
	{DATA_REPORT   , "report"},
};

#define N_DATA_SET 7

struct string_pair data_set_pairs[N_DATA_SET] = {
	{DATA_SET_DIR     , "directory"},
	{DATA_SET_SEQ     , "sequence"},
	{DATA_SET_CARD    , "card"},
	{DATA_SET_FORMAT  , "format"},
	{DATA_SET_FILEMODE, "filemode"},
	{DATA_SET_BASENAME, "basename"},
	{DATA_SET_FRAMEINT, "frameint"},
};


/* data_* interface manipulates params_t object */

#define CHECKPTR        if (p==NULL) return -1

void data_init(params_t *p)
{
	if (p==NULL) return;
	memset(p, 0, sizeof(*p));
}


/*
 *      Command response functions
 */

int quit(params_t *p)
{
	data_kill(p);
	p->pflags |= PFLAG_QUIT;
	return 0;
}


/* Handler for commands */

int get_int(int *dest, char *src) {
	if (src==NULL || *src==0) return -1;
	char *end = src;
	int tmp = strtol(src, &end, 0);
	if (*end!=0) return -1;
	*dest = tmp;
	return 0;
}

int store_tokens(int *argc, char **argv, char *src)
{
	char *lasts;
	int k;
	for (k=0; k<*argc; k++) {
		argv[k] = strtok_r(src, " ", &lasts);
		src = NULL;
		if (argv[k]==NULL) break;
	}
	*argc = k;
	return k;
}

char* collect_strings(char *dest,
		      struct string_pair *pairs, int n_pairs, char *separator)
{
	char *s = dest;
	int i;
	for (i=0; i<n_pairs; i++) {
		s += sprintf(s, "%s%s",
			     pairs[i].name, separator);
	}
	return dest;
}

int command_set(params_t *p, int client_idx,
		int argc, char **argv, char *errstr)
{
	char options[MEDLEN];
	
	// Args are SET PARAM [arg [arg [...] ] ]

	int key = -1;
	if (argc>=2)
		key = lookup_key(data_set_pairs, N_DATA_SET, argv[1]);

	int a, b;

	switch (key) {

/* 	case DATA_SETFILE: */
/* 		printf("Received setfile '%s' from client[%i]\n", */
/* 		       nextword, client_idx); */
/* 		if (data_setfilename(&p->datafile, errstr+1, nextword) !=0) */
/* 			errstr[0] = '0'; */
/* 		break; */

	case DATA_SET_DIR:
		if (argc < 3) {
			sprintf(errstr, "'%s %s' expects an argument",
				argv[0], argv[1]);
			return -1;
		}
		if (data_set_directory(p, errstr, argv[2])!=0) {
			sprintf(errstr, "Could not change working directory "
				"to '%s'", argv[2]);
			return -1;
		}
		break;

	case DATA_SET_SEQ:
		if (argc < 4 ||
		    get_int(&a, argv[2])!=0 ||
		    get_int(&b, argv[3])!=0) {
			sprintf(errstr, "'%s %s' expects 2 integer arguments",
				argv[0], argv[1]);
			return -1;
		}
		if (frame_set_sequence(&p->frame_setup, a, b)!=0) {
			sprintf(errstr, "failed to set sequence [%i, %i]",
				a, b);
			return -1;
		}
		break;

	case DATA_SET_FORMAT:		
		if (frame_set_format(&p->frame_setup, argv[2], errstr)!=0)
			return -1;
		break;

	case DATA_SET_CARD:
		if (frame_set_card(&p->frame_setup, argv[2], errstr)<0)
			return -1;
			
/* 			errstr += sprintf(errstr, "'%s %s' expects argument " */
/* 					  "from [ ", argv[0], argv[1]); */
/* 			int i=0; */
/* 			for (i=0; i<p->frame_setup.colcfg_count; i++) */
/* 				errstr += sprintf(errstr, "%s ", */
/*   				  p->frame_setup.colcfgs[i].code); */

/* 			errstr += sprintf(errstr, "]"); */
/* 			return -1; */
/* 		}		 */
		break;

	case DATA_SET_FILEMODE:
		// Don't check argument count; instead get nice error msg
		if (data_set_filemode(&p->datafile, argv[2], errstr)!=0)
			return -1;
		break;
		
	case DATA_SET_BASENAME:
		if (data_set_basename(&p->datafile, argv[2], errstr))
			return -1;
		break;

	case DATA_SET_FRAMEINT:
		a = 0;  // This will cause data_set_frameint to fail
		get_int(&a, argv[2]);
		if (data_set_frameint(&p->datafile, a, errstr)!=0)
			return -1;
		break;

	default:
		sprintf(errstr, "'%s' requires parameter from [ %s]",
			argv[0],
			collect_strings(options, data_set_pairs,
					N_DATA_SET, " "));
		return -1;
	}

	return 0;
}

#define CMD_MAX_ARGS 10

int command(params_t *p, int client_idx, char *str)
{
	char message[MEDLEN] = "1Ok";
	char *errstr = message + 1;
	char emptiness = 0;

	// Break command string into argument words
	char *argv[CMD_MAX_ARGS];
	int  argc = CMD_MAX_ARGS;
	store_tokens(&argc, argv, str);

	// Point other arguments to empty strings
	int i;
	for (i=argc; i<CMD_MAX_ARGS; i++)
		argv[i] = &emptiness;

	int err = 0;
	int key = lookup_key(data_key_pairs, N_DATA_KEY, argv[0]);

	switch(key) {

	case DATA_QUIT:
		printf("Received quit from client[%i]\n", client_idx);
		quit(p);
		break;

	case DATA_SET:
		err = command_set(p, client_idx,
				  argc, argv, errstr);
		break;

	case DATA_FLUSH:
		printf("Received flush from client[%i]\n", client_idx);
		data_flush(&p->datafile);
		break;


	case DATA_OPENFILE:
		printf("Received openfile from client [%i]\n", client_idx);
		err = data_openfile(&p->datafile, errstr);
		break;

	case DATA_CLOSEFILE:
		printf("Received closefile from client [%i]\n", client_idx);
		err = data_closefile(&p->datafile, errstr);
		break;

/* 	case DATA_OPENDIR: */
/* 		printf("Received opendir from client [%i]\n", client_idx); */
/* 		err = data_setdir(p, errstr, nextword); */
/* 		break; */

	case DATA_GO:
		printf("Received GO from client[%i]\n", client_idx);
		err = data_go(p, errstr);
		break;

	case DATA_STOP:
		printf("Received STOP from client[%i]\n", client_idx);
		err = data_stop(p, errstr);
		break;

	case DATA_REPORT:
		sprintf(errstr, "Frames acquired: %i, checksum errors: %i, "
			"dropped: %i", p->data_data.count, p->data_data.chksum,
			p->data_data.drop);
		break;

	case DATA_FAKESTOP:
		printf("Received fakestop from client[%i].\n", client_idx);
		if ( (err=data_fakestop(&p->mce_comms)) != 0) {
			data_client_reply(p, client_idx,
					  "0Could not fakestop.");
		}
		break;

	case DATA_BLOCK:
		p->server.clients[client_idx].blocking = 1;
		printf("Client [%i] blocking for acquisition completion.\n",
		       client_idx);
		while ( data_checkflags(&p->data_data, FLAG_GO) &&
		        !data_checkflags(&p->data_data, FLAG_DONE) )
			usleep(1);
		break;

	default:
		sprintf(errstr, "Invalid command '%s'", argv[0]);
		err = -1;
	}

	*message = err ? '0' : '1';
	
	data_client_reply(p, client_idx, message);
	
	return 0;

}

/* Entry point for commands */

int data_client_process(params_t *p, int client_idx)
{
	CHECKPTR;

	client_t *l_client =
		p->server.listener.clients + client_idx;

	//Check for complete commands;
	char *buf   = l_client->recv_buf;
	int  size   = l_client->recv_idx;

	char *start = l_client->recv_buf;
	char *stop  = start;

	while (stop < buf + size) {
		if (*stop=='\n') *stop = 0;
		if (*stop==0) {
			command(p, client_idx, start);
			start = stop+1;
			stop = start;
			continue;
		}
		stop++;
	}
	
	//Move unprocessed data to the beginning of the buffer

	l_client->recv_idx = stop-start;
	if (stop-start == MAX_MSG) {
		fprintf(stderr, "Message buffer exceeded, discarding!\n");
		l_client->recv_idx = 0;
		return -1;
	}

	while (start < stop)
		*(buf++) = *(start++);

	return 0;
}


int data_client_reply(params_t *p, int client_idx, char *message)
{
	client_t *l_client = p->server.listener.clients + client_idx;

	listener_flags lf = client_send( l_client, message, strlen(message)+1 );
	if (lf & LISTENER_ERR) {
		fprintf(stderr, "Client[%i]: Could not send reply\n", client_idx);
		return -1;
	}
	
	return 0;
}

