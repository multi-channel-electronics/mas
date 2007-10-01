#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libmasdata.h"

#define CONTROL "goctrl"

#define MEDLEN 1024

#define DEFAULT_CFG "/etc/mas.cfg"

char config_file[MEDLEN] = DEFAULT_CFG;
int  command_now = 0;
char *line;

int setup_command(int argc, char **argv, int optind);
int process_options(int argc, char **argv);
char *trim(char *line);

int main(int argc, char **argv) {

	line = (char*)malloc(MEDLEN);
	char *reply = (char*)malloc(MEDLEN);

	if (process_options(argc, argv))
		return 1;

	int sock = data_connect(config_file, CONTROL);
	if (sock<0)
	    return 1;

	int done = 0;
	int retval = 1;
	while ( !done ) {

		retval = 0;

		if (command_now) {
			// Command passed on command line, process and exit.
			printf("Command: %s:", line);
			done = 1;
		} else {
			printf("Command:  ");

			int nin = MEDLEN-1;
			int nout = getline(&line, &nin, stdin);
			if (nout > 0)
				command_now = 1;
		}
	       
		if (command_now) {
			command_now = 0;
			char *line_trim = trim(line);

			if (strcmp(line_trim, "exit")==0)
				break;

			int send_err = data_command(sock, line_trim, reply);
			if (send_err!=0) {
				printf(" Exiting: %s\n", reply+1);
				break;
			}
			
			if (reply[0]=='1') {
				printf(" Success: %s\n", reply+1);
				retval = 0;
			} else {
				printf(" Failure: %s\n", reply+1);
				retval = 1;
			}
		}

		if (feof(stdin)) {
			printf("\n");
			done = 1;
		}
	}

	free(reply);
	free(line);
	data_close(sock);

	return retval;
}


char *trim(char *line)
{
	char *s = line;

	while (*s == ' ' || *s=='\t' || *s=='\n') s++;
	char *trimmed = s;

	if (*trimmed=='#') *trimmed = 0;

	while (*s != 0) s++;
	
	if (s==line) return trimmed;
	
	s--;
	while (*s == ' ' || *s=='\t' || *s=='\n') s--;
	*(++s) = 0;
	return trimmed;
}

int setup_command(int argc, char **argv, int optind)
{
	if (line==NULL) {
		fprintf(stderr, "Memory, tsk tsk.\n");
		return -1;
	}

	char *s = line;
	while (optind < argc) {
		s += sprintf(s, "%s ", argv[optind++]);
	}
	command_now = 1;
	
	return 0;
}


int process_options(int argc, char **argv)
{
	int option;
	while ( (option = getopt(argc, argv, "x:f:h")) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf("Usage:\n\t%s [-f <config file>] [-x <command>]\n",
			       argv[0]);
			return -1;

		case 'x':
			setup_command(argc, argv, optind-1);
			break;

		case 'f':
			strcpy(config_file, optarg);
			break;

		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}

	return 0;
}

