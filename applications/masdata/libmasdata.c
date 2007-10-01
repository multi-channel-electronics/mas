#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <libconfig.h>

#include <socks.h>
#include "libmasdata.h"

#define CONFIG_FILE "/etc/mas.cfg"

#define CONFIG_CLIENT "data_client"
#define CONFIG_ADDR   "address"


int destroy_exit(struct config_t *cfg, int error) {
	config_destroy(cfg);
	return error;
}

#define SUBNAME "data_connect: "

int get_string(char *dest,
	       config_setting_t *parent, const char *name)
{
	config_setting_t *set =
		config_setting_get_member(parent, name);
	if (set==NULL) {
		fprintf(stderr, SUBNAME
			"key '%s' not found in config file\n",
			name);
		return -1;
	}
	strcpy(dest, config_setting_get_string(set));
	return 0;
}


int data_connect(char *config_file, char *name)
{
	struct config_t cfg;
	config_init(&cfg);

	if (config_file!=NULL) {
		if (!config_read_file(&cfg, config_file)) {
			fprintf(stderr,
				SUBNAME "Could not read config file '%s'\n",
				config_file);
			return -1;
		}
	} else {
		if (!config_read_file(&cfg, CONFIG_FILE)) {
			fprintf(stderr, SUBNAME
				"Could not read default configfile '%s'\n",
				CONFIG_FILE);
			return -1;
		}
	}

	config_setting_t *client = config_lookup(&cfg, CONFIG_CLIENT);
	
	char address[1024];
	if (get_string(address, client, CONFIG_ADDR)!=0)
		return destroy_exit(&cfg, -2);

	int sock = connect_to_addr(address);
	if (sock<0) {
		fprintf(stderr, SUBNAME "could not connect to data at %s\n",
			address);
		return destroy_exit(&cfg, -3);
	}

	return destroy_exit(&cfg, sock);
}

#undef SUBNAME

#define SUBNAME "data_command: "

int data_command(int sock, const char *str, char *reply)
{
	if (sock<=0) return -1;

	int sent = send(sock, str, strlen(str)+1, 0);
	if (sent != strlen(str)+1) {
		fprintf(stderr, SUBNAME "command failed, (send error %i/%i)\n",
			sent, strlen(str)+1);
		sprintf(reply, "0send failed with errno=%i", errno);
		return -1;
	}
	
	int recd = recv(sock, reply, MEDLEN, 0);

	if (recd<0) {
		sprintf(reply, "0recv failed with errno=%i", errno);
		return -1;
	}
	if (recd==0) {
		sprintf(reply, "0Connection closed.");
		return -1;
	}

	return 0;
}

#undef SUBNAME


int data_close(int sock)
{
	if (sock<=0) return -1;
	close(sock);
	return 0;
}
