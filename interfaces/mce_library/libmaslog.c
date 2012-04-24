/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "mce/socks.h"
#include "mce/defaults.h"
#include <libconfig.h>

#include "context.h"
#include <libmaslog.h>

#define CONFIG_CLIENT "log_client"
#define CONFIG_LOGADDR "log_address"

#define CLIENT_NAME "\x1b" "LOG:client_name "


static
int destroy_exit(struct config_t *cfg, int error) {
	config_destroy(cfg);
	return error;
}

static
int get_string(char *dest,
	       config_setting_t *parent, const char *name)
{
	config_setting_t *set =
		config_setting_get_member(parent, name);
	if (set==NULL) {
		fprintf(stderr, "%s: key '%s' not found in config file\n", __func__,
                name);
		return -1;
	}
	strcpy(dest, config_setting_get_string(set));
	return 0;
}


int maslog_connect(maslog_t *logger, char *config_file, char *name)
{
	struct config_t cfg;
	config_init(&cfg);

	if (logger==NULL) {
		fprintf(stderr, "%s: Null maslog_t pointer!\n", __func__);
		return -1;
	}

	logger->fd = 0;

	if (config_file!=NULL) {
		if (!config_read_file(&cfg, config_file)) {
			fprintf(stderr,
				"%s: Could not read config file '%s'\n", __func__,
				config_file);
			return -1;
		}
	} else {
        char *ptr = mcelib_default_masfile();
        if (ptr == NULL)
            fprintf(stderr, "Unable to obtain path to default configfile!\n");
        else if (!config_read_file(&cfg, ptr)) {
			fprintf(stderr, "%s: Could not read default configfile '%s'\n",
                    __func__, ptr);
			return -1;
		}
        free(ptr);
	}

	config_setting_t *client = config_lookup(&cfg, CONFIG_CLIENT);
	
	char address[SOCKS_STR];
	if (get_string(address, client, CONFIG_LOGADDR)!=0)
		return destroy_exit(&cfg, -2);

	int sock = massock_connect(address, -1);
	if (sock<0) {
		fprintf(stderr, "%s: could not connect to logger at %s\n", __func__,
			address);
		return destroy_exit(&cfg, -3);
	}

	// Ignore pipe signals, we'll handle the errors (right?)
	signal(SIGPIPE, SIG_IGN);

	char cmd[SOCKS_STR];
	sprintf(cmd, "%c%s%s", '0'+LOGGER_ALWAYS, CLIENT_NAME, name);
	int sent = send(sock, cmd, strlen(cmd)+1, 0);
	if (sent != strlen(cmd)+1) {
		fprintf(stderr, "%s: failed to send client name\n", __func__);
	}

	logger->fd = sock;

	return destroy_exit(&cfg, 0);
}

int maslog_print(maslog_t *logger, const char *str)
{
	return maslog_print_level(logger, str, LOGGER_ALWAYS);
}

int maslog_print_level(maslog_t *logger, const char *str, int level)
{
	if (logger==NULL || logger->fd<=0) return -1;

	char packet[2048];
	int idx = 0;

	while (*str != 0) {
		packet[idx++] = '0' + level;
		while (*str != 0 && *str != '\n') {
			packet[idx++] = *str++;
		}
		if (*str == '\n') str++;
		packet[idx++] = 0;
	}

	int sent = send(logger->fd, packet, idx, 0);
	if (sent != idx) {
		if (sent==0) {
			fprintf(stderr, "%s: connection closed, no further logging.\n",
                    __func__);
			maslog_close(logger);
		} else if (sent<0) {
			fprintf(stderr, "%s: pipe error, errno=%i, no further logging.\n",
                    __func__, errno);
			maslog_close(logger);
		} else {
			fprintf(stderr, "%s: partial send, logging will continue.\n",
                    __func__);
		}
	}
	return (logger->fd > 0) ? 0 : -1;
}

int maslog_write(maslog_t *logger, const char *buf, int size)
{
	if (logger==NULL || logger->fd<=0) return -1;
	
	int sent = send(logger->fd, buf, size, 0);
	if (sent != size) {
		fprintf(stderr, "%s: logging failed, (send error %i/%i)\n", __func__,
			sent, size);
		maslog_close(logger);
		return -1;
	}
	return 0;
}

int maslog_close(maslog_t *logger)
{
	if (logger==NULL || logger->fd<=0) return -1;

	int fd = logger->fd;
	logger->fd = 0;

	int ret = close(fd);
	if (ret!=0) {
		fprintf(stderr, "%s: close error, errno=%i\n", __func__, ret);
	}

	return ret;
}
