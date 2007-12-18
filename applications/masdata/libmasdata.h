#ifndef _LIBMASLOG_H_
#define _LIBMASLOG_H_

/*
   When using libmcelog it is necessary to also link to libconfig,
   i.e. "gcc src.c -o src -lmcelog -lconfig"

   Since libconfig likes to install in /usr/local/lib, the application
   will fail to find libconfig.so.2 unless this folder is added to the
   shared library path.  This is most easily accomplished by adding
   /usr/local/lib to /etc/ld.so.conf .
*/

#define MEDLEN 1024


int data_close(int sock);
int data_command(int sock, const char *str, char *reply);
int data_connect(char *config_file, char *name);

#endif
