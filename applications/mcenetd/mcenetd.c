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

#define DEVID(p) ( ((confd[p].type & 3) == FDTYPE_DAT) ? "DAT" : \
        ((confd[p].type & 3) == FDTYPE_CMD) ? "CMD" : \
        ((confd[p].type & 3) == FDTYPE_DSP) ? "DSP" : "CTL")

/* socket management */
#define FDTYPE_CTL 0
#define FDTYPE_DSP 1
#define FDTYPE_CMD 2
#define FDTYPE_DAT 3
#define FDTYPE_HX  4
struct pollfd *pfd = NULL;
nfds_t npfd = 0;
int pfd_s = 0;

struct confd_struct {
    int type;
    int con;
    struct sockaddr_in addr;

    unsigned char *buf;
    size_t buflen;

    unsigned char rsp[256];
    size_t rsplen;
} *confd;

/* connection management */
#define CON_DROP   0x01
int ser[256];
int serind = 0;
struct con_struct {
    struct sockaddr_in addr;

    int sock[4];

    int dev_idx;
    int udepth;
    unsigned char ser;

    int flags;
    int nerr;
} *con = NULL;
int ncon = 0;
int con_s = 0;

/* mce stuff */
struct dev_struct {
    mce_context_t context;
    const char *url;
    unsigned char ddepth;
    unsigned char endpt;

    unsigned char flags;

    unsigned char *cmd_buf;
    size_t cmd_len;

    unsigned char *dat_buf;
    size_t dat_len;
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

int AddSocket(int fd, short events, int type, int c, struct sockaddr_in addr)
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
    confd[npfd].type = type | FDTYPE_HX;
    confd[npfd].addr.sin_addr = addr.sin_addr;
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
    lprintf(LOG_INFO, "Dropping connection to CLx%02hhX (%s).\n",
            con[c].ser, inet_ntoa(con[c].addr.sin_addr));
    ser[con[c].ser] = 0;
    DropSocket(con[c].sock[3]);
    DropSocket(con[c].sock[2]);
    DropSocket(con[c].sock[1]);
    DropSocket(con[c].sock[0]);

    con[c] = con[--ncon];
    ser[con[c].ser] = ncon + 1;
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
    confd = malloc(pfd_s * sizeof(struct confd_struct));

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
    confd[0].type = FDTYPE_CTL;
    confd[1].type = FDTYPE_DSP;
    confd[2].type = FDTYPE_CMD;
    confd[3].type = FDTYPE_DAT;
}

static void MCEInit(void)
{
    int i;

    /* hah: we'll change this as soon as we know the real answer. */
    ndev = 1;
    dev = malloc(sizeof(struct dev_struct));

    for (i = 0; i < ndev; ++i) {
        dev[i].flags = dev[i].cmd_len = dev[i].dat_len = 0;
        dev[i].cmd_buf = NULL;
        dev[i].dat_buf = NULL;

        /* get a context, if this fails, we disable the card completely */
        if ((dev[i].context = mcelib_create(i, options.mas_file, 0)) == NULL)
            lprintf(LOG_ERR, "Unable to get MCE context #%i.\n", i);
        else {
            /* update the number of devices */
            if (i == 0) {
                ndev = mcelib_ndev(dev[0].context);
                struct dev_struct *ptr = realloc(dev,
                        ndev * sizeof(struct dev_struct));

                if (ptr == NULL)
                    lprintf(LOG_CRIT, "Memory allocation error.\n");

                dev = ptr;
            }

            dev[i].url = mcelib_dev(dev[i].context);
            dev[i].ddepth = (unsigned char)mcelib_ddepth(dev[i].context) + 1;
            dev[i].flags |= MCENETD_F_MAS_OK;

            /* open the devices */
            if (mcedsp_open(dev[i].context))
                lprintf(LOG_ERR, "Unable to open DSP for %s\n", dev[i].url);
            else
                dev[i].flags |= MCENETD_F_DSP_OK;

            if (mcecmd_open(dev[i].context))
                lprintf(LOG_ERR, "Unable to open CMD for %s\n", dev[i].url);
            else
                dev[i].flags |= MCENETD_F_CMD_OK;

            if (mcedata_open(dev[i].context))
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
        con[c].flags |= CON_DROP;
        return;
    }

    /* data */
    con[c].udepth = message[4];
    con[c].dev_idx = message[5];
    d = dev + con[c].dev_idx;

    /* send a response */
    if (con[c].dev_idx > ndev) {
        /* no such device */
        confd[p].rsp[0] = MCENETD_STOP;
        confd[p].rsp[1] = MCENETD_ERR_INDEX;
        confd[p].rsplen = 2;
    } else if (con[c].udepth + d->ddepth > MAX_DEPTH) {
        /* too much recurstion */
        confd[p].rsp[0] = MCENETD_STOP;
        confd[p].rsp[1] = MCENETD_ERR_BALROG;
        confd[p].rsplen = 2;
    } else {
        /* we're good to go */
        confd[p].rsp[0] = MCENETD_READY;
        confd[p].rsp[1] = VERSION;
        confd[p].rsp[2] = con[c].ser;
        confd[p].rsp[3] = d->ddepth;
        confd[p].rsp[4] = d->endpt;
        confd[p].rsp[5] = d->flags;
        confd[p].rsplen = 6;
    }

    /* register the pending response */
    pfd[p].events = POLLOUT;
}

/* send a response on the control channel */
void RspWrite(int p, int c)
{
    ssize_t n;

    lprintf(LOG_DEBUG, "%s@CLx%02hhX: f:%x <== %s\n", DEVID(p), con[c].ser,
            con[c].flags, SpitMessage(confd[p].rsp, confd[p].rsplen));

    n = write(pfd[p].fd, confd[p].rsp, confd[p].rsplen);

    if (n < 0) {
        lprintf(LOG_ERR, "write error: %s\n", strerror(errno));
    } else if (n < confd[p].rsplen) {
        lprintf(LOG_ERR, "short write on response to CLx%02hhX.\n", con[c].ser);
    }

    /* error check */
    if (confd[c].type == FDTYPE_CTL && confd[p].rsp[0] == MCENETD_STOP) {
        if (++con[c].nerr >= MAX_ERR)
            con[c].flags |= CON_DROP;
    } else
        con[c].nerr = 0;

    /* done writing */
    pfd[p].events = POLLIN;
}

ssize_t NetRead(int d, int c, unsigned char *buf, const char *where,
        unsigned int who)
{
    ssize_t n;

    n = mcenet_readmsg(d, buf, -1);

    if (n < 0) {
        lprintf(LOG_ERR, "read error from CLx%02hhX: %s\n", who,
                strerror(errno));
    } else if (n == 0) {
        lprintf(LOG_INFO, "%s connection closed by CLx%02hhX.\n", where, who);
        con[c].flags |= CON_DROP;
        return -1;
    }

    return n;
}

/* process a read on the command port */
void CmdRead(int p, int c)
{
    ssize_t n;
    unsigned char message[256];
    struct dev_struct *d = dev + con[c].dev_idx;

    /* read the requested operation */
    n = NetRead(pfd[p].fd, c, message, "CMD", con[c].ser);

    if (n < 0)
        return;

    lprintf(LOG_DEBUG, "CMD@CLx%02hhX: f:%x ==> %s\n", con[c].ser, con[c].flags,
            SpitMessage(message, n));

    if (message[0] == MCENETD_MORE) {
        /* packetised data, accumulate it */

        /* resize */
        void *ptr = realloc(confd[p].buf, confd[p].buflen + 254);
        if (ptr == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");

        confd[p].buf = ptr;

        /* append data */
        memcpy(confd[p].buf + confd[p].buflen, message + 1, 254);

        /* update length */
        confd[p].buflen += 254;
    } else if (message[0] == MCENETD_CLOSURE) {
        /* end of a packetise write; send it; report the result */
        size_t l = message[1] - 2;
        ssize_t *n = (ssize_t *)(confd[p].rsp + 1);
        int32_t *err = (int32_t*)(confd[p].rsp + 5);

        /* resize */
        void *ptr = realloc(confd[p].buf, confd[p].buflen + l);
        if (ptr == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");

        confd[p].buf = ptr;

        /* append data */
        memcpy(confd[p].buf + confd[p].buflen, message + 2, l);

        /* write it! */
        *n = mcecmd_write(d->context, confd[p].buf, confd[p].buflen + l);

        /* report the result */
        confd[p].rsp[0] = MCENETD_RECEIPT;
        *err = (int32_t)errno;
        confd[p].rsplen = MCENETD_MSGLEN(MCENETD_RECEIPT);
        pfd[p].events = POLLOUT;
    } else if (message[0] == MCENETD_READ) {
        uint32_t *count = (uint32_t*)message + 1;

        /* try to read enough data to fill up the buffer */
        if (*count < d->cmd_len) {
            /* resize */
            void *ptr = realloc(d->cmd_buf, *count);
            if (ptr == NULL) 
                lprintf(LOG_CRIT, "Memory error.\n");
            d->cmd_buf = ptr;

            ssize_t n = mcecmd_read(d->context, d->cmd_buf + d->cmd_len,
                    *count - d->cmd_len);

            if (n < 0) {
                lprintf(LOG_ERR, "Read error from command device %i\n", 
                        con[c].dev_idx);
                
                /* abort */
                confd[p].rsp[0] = MCENETD_STOP;
                confd[p].rsp[1] = 7;
                confd[p].rsp[2] = MCENETD_ERR_READ;
                int32_t *e = (int32_t*)(confd[p].rsp + 3);
                *e = (int32_t)errno;
                confd[p].rsplen = 7;
                pfd[p].events = POLLOUT;
                return;
            }

            d->cmd_len += n;
        }

        if (*count <= d->cmd_len)
            abort();
    } else {
        lprintf(LOG_CRIT,
                "Unknown message type on command connection: %02hhX\n",
                message[0]);
    }
}

/* process a request on the control channel */
void CtlRead(int p, int c)
{
    ssize_t n;
    unsigned char message[256];
    struct dev_struct *d = dev + con[c].dev_idx;

    /* read the requested operation */
    n = NetRead(pfd[p].fd, c, message, "CTL", con[c].ser);

    if (n < 0)
        return;

    lprintf(LOG_DEBUG, "CTL@CLx%02hhX: f:%x ==> %s\n", con[c].ser, con[c].flags,
            SpitMessage(message, n));

    if (confd[p].type & FDTYPE_HX) {
        /* trying to verify connection, expecting HELLO */
        Hello(p, c, message, n);
        confd[p].type = FDTYPE_CTL;
    } else if (message[0] == MCENETD_CMDIOCTL) {
        /* ioctl time */
        uint32_t *req = (uint32_t*)(message + 1);
        int32_t *arg = (int32_t*)(message + 5);
        int ret = mcecmd_ioctl(d->context, *req, *arg);

        /* send the result */
        confd[p].rsp[0] = MCENETD_IOCTLRET;
        confd[p].rsp[1] = 6;
        arg = (int32_t*)(confd[p].rsp + 2);
        *arg = (int32_t)ret;
        confd[p].rsplen = 6;
        pfd[p].events = POLLOUT;
    } else {
        lprintf(LOG_CRIT, "Unhandled communication (%x): CTL#%x\n", message[0],
                con->flags);
    }
}

/* accept a new connection on one of the device ports */
void AcceptDevice(int p)
{
    int fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    
    /* provisionally accept the connection */
    fd = accept(pfd[p].fd, (struct sockaddr*)&addr, &addrlen);

    lprintf(LOG_DEBUG, "connect on %s port from %s assigned %i\n", DEVID(p),
            inet_ntoa(addr.sin_addr), (int)npfd);

    AddSocket(fd, POLLIN, p, ncon, addr);
}

/* finalise the handshaking for one of the device ports */
void HandshakeDevice(int p)
{
    ssize_t n;
    unsigned char message[256];

    /* read the identification message */
    n = mcenet_readmsg(pfd[p].fd, message, 4);

    lprintf(LOG_DEBUG, "dev#%i ==> %s\n", p, SpitMessage(message, n));

    if (n < 4 || message[1] != MCENETD_MAGIC1
            || message[2] != MCENETD_MAGIC2 || message[3] != MCENETD_MAGIC3
            || !ser[message[0]])
    {
        /* drop the connection */
        lprintf(LOG_INFO, "Dropping spurious %s connection from %s.\n",
                DEVID(p), inet_ntoa(confd[p].addr.sin_addr));
        DropSocket(p);
    } else {
        int c = ser[message[0]] - 1;
        lprintf(LOG_INFO, "CLx%02hhX connected to %s port.\n", c, DEVID(p));
        confd[p].type -= FDTYPE_HX;
        confd[p].con = c;
        confd[p].buf = NULL;
        confd[p].buflen = 0;
        con[c].sock[confd[p].type] = p;

        /* send confirmation */
        confd[p].rsp[0] = con[c].ser;
        confd[p].rsp[1] = 5;
        confd[p].rsp[2] = MCENETD_MAGIC1;
        confd[p].rsp[3] = MCENETD_MAGIC2;
        confd[p].rsp[4] = MCENETD_MAGIC3;
        confd[p].rsplen = 5;
        pfd[p].events = POLLOUT;
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
    con[ncon].sock[0] = AddSocket(fd, POLLIN, FDTYPE_CTL, ncon, con[ncon].addr);
    con[ncon].sock[1] = con[ncon].sock[2] = con[ncon].sock[3] = -1;

    /* initialise */
    con[ncon].flags = 0;
    con[ncon].nerr = 0;
    con[ncon].ser = serind;
    ser[serind++] = ncon + 1;

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
        else if (n == 0)
            lprintf(LOG_CRIT, "poll timed out!\n");

        if (pfd[0].revents & POLLIN)
            NewClient();

        /* check for errors */
        for (i = 0; i < npfd; ++i) {
            if (pfd[i].revents & POLLERR)
                lprintf(LOG_WARNING, "Error detected on socket %i\n", i);
        }

        /* look for new connections on the device ports */
        for (i = 1; i <= 3; ++i)
            if (pfd[i].revents & POLLIN)
                AcceptDevice(i);

        for (i = 4; i < npfd; ++i) {
            /* skip connections about to be closed */
            if (!(confd[i].type & FDTYPE_HX) &&
                    con[confd[i].con].flags & CON_DROP)
            {
                continue;
            }

            if (pfd[i].revents & POLLHUP) {
                lprintf(LOG_INFO, "%s connection closed by CLx%02hhX.\n",
                        DEVID(i), con[confd[i].con].ser);
                con[confd[i].con].flags = CON_DROP;
                continue;
            }

            if (pfd[i].revents & POLLOUT)
                RspWrite(i, confd[i].con);

            if (pfd[i].revents & POLLIN) {
                switch (confd[i].type) {
                    case FDTYPE_CTL:
                    case FDTYPE_CTL | FDTYPE_HX:
                        CtlRead(i, confd[i].con);
                        break;
                    case FDTYPE_DSP | FDTYPE_HX:
                    case FDTYPE_CMD | FDTYPE_HX:
                    case FDTYPE_DAT | FDTYPE_HX:
                        HandshakeDevice(i);
                        break;
                    case FDTYPE_DSP:
                        lprintf(LOG_CRIT, "can't handle read on dsp#%i\n", i);
                    case FDTYPE_CMD:
                        CmdRead(i, confd[i].con);
                        break;
                    case FDTYPE_DAT:
                        lprintf(LOG_CRIT, "can't handle read on dat#%i\n", i);
                    default:
                        lprintf(LOG_CRIT,
                                "unhandled read type: %i\n", confd[i].type);
                }
            }
        }

        /* process pending closes */
        for (i = 0; i < ncon; ++i)
            if (con[i].flags & CON_DROP)
                DropConnection(i);
    }
}
