/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "mce_library.h"
#include "mce/socks.h"
#include "mce/defaults.h"
#include <libconfig.h>

#include "libmaslog.h"

#define CONFIG_CLIENT "log_client"
#define CONFIG_LOGADDR "log_address"

#define CLIENT_NAME "\x1b" "LOG:client_name "


struct maslog_struct {
	int fd;       // ha ha, it's just an int.
};

static
int get_string(char *dest, config_setting_t *parent, const char *name)
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


maslog_t maslog_connect(const mce_context_t *mce, const char *name)
{
	struct config_t *cfg;
    maslog_t maslog = (maslog_t)malloc(sizeof(struct maslog_struct));

    maslog->fd = 0;
    cfg = mce->mas_cfg;

	config_setting_t *client = config_lookup(cfg, CONFIG_CLIENT);
	
	char address[SOCKS_STR];
	if (get_string(address, client, CONFIG_LOGADDR)!=0)
		return NULL;

	int sock = massock_connect(address);
	if (sock<0) {
		fprintf(stderr, "%s: could not connect to maslog at %s\n", __func__,
			address);
		return NULL;
	}

	// Ignore pipe signals, we'll handle the errors (right?)
	signal(SIGPIPE, SIG_IGN);

	char cmd[SOCKS_STR];
	sprintf(cmd, "%c%s%s", '0'+MASLOG_ALWAYS, CLIENT_NAME, name);
	int sent = send(sock, cmd, strlen(cmd)+1, 0);
	if (sent != strlen(cmd)+1) {
		fprintf(stderr, "%s: failed to send client name\n", __func__);
	}

	maslog->fd = sock;

	return maslog;
}

int maslog_print(maslog_t maslog, const char *str)
{
	return maslog_print_level(maslog, str, MASLOG_ALWAYS);
}

int maslog_print_level(maslog_t maslog, const char *str, int level)
{
    if (maslog == NULL || maslog->fd <= 0)
        return -1;

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

    int sent = send(maslog->fd, packet, idx, 0);
	if (sent != idx) {
		if (sent==0) {
			fprintf(stderr, "%s: connection closed, no further logging.\n",
                    __func__);
            maslog_close(maslog);
		} else if (sent<0) {
			fprintf(stderr, "%s: pipe error: %s, no further logging.\n",
                    __func__, strerror(errno));
            maslog_close(maslog);
		} else {
			fprintf(stderr, "%s: partial send, logging will continue.\n",
                    __func__);
		}
	}
	return (maslog->fd > 0) ? 0 : -1;
}

int maslog_write(maslog_t maslog, const char *buf, int size)
{
	if (maslog==NULL || maslog->fd<=0) return -1;
	
	int sent = send(maslog->fd, buf, size, 0);
	if (sent != size) {
		fprintf(stderr, "%s: logging failed, (send error %i/%i)\n", __func__,
			sent, size);
		maslog_close(maslog);
		return -1;
	}
	return 0;
}

int maslog_close(maslog_t maslog)
{
	if (maslog==NULL || maslog->fd <= 0)
        return -1;

	int fd = maslog->fd;
	maslog->fd = 0;

	int ret = close(fd);
	if (ret != 0) {
		fprintf(stderr, "%s: close error, errno=%i\n", __func__, ret);
    } else
        free(maslog);

	return ret;
}
