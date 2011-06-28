/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <mcedsp.h>
#include <mcenetd.h>
#include <mce_library.h>
#include <mce/defaults.h>

#include "../../defaults/config.h"

/* maximum chain length */
#define MAX_DEPTH 10

/* number of consecutive request errors we're willing to tollerate. */
#define MAX_ERR 3

#define VERSION 1

/* socket management */
#define FDTYPE_CTL 0
#define FDTYPE_DSP 1
#define FDTYPE_CMD 2
#define FDTYPE_DAT 3
struct pollfd *pfd = NULL;
nfds_t npfd = 0;
int pfd_s = 0;

struct confd_struct {
    int type;
    int con;
} *confd;

/* connection management */
int ser[256];
int serind = 0;
struct con_struct {
    struct sockaddr_in addr;

    int ctl_sock;
    int dsp_sock;
    int cmd_sock;
    int dat_sock;

    int dev_idx;
    int udepth;
    unsigned char ser;

    int state;
    int nerr;

    unsigned char rsp[256];
    size_t rsplen;
} *con = NULL;
int ncon = 0;
int con_s = 0;

/* mce stuff */
struct dev_struct {
    mce_context_t mce;
    const char *url;
    unsigned char ddepth;
    unsigned char endpt;

    unsigned char flags;
} *dev;
int ndev = 0;

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

static char spit_buffer[1024];
const char *SpitMessage(const unsigned char *m, ssize_t n)
{
    ssize_t i;
    char *ptr = spit_buffer;
    for (i = 0; i < n; ++i) {
        sprintf(ptr, "%02hhx ", m[i]);
        ptr += 3;
    }
    *ptr = '\0';

    return spit_buffer;
}

int AddSocket(int fd, short events, int type, int c)
{
    /* allocate, if necessary */
    if (pfd_s == npfd) {
        pfd_s += 16;
        void *ptr = realloc(pfd, pfd_s * sizeof(struct pollfd));

        if (ptr == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");

        pfd = (struct pollfd*)ptr;

        ptr = realloc(confd, pfd_s * sizeof(struct confd_struct));

        if (ptr == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");

        confd = (struct confd_struct*)ptr;
    }

    confd[npfd].con = c;
    confd[npfd].type = type;
    pfd[npfd].fd = fd;
    pfd[npfd].events = events;

    return npfd++;
}

void DropSocket(int p)
{
    if (p >= 0) {
        npfd--;
        shutdown(pfd[p].fd, SHUT_RDWR);
        confd[p] = confd[npfd];
        pfd[p] = pfd[npfd];
    }
}

void DropConnection(int c) {
    ser[con[c].ser] = 0;
    DropSocket(con[c].dat_sock);
    DropSocket(con[c].cmd_sock);
    DropSocket(con[c].dsp_sock);
    DropSocket(con[c].ctl_sock);

    con[c] = con[--ncon];
}

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
    pfd_s = npfd = 4;
    pfd = malloc(pfd_s * sizeof(struct pollfd));

    /* try to open all our ports. */
    if ((pfd[0].fd = Listen(MCENETD_CTLPORT)) < 0)
        lprintf(LOG_CRIT, "Unable to listen on port %i.\n", MCENETD_CTLPORT);

    if ((pfd[1].fd = Listen(MCENETD_DSPPORT)) < 0)
        lprintf(LOG_CRIT, "Unable to listen on port %i.\n", MCENETD_DSPPORT);

    if ((pfd[2].fd = Listen(MCENETD_CMDPORT)) < 0)
        lprintf(LOG_CRIT, "Unable to listen on port %i.\n", MCENETD_CMDPORT);

    if ((pfd[3].fd = Listen(MCENETD_DATPORT)) < 0)
        lprintf(LOG_CRIT, "Unable to listen on port %i.\n", MCENETD_DATPORT);

    /* listening sockets need only read */
    pfd[0].events = pfd[1].events = pfd[2].events = pfd[3].events = POLLIN;
}

static void MCEInit(void)
{
    int i;

    /* hah: we'll change this as soon as we know the real answer. */
    ndev = 1;
    dev = malloc(sizeof(struct dev_struct));

    for (i = 0; i < ndev; ++i) {
        dev[i].flags = 0;

        /* get a context, if this fails, we disable the card completely */
        if ((dev[i].mce = mcelib_create(i, options.mas_file, 0)) == NULL)
            lprintf(LOG_ERR, "Unable to get MCE context #%i.\n", i);
        else {
            /* update the number of devices */
            if (i == 0) {
                ndev = mcelib_ndev(dev[0].mce);
                struct dev_struct *ptr = realloc(dev,
                        ndev * sizeof(struct dev_struct));

                if (ptr == NULL)
                    lprintf(LOG_CRIT, "Memory allocation error.\n");

                dev = ptr;
            }

            dev[i].url = mcelib_dev(dev[i].mce);
            dev[i].ddepth = (unsigned char)mcelib_ddepth(dev[i].mce) + 1;
            dev[i].flags |= MCENETD_F_MAS_OK;

            /* open the devices */
            if (mcedsp_open(dev[i].mce))
                lprintf(LOG_ERR, "Unable to open DSP for %s\n", dev[i].url);
            else
                dev[i].flags |= MCENETD_F_DSP_OK;

            if (mcecmd_open(dev[i].mce))
                lprintf(LOG_ERR, "Unable to open CMD for %s\n", dev[i].url);
            else
                dev[i].flags |= MCENETD_F_CMD_OK;

            if (mcedata_open(dev[i].mce))
                lprintf(LOG_ERR, "Unable to open DAT for %s\n", dev[i].url);
            else
                dev[i].flags |= MCENETD_F_DAT_OK;
        }

        lprintf(LOG_INFO,
                "MCE%i:  mas:%s  dsp:%s  cmd:%s  dat:%s\n", i,
                (dev[i].flags & MCENETD_F_MAS_OK) ? "ok" : "FAIL",
                (dev[i].flags & MCENETD_F_DSP_OK) ? "ok" : "FAIL",
                (dev[i].flags & MCENETD_F_CMD_OK) ? "ok" : "FAIL",
                (dev[i].flags & MCENETD_F_DAT_OK) ? "ok" : "FAIL");
    }
}

/* verify a hello message.  On error, drop the connection */
void Hello(int p, int c, unsigned char *message, ssize_t n)
{
    const struct dev_struct *d;

    /* the following anti-DoS code will accept 1 in 2**32 spurious connections
    */
    if (message[0] != MCENETD_HELLO || message[1] != MCENETD_MAGIC1
            || message[2] != MCENETD_MAGIC2 || message[3] != MCENETD_MAGIC3)
    {
        lprintf(LOG_INFO, "dropped spurious connection from %s (CLx%02hhX).\n",
                inet_ntoa(con[c].addr.sin_addr), con[c].ser);
        con[c].state = -1;
        return;
    }

    /* data */
    con[c].udepth = message[4];
    con[c].dev_idx = message[5];
    d = dev + con[c].dev_idx;

    /* send a response */
    if (con[c].dev_idx > ndev) {
        /* no such device */
        con[c].rsp[0] = MCENETD_STOP;
        con[c].rsp[1] = MCENETD_ERR_INDEX;
        con[c].rsplen = 2;
    } else if (con[c].udepth + d->ddepth > MAX_DEPTH) {
        /* too much recurstion */
        con[c].rsp[0] = MCENETD_STOP;
        con[c].rsp[1] = MCENETD_ERR_BALROG;
        con[c].rsplen = 2;
    } else {
        /* we're good to go */
        con[c].rsp[0] = MCENETD_READY;
        con[c].rsp[1] = VERSION;
        con[c].rsp[2] = con[c].ser;
        con[c].rsp[3] = d->ddepth;
        con[c].rsp[4] = d->endpt;
        con[c].rsp[5] = d->flags;
        con[c].rsplen = 6;
    }

    /* register the pending response */
    pfd[p].events = POLLOUT;
}

/* send a response on the control channel */
void CtlWrite(int p, int c)
{
    ssize_t n;

    lprintf(LOG_DEBUG, "ctl: s:%i <== %s\n", con[c].state,
            SpitMessage(con[c].rsp, con[c].rsplen));

    n = write(pfd[p].fd, con[c].rsp, con[c].rsplen);

    if (n < 0) {
        lprintf(LOG_ERR, "write error: %s\n", strerror(errno));
    } else if (n < con[c].rsplen) {
        lprintf(LOG_ERR, "short write on response to CLx%02hhX.\n", con[c].ser);
    }

    /* error check */
    if (con[c].rsp[0] == MCENETD_STOP) {
        if (++con[c].nerr >= MAX_ERR)
            con[c].state = -1;
    } else
        con[c].nerr = 0;

    /* done writing */
    pfd[p].events = POLLIN;
}

ssize_t NetRead(int d, int c, unsigned char *buf, const char *where,
        unsigned int who)
{
    ssize_t n;

    n = mcenet_readmsg(d, buf);

    if (n < 0) {
        lprintf(LOG_ERR, "read error from CLx%02hhX: %s\n", who,
                strerror(errno));
    } else if (n == 0) {
        lprintf(LOG_INFO, "%s connection closed by CLx%02hhX.\n", where, who);
        con[c].state = -1;
        return -1;
    }

    return n;
}

/* process a request on the control channel */
void CtlRead(int p, int c)
{
    ssize_t n;
    unsigned char message[256];

    /* read the requested operation */
    n = NetRead(pfd[p].fd, c, message, "CTL", con[c].ser);

    if (n < 0)
        return;

    lprintf(LOG_DEBUG, "ctl: s:%i ==> %s\n", con[c].state,
            SpitMessage(message, n));

    switch (con[c].state) {
        case 0: /* trying to verify connection, expecting HELLO */
            Hello(p, c, message, n);
            break;
        default:
            lprintf(LOG_CRIT, "Unhandled state: CTL#%i\n", con->state);
    }
}

/* accept a new conenction on the control port */
void NewClient(void)
{
    int fd;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    /* allocate, when needed. */
    if (ncon == con_s) {
        con_s += 16;
        struct con_struct *ptr = (struct con_struct *)realloc(con,
                con_s * sizeof(struct con_struct));

        if (ptr == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");

        con = ptr;
    }

    /* accept the connection, and store it. */
    fd = accept(pfd[0].fd, (struct sockaddr*)&con[ncon].addr, &addrlen);

    /* find a serial number */
    int old_serind = serind;
    while (ser[serind]) {
        serind = (serind + 1) % 256;
        if (old_serind == serind) {
            /* ran out of serial numbers; drop the connection */
            lprintf(LOG_WARNING, "no available connection!\n");
            shutdown(fd, SHUT_RDWR);
            return;
        }
    }

    /* add it to the poll list */
    con[ncon].ctl_sock = AddSocket(fd, POLLIN, FDTYPE_CTL, ncon);
    con[ncon].dsp_sock = con[ncon].cmd_sock = con[ncon].dat_sock = -1;

    /* initialise */
    con[ncon].state = 0;
    con[ncon].nerr = 0;
    con[ncon].ser = serind;
    ser[serind++] = 1;

    ncon++;

    /* whine */
    lprintf(LOG_INFO, "accepted control connection from %s as CLx%02hhX.\n",
            inet_ntoa(con[ncon - 1].addr.sin_addr), serind - 1);
}

int main(int argc, char **argv)
{
    int opt, i;

    while ((opt = getopt(argc, argv, "c:f")) >= 0) {
        switch(opt) {
            case '?':
                puts(
                        "Usage:\n\n"
                        "  mcenetd [OPTION]...\n\n"
                        "Available options:\n"
                        "  -c FILE       read configuration from FILE instead "
                        "of the default mas.cfg.\n"
                        "  -f            don't fork to background on "
                        "start-up.\n"
                    );
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

    memset(ser, 0, 256 * sizeof(int));

    lprintf(LOG_INFO, "mcenetd proto_vers#%i (C) 2011 D. V. Wiebe\n", VERSION);

    /* initialise MAS and open all the devices */
    MCEInit();

    /* initialise the network interface */
    NetInit();

    /* poll loop */
    for (;;) {
        /* wait for ever, or at least until something happens. */
        int n = poll(pfd, npfd, -1);

        if (n < 0)
            lprintf(LOG_CRIT, "poll error: %s\n", strerror(errno));

        if (pfd[0].revents & POLLIN)
            NewClient();

        if (pfd[1].revents & POLLIN) {
            lprintf(LOG_CRIT, "can't handle read on socket#1\n");
        }

        if (pfd[2].revents & POLLIN) {
            lprintf(LOG_CRIT, "can't handle read on socket#2\n");
        }

        if (pfd[3].revents & POLLIN) {
            lprintf(LOG_CRIT, "can't handle read on socket#3\n");
        }

        for (i = 4; i < npfd; ++i) {
            /* skip connections about to be closed */
            if (con[confd[i].con].state == -1)
                continue;

            if (pfd[i].revents & POLLHUP) {
                lprintf(LOG_INFO, "%s connection closed by CLx%02hhX.\n",
                        (confd[i].type == FDTYPE_CTL) ? "CTL" :
                        (confd[i].type == FDTYPE_DSP) ? "DSP" :
                        (confd[i].type == FDTYPE_CMD) ? "CMD" : "DAT",
                        con[confd[i].con].ser);
                con[confd[i].con].state = -1;
                continue;
            }

            if (pfd[i].revents & POLLOUT) {
                switch (confd[i].type) {
                    case FDTYPE_CTL:
                        CtlWrite(i, confd[i].con);
                        break;
                    case FDTYPE_DSP:
                        lprintf(LOG_CRIT, "can't handle write on dsp#%i\n", i);
                    case FDTYPE_CMD:
                        lprintf(LOG_CRIT, "can't handle write on cmd#%i\n", i);
                    case FDTYPE_DAT:
                        lprintf(LOG_CRIT, "can't handle write on dat#%i\n", i);
                }
            }

            if (pfd[i].revents & POLLIN) {
                switch (confd[i].type) {
                    case FDTYPE_CTL:
                        CtlRead(i, confd[i].con);
                        break;
                    case FDTYPE_DSP:
                        lprintf(LOG_CRIT, "can't handle read on dsp#%i\n", i);
                    case FDTYPE_CMD:
                        lprintf(LOG_CRIT, "can't handle read on cmd#%i\n", i);
                    case FDTYPE_DAT:
                        lprintf(LOG_CRIT, "can't handle read on dat#%i\n", i);
                }
            }
        }

        /* process pending closes */
        for (i = 0; i < ncon; ++i)
            if (con[i].state == -1)
                DropConnection(i);
    }
}
