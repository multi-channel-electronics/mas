#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <mcedsp.h>
#include <mce_library.h>
#include <mce/defaults.h>

#include "../../defaults/config.h"

#define VERSION "0.1"

#define BASE_PORT 1200

int con_sock;
struct {
  mce_context_t *mce;

  char *dsp_dev;
  char *cmd_dev;
  char *dat_dev;
  char *mce_cfg;

  int mas_ok;
  int dsp_ok;
  int cmd_ok;
  int dat_ok;
  int cfg_ok;

  int dsp_handle;

  int dsp_sock;
  int cmd_sock;
  int dat_sock;
} pci[MAX_FIBRE_CARD];

static struct {
  int daemonise;
  char *mas_file;
} options = { 1, NULL };

static const char *marker[] = {
  NULL, /* LOG_EMERG   -- not used by MCENETD */
  NULL, /* LOG_ALERT   -- not used by MCENETD */
  "!!", /* LOG_CRIT    */
  "EE", /* LOG_ERR     */
  "WW", /* LOG_WARNING */
  "nn", /* LOG_NOTICE  */
  "ii", /* LOG_INFO    */
  "--"  /* LOG_DEBUG   */
};

#define lprintf(l,f,...) do { \
  if (options.daemonise) syslog(l, f, ##__VA_ARGS__); \
  else printf("[%s] " f, marker[l], ##__VA_ARGS__); \
  if (l == LOG_CRIT) exit(1); \
} while (0)

/* listen on port */
static int Listen(int port)
{
  int sock, n;
  struct sockaddr_in addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    lprintf(LOG_ERR, "Unable to create socket: %s\n", strerror(errno));
    return -1;
  }

  /* to simplify restarting the daemon */
  n = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));
  
  /* bind */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)) == -1) {
    lprintf(LOG_ERR, "Unable to bind to port: %s\n", strerror(errno));
    return -1;
  }

  /* listen */
  if (listen(sock, 100) == -1) {
    lprintf(LOG_ERR, "Unable to listen on port: %s\n", strerror(errno));
    return -1;
  }

  return sock;
}

static void NetInit(void)
{
  int i;
  /* try to open all our ports.  There is a 1-1 correspondance between
   * network port and mce_dsp device:
   *
   * BASE_PORT               == control channel
   * BASE_PORT + 10 * N + 10 == /dev/mce_dsp<N>
   * BASE_PORT + 10 * N + 11 == /dev/mce_cmd<N>
   * BASE_PORT + 10 * N + 12 == /dev/mce_data<N>
   * 
   */
  if ((con_sock = Listen(BASE_PORT)) < 0)
    lprintf(LOG_CRIT, "Unable to listen on port %i.\n", BASE_PORT);

  for (i = 0; i < MAX_FIBRE_CARD; ++i) {
    pci[i].dsp_sock = pci[i].cmd_sock = pci[i].dat_sock = 0;

    if (pci[i].dsp_ok) {
      if ((pci[i].dsp_sock = Listen(BASE_PORT + 10 * i + 10)) < 0)
        lprintf(LOG_ERR, "Unable to listen on port %i.\n", BASE_PORT);
    }

    if (pci[i].cmd_ok) {
      if ((pci[i].cmd_sock = Listen(BASE_PORT + 10 * i + 11)) < 0)
        lprintf(LOG_ERR, "Unable to listen on port %i.\n", BASE_PORT);
    }

    if (pci[i].dat_ok) {
      if ((pci[i].dat_sock = Listen(BASE_PORT + 10 * i + 12)) < 0)
        lprintf(LOG_ERR, "Unable to listen on port %i.\n", BASE_PORT);
    }

    lprintf(LOG_INFO, "MCENETD%i:  dsp:%s  cmd:%s  data:%s\n", i,
        (pci[i].dsp_sock >= 0) ? "online" : "OFFLINE",
        (pci[i].cmd_sock >= 0) ? "online" : "OFFLINE",
        (pci[i].dat_sock >= 0) ? "online" : "OFFLINE");
  }
}

static void MCEInit(void)
{
  int i;

  for (i = 0; i < MAX_FIBRE_CARD; ++i) {
    pci[i].mas_ok = pci[i].dsp_ok = pci[i].cmd_ok = pci[i].dat_ok = 0;

    /* get a context, if this fails, we disable the card completely */
    if ((pci[i].mce = mcelib_create(i)) == NULL)
      lprintf(LOG_ERR, "Unable to get MCE context #%i.\n", i);
    else {
      pci[i].mas_ok = 1;
      /* get the names of the devices */
      pci[i].dsp_dev = mcelib_dsp_device(i);
      pci[i].cmd_dev = mcelib_cmd_device(i);
      pci[i].dat_dev = mcelib_data_device(i);
      pci[i].mce_cfg = mcelib_default_hardwarefile(i);

      /* open the devices */
      if ((pci[i].dsp_handle = dsp_open(pci[i].dsp_dev)) < 0)
        lprintf(LOG_ERR, "Open failed on %s\n", pci[i].dsp_dev);
      else
        pci[i].dsp_ok = 1;

      if (mceconfig_open(pci[i].mce, pci[i].mce_cfg, NULL))
        lprintf(LOG_ERR, "Unable to load hardware configuration from %s.\n",
            pci[i].mce_cfg);
      else
        pci[i].cfg_ok = 1;

      if (mcecmd_open(pci[i].mce, pci[i].cmd_dev))
        lprintf(LOG_ERR, "Unable to open %s\n", pci[i].cmd_dev);
      else
        pci[i].cmd_ok = 1;

      if (mcedata_open(pci[i].mce, pci[i].dat_dev))
        lprintf(LOG_ERR, "Unable to open %s\n", pci[i].cmd_dev);
      else
        pci[i].dat_ok = 1;
    }

    lprintf(LOG_INFO, "MCE%i:  mas:%s  hwconfig:%s  dsp:%s  cmd:%s  data:%s\n",
        i, pci[i].mas_ok >= 0 ? "ok" : "FAIL",
        pci[i].cfg_ok ? "ok" : "FAIL", pci[i].dsp_ok ? "ok" : "FAIL",
        pci[i].cmd_ok ? "ok" : "FAIL", pci[i].dat_ok ? "ok" : "FAIL");
  }
}

int main(int argc, char **argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "c:f")) >= 0) {
    switch(opt) {
      case '?':
        puts(
            "Usage:\n\n"
            "  mcenetd [OPTION]...\n\n"
            "Available options:\n"
            "  -c FILE       read configuration from FILE instead of the "
            "default mas.cfg.\n"
            "  -f            don't fork to background on start-up.\n");
        return 0;
      case 'c':
        if (options.mas_file)
          free(options.mas_file);
        options.mas_file = strdup(optarg);
        break;
      case 'f':
        options.daemonise = 0;
        break;
    }
  }

  if (options.daemonise) {
    int pid;
    FILE *stream;

    openlog("mcenetd", LOG_PID, LOG_DAEMON);

    if ((pid = (int)fork()) != 0) {
      if (pid == -1) {
        fprintf(stderr, "unable to fork to background\n");
        return 1;
      }

      if ((stream = fopen("/var/run/decomd.pid", "w")) == NULL)
        fprintf(stderr, "unable to write PID to disk\n");
      else {
        fprintf(stream, "%i\n", pid);
        fflush(stream);
        fclose(stream);
      }
      closelog();
      printf("PID = %i\n", pid);
      exit(0);
    }

    /* Daemonise */
    pid = chdir("/");
    stream =  freopen("/dev/null", "r", stdin);
    stream = freopen("/dev/null", "w", stdout);
    stream = freopen("/dev/null", "w", stderr);
    setsid();
  }

  lprintf(LOG_INFO, "mcenetd " VERSION " (C) 2011 D. V. Wiebe\n");

  /* initialise MAS and open all the devices */
  MCEInit();

  /* initialise the network interface */
  NetInit();
}
