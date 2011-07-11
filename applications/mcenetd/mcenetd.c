/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <errno.h>
#include <pthread.h>
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

/* number of consecutive request errors we're willing to tolerate. */
#define MAX_ERR 3

#define DEVID(p) ( ((confd[p].type & 3) == FDTYPE_DAT) ? "DAT" : \
        ((confd[p].type & 3) == FDTYPE_CMD) ? "CMD" : \
        ((confd[p].type & 3) == FDTYPE_DSP) ? "DSP" : "CTL")

/* ITC */
pthread_t *tid;
int itc_ser = 1;

#define ITC_ATTACH   1
#define ITC_CMD      2
#define ITC_CMDIOCTL 3
static struct itc_t {
    int dev;
    int ser;

    int req;
    size_t req_dlen;
    void *req_data;

    int res;
    size_t res_dlen;
    void *res_data;

    struct itc_t *next;
} **itc;

/* pointer assigment is probably going to be atomic on the computer this
 * program is running on, but... whatever; mutices */
static pthread_mutex_t *mutex;

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
unsigned char con_ser = 0;
struct con_struct {
    struct sockaddr_in addr;

    int sock[4];

    struct itc_t *pending;
    int dev_idx;
    int udepth;
    unsigned char ser;

    char cmd_buf[sizeof(mce_command)];
    size_t cmd_len;

    int flags;
    int nerr;
} *con = NULL;
int ncon = 0;
int con_s = 0;

/* mce stuff */
struct dev_t {
    mce_context_t context;
    const char *url;
    unsigned char ddepth;
    unsigned char endpt;

    unsigned char flags;

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
    "--", /* LOG_INFO    */
    ".."  /* LOG_DEBUG   */
};

#define lprintf(l,f,...) do { \
    if (options.daemonise) syslog(l, f, ##__VA_ARGS__); \
    else printf("[%s] " f, marker[l], ##__VA_ARGS__); \
    if (l == LOG_CRIT) exit(1); \
} while (0)

#if 0
static char __thread debug_col[1000];
static char __thread debug_col2[1000];
static int __thread col_count;
static const char* colnil(void) {
    return debug_col;
}

static const char* coladd(void) {
    if (col_count < 999) {
        debug_col[col_count++] = ':';
        debug_col[col_count] = '\0';
    }

    return debug_col;
}

static const char* colsub(void) {
    strcpy(debug_col2, debug_col);

    if (col_count > 0)
        debug_col[--col_count] = '\0';

    return debug_col2;
}
#define dtrace(f,...) lprintf(LOG_DEBUG, "%s %s(" f ")\n", coladd(), \
        __func__, ##__VA_ARGS__)
#define dreturn(f,v) lprintf(LOG_DEBUG, "%s %s = " f "\n", colsub(), \
        __func__, v)
#define dreturnvoid() lprintf(LOG_DEBUG, "%s %s = (nil)\n", colsub(), __func__)
#else
#define dtrace(...)
#define dreturn(...)
#define dreturnvoid()
#endif

static __thread char spit_buffer[1024];
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
    dtrace("%i, %i, %i, %i, %p", fd, events, type, c, &addr);
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
    pfd[npfd].revents = 0;

    dreturn("%i", (int)npfd);
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
    lprintf(LOG_INFO, "Dropping CLx%02hhX (%s).\n", con[c].ser,
            inet_ntoa(con[c].addr.sin_addr));
    ser[con[c].ser] = 0;
    DropSocket(con[c].sock[3]);
    DropSocket(con[c].sock[2]);
    DropSocket(con[c].sock[1]);
    DropSocket(con[c].sock[0]);

    con[c] = con[--ncon];
    ser[con[c].ser] = ncon + 1;
}

/* delete an ITC */
static void ITDiscard(struct itc_t *it)
{
    const int who = it->dev;
    struct itc_t **ptr, *tmp = NULL;

    pthread_mutex_lock(mutex + who);

    /* find it */
    for (ptr = itc + who; *ptr != NULL; ptr = &((*ptr)->next)) {
        if (*ptr == it) {
            tmp = (*ptr);
            *ptr = (*ptr)->next;
            break;
        }
    }

    pthread_mutex_unlock(mutex + who);

    /* delete it */
    if (tmp) {
        free(tmp->req_data);
        free(tmp->res_data);
        free(tmp);
    } else
        lprintf(LOG_CRIT, "Failed to find ITC %p\n", it);
}

/* return a device interthread response */
static void ITRes(int me, int ser, int res, void *data, size_t dlen)
{
    struct itc_t *ptr = itc[me];

    pthread_mutex_lock(mutex + me);

    /* find the request we're responding to */
    do {
        if (ptr->ser == ser)
            break;
        ptr = ptr->next;
    } while (ptr != NULL);

    if (ptr == NULL) {
        lprintf(LOG_ERR, "Lost ITC request #%i with pending response!\n", ser);
        pthread_mutex_unlock(mutex + me);
        return;
    }

    ptr->res = res;
    if (data == NULL || dlen == 0) {
        ptr->res_data = NULL;
        ptr->res_dlen = 0;
    } else {
        ptr->res_data = malloc(dlen);
        if (ptr->res_data == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");
        memcpy(ptr->res_data, data, dlen);
        ptr->res_dlen = dlen;
    }

    pthread_mutex_unlock(mutex + me);

    lprintf(LOG_DEBUG, "ITC#%i: %i <- %i: %s\n", ptr->ser, ptr->res, me,
            SpitMessage(ptr->res_data, ptr->res_dlen));
}

/* set up a device interthread request */
static struct itc_t *ITReq(int who, int req, void *data, size_t dlen)
{
    struct itc_t **ptr = itc + who;

    /* compose the itc */
    struct itc_t *next = (struct itc_t *)malloc(sizeof(struct itc_t));

    if (next == NULL)
        lprintf(LOG_CRIT, "Memory error.\n");

    next->dev = who;
    next->ser = itc_ser++;
    next->req = req;

    if (data == NULL || dlen == 0) {
        next->req_data = NULL;
        next->req_dlen = 0;
    } else {
        next->req_data = malloc(dlen);
        if (next->req_data == NULL) 
            lprintf(LOG_CRIT, "Memory error.\n");
        memcpy(next->req_data, data, dlen);
        next->req_dlen = dlen;
    }
    next->res = 0;
    next->next = NULL;

    /* find the end of the pending request queue */
    while (*ptr != NULL)
        ptr = &((*ptr)->next);

    pthread_mutex_lock(mutex + who);

    /* store the request */
    *ptr = next;

    pthread_mutex_unlock(mutex + who);

    lprintf(LOG_DEBUG, "ITC#%i: %i -> %i: %s\n", next->ser, next->req, who,
            SpitMessage(next->req_data, next->req_dlen));

    return next;
}

/* listen on port */
static int Listen(int port)
{
    int sock, n;
    struct sockaddr_in addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        char errbuf[2048];
        lprintf(LOG_ERR, "Unable to create socket: %s\n",
                strerror_r(errno, errbuf, 2048));
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
        char errbuf[2048];
        lprintf(LOG_ERR, "Unable to bind to port: %s\n",
                strerror_r(errno, errbuf, 2048));
        return -1;
    }

    /* listen */
    if (listen(sock, 100) == -1) {
        char errbuf[2048];
        lprintf(LOG_ERR, "Unable to listen on port: %s\n",
                strerror_r(errno, errbuf, 2048));
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

static void MCEInit(int i)
{
    dev[i].flags = dev[i].dat_len = 0;
    dev[i].dat_buf = NULL;

    /* get a context, if this fails, we disable the card completely */
    if ((dev[i].context = mcelib_create(i, options.mas_file, 0)) == NULL)
        lprintf(LOG_ERR, "Unable to get MCE context #%i.\n", i);
    else {
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

/* (DevThread) Respond to an ITC request */
void DevRes(int me, struct itc_t *req)
{
    int32_t ret;
    char buffer[5];
    mce_reply rep;

    switch(req->req) {
        case ITC_ATTACH:
            if ((dev[me].flags & (MCENETD_F_MAS_OK | MCENETD_F_DSP_OK |
                            MCENETD_F_CMD_OK | MCENETD_F_DAT_OK)) !=
                    (MCENETD_F_MAS_OK | MCENETD_F_DSP_OK | MCENETD_F_CMD_OK |
                     MCENETD_F_DAT_OK))
            {
                MCEInit(me);
            }
            buffer[0] = dev[me].ddepth;
            buffer[1] = dev[me].endpt;
            buffer[2] = dev[me].flags;
            ITRes(me, req->ser, ITC_ATTACH, buffer, 3);
            break;
        case ITC_CMD:
            /* send a command ... */
            fprintf(stderr, "send...\n");
            ret = (int32_t)mcecmd_send_command_now(dev[me].context,
                    (mce_command*)req->req_data);
            fprintf(stderr, "ret = %i\n", ret);
            if (ret < 0) {
                /* send error! */
                buffer[0] = 1;
                *(int32_t*)(buffer + 1) = ret;
                ITRes(me, req->ser, ITC_CMD, buffer, 5);
            } else if ((ret = (int32_t)mcecmd_read_reply_now(dev[me].context,
                            &rep)) < 0)
            {
                /* reply error! */
                buffer[0] = 2;
                *(int32_t*)(buffer + 1) = ret;
                ITRes(me, req->ser, ITC_CMD, buffer, 5);
            } else {
                /* good response, send it back to the master */
                ITRes(me, req->ser, ITC_CMD, &rep, sizeof(mce_reply));
            }
        case ITC_CMDIOCTL:
            /* ioctl time */
            ret = (int32_t)mcecmd_ioctl(dev[me].context,
                    *(uint32_t*)req->req_data, *(int32_t*)(req->req_data + 4));
            /* send the result back to the command thread */
            memcpy(buffer, &ret, 4);
            ITRes(me, req->ser, ITC_CMDIOCTL, buffer, 4);
            break;
        default:
            lprintf(LOG_CRIT, "Unknown ITC request: %i\n", req->req);
    }
}

/* this is the start routine for the device threads; the argument is a
 * pseudopointer cryptinteger. */
void *RunDev(void *arg)
{
    int need_unlock;
    const int me = (int)arg;
    struct itc_t *ptr;
    struct itc_t req;

    /* main loop: wait for an ITC request */
    for (;;) {
        need_unlock = 1;
        pthread_mutex_lock(mutex + me);

        /* find the first unhandled ITC and respond to it; we have to start
         * from the root of the list every time, if we unlock in the middle.
         */
        for (ptr = itc[me]; ptr != NULL; ptr = ptr->next) {
            if (ptr->res == 0) {
                /* make a local copy */
                memcpy(&req, ptr, sizeof(struct itc_t));
                pthread_mutex_unlock(mutex + me);

                /* handle it */
                DevRes(me, &req);

                /* bail */
                need_unlock = 0;
                break;
            }
        }

        if (need_unlock)
            pthread_mutex_unlock(mutex + me);

        usleep(1000);
    }

    return NULL;
}

/* Returns true if the IT response was successfully "handled". */
static int HandleITRes(struct itc_t *itc, int c, int d)
{
    const int ctlsock = con[c].sock[0];

    switch(itc->res) {
        case ITC_ATTACH:
            confd[ctlsock].rsp[0] = MCENETD_READY;
            confd[ctlsock].rsp[1] = MCENETD_PROTO_VERSION;
            confd[ctlsock].rsp[2] = con[c].ser;
            memcpy(confd[ctlsock].rsp + 3, itc->res_data, 3);
            confd[ctlsock].rsplen = 6;
            pfd[ctlsock].events = POLLOUT;
            con[c].pending = NULL;
            break;
        case ITC_CMDIOCTL:
            /* send the result */
            confd[ctlsock].rsp[0] = MCENETD_IOCTLRET;
            confd[ctlsock].rsp[1] = 6;
            memcpy(confd[ctlsock].rsp + 2, itc->res_data, 4);
            confd[ctlsock].rsplen = 6;
            pfd[ctlsock].events = POLLOUT;
            con[c].pending = NULL;
            break;
        default:
            lprintf(LOG_CRIT, "Unknown ITC response: %i\n", itc->res);
    }

    pthread_mutex_unlock(mutex + d);

    return 1;
}

static void InitDevs(void)
{
    int i;

    /* grab a context for a null device */
    mce_context_t context = mcelib_create(MCE_NULL_MCE, options.mas_file, 0);

    /* record ndev, allocate and initialise */
    ndev = mcelib_ndev(context);
    dev = malloc(ndev * sizeof(struct dev_t));
    itc = malloc(ndev * sizeof(struct itc_t*));
    tid = malloc(ndev * sizeof(pthread_t));
    mutex = malloc(ndev * sizeof(pthread_mutex_t));

    if (dev == NULL || itc == NULL || tid == NULL)
        lprintf(LOG_CRIT, "Memory allocation error.\n");

    memset(dev, 0, ndev * sizeof(struct dev_t));
    memset(itc, 0, ndev * sizeof(struct itc_t*));

    /* destroy the context */
    mcelib_destroy(context);

    /* creatify threads */
    for (i = 0; i < ndev; ++i) {
        if (pthread_mutex_init(mutex + i, NULL))
            lprintf(LOG_CRIT, "Threading error.\n");
        if (pthread_create(tid + i, NULL, RunDev, (void*)i))
            lprintf(LOG_CRIT, "Out of resources.\n");
    }

}

/* verify a hello message.  On error, drop the connection */
void Hello(int p, int c, unsigned char *message, ssize_t n)
{
    const struct dev_t *d;

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
        pfd[p].events = POLLOUT;
    } else if (con[c].udepth + d->ddepth > MAX_DEPTH) {
        /* too much recurstion */
        confd[p].rsp[0] = MCENETD_STOP;
        confd[p].rsp[1] = MCENETD_ERR_BALROG;
        confd[p].rsplen = 2;
        pfd[p].events = POLLOUT;
    } else {
        /* we're good to go: signal the device thread to check it's context,
         * and then asyncrhonously wait for the response */
        con[c].pending = ITReq(con[c].dev_idx, ITC_ATTACH, NULL, 0);
    }
}

/* send a response on the control channel */
void RspWrite(int p, int c)
{
    ssize_t n;
    dtrace("%i, %i", p, c);

    lprintf(LOG_DEBUG, "%s@CLx%02hhX: f:%x <== %s\n", DEVID(p), con[c].ser,
            con[c].flags, SpitMessage(confd[p].rsp, confd[p].rsplen));

    n = write(pfd[p].fd, confd[p].rsp, confd[p].rsplen);

    if (n < 0) {
        char errbuf[2048];
        lprintf(LOG_ERR, "write error: %s\n", strerror_r(errno, errbuf, 2048));
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

    dreturnvoid();
}

ssize_t NetRead(int d, int c, unsigned char *buf, const char *where,
        unsigned int who)
{
    ssize_t n;

    n = mcenet_readmsg(d, buf, -1);

    if (n < 0) {
        char errbuf[2048];
        lprintf(LOG_ERR, "read error from CLx%02hhX: %s\n", who,
                strerror_r(errno, errbuf, 2048));
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
    /* Read a command from the client... basically we're trying to fill up
     * cmd_buf with a full mce command.  Once it's full, it's time to send it
     * off downstream */

    if (con[c].cmd_len < sizeof(mce_command)) {
        size_t n = read(pfd[p].fd, con[c].cmd_buf + con[c].cmd_len,
                sizeof(mce_command) - con[c].cmd_len);

        if (n < 0) {
            char errbuf[2048];
            lprintf(LOG_ERR, "error reading from CMD port: %s\n", 
                strerror_r(errno, errbuf, 2048));
            return;
        } else
            con[c].cmd_len += n;
    }

    /* check for a full command */
    if (con[c].cmd_len == sizeof(mce_command)) {
        /* send an ITC signalling the availability of a command */
        con[c].pending = ITReq(con[c].dev_idx, ITC_CMD, con[c].cmd_buf,
                sizeof(mce_command));
        /* and then forget about the command completely */
        con[c].cmd_len = 0;
    }
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

    lprintf(LOG_DEBUG, "CTL@CLx%02hhX: f:%x ==> %s\n", con[c].ser, con[c].flags,
            SpitMessage(message, n));

    if (con[c].pending != NULL) {
        /* if there's a pending operation to the device, we don't let more
         * build up */
        confd[p].rsp[0] = MCENETD_STOP;
        confd[p].rsp[1] = MCENETD_ERR_PENDING;
        confd[p].rsplen = 2;
        pfd[p].events = POLLOUT;
    } else if (confd[p].type & FDTYPE_HX) {
        /* trying to verify connection, expecting HELLO */
        Hello(p, c, message, n);
        confd[p].type = FDTYPE_CTL;
    } else if (message[0] == MCENETD_CMDIOCTL) {
        /* Send the IOCTL to the dev thread */
        char buffer[8];

        /* copy ioctl reqeust # and argument and send it to the dev thread */
        memcpy(buffer, message + 1, 8);
        con[c].pending = ITReq(con[c].dev_idx, ITC_CMDIOCTL, buffer, 8);
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

    dtrace("%i", p);
    socklen_t addrlen = sizeof(struct sockaddr_in);

    /* provisionally accept the connection */
    fd = accept(pfd[p].fd, (struct sockaddr*)&addr, &addrlen);

    lprintf(LOG_DEBUG, "connect on %s port from %s assigned %i\n", DEVID(p),
            inet_ntoa(addr.sin_addr), (int)npfd);

    AddSocket(fd, POLLIN, p, -1, addr);

    dreturnvoid();
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
    int old_con_ser = con_ser;
    while (ser[con_ser]) {
        con_ser = (con_ser + 1) % 256;
        if (old_con_ser == con_ser) {
            /* ran out of serial numbers; drop the connection */
            lprintf(LOG_WARNING, "no available connection!\n");
            shutdown(fd, SHUT_RDWR);
            return;
        }
    }

    /* add it to the poll list */
    memset(con + ncon, 0, sizeof(struct con_struct));
    con[ncon].sock[0] = AddSocket(fd, POLLIN, FDTYPE_CTL, ncon, con[ncon].addr);
    con[ncon].sock[1] = con[ncon].sock[2] = con[ncon].sock[3] = -1;

    /* initialise */
    con[ncon].ser = con_ser;
    ser[con_ser++] = ncon + 1;

    ncon++;

    /* whine */
    lprintf(LOG_INFO, "accepted control connection from %s as CLx%02hhX.\n",
            inet_ntoa(con[ncon - 1].addr.sin_addr), con_ser - 1);
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

        /* fork to background and record daemon's PID */
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
            _exit(0);
        }

        /* Daemonise */
        pid = chdir("/");
        stream = freopen("/dev/null", "r", stdin);
        stream = freopen("/dev/null", "w", stdout);
        stream = freopen("/dev/null", "w", stderr);
        setsid();
    }

    memset(ser, 0, 256 * sizeof(int));

    lprintf(LOG_INFO, "mcenetd proto_vers#%i (C) 2011 D. V. Wiebe\n",
            MCENETD_PROTO_VERSION);

    /* initialise and spawn threads */
    InitDevs();

    /* initialise the network interface */
    NetInit();

    /* poll loop */
    for (;;) {
        /* check for ITC responses */
        for (i = 0; i < ncon; ++i)
            if (con[i].pending != NULL) {
                struct itc_t *pending = con[i].pending;
                pthread_mutex_t *mu = mutex + pending->dev;
                pthread_mutex_lock(mu);

                if (pending->res != 0) {
                    if (HandleITRes(pending, i, pending->dev))
                        ITDiscard(pending);
                } else
                    pthread_mutex_unlock(mu);
            }

        /* check for pending client communication */
        int n = poll(pfd, npfd, 10);

        if (n < 0) {
            char errbuf[2048];
            lprintf(LOG_CRIT, "poll error: %s\n",
                    strerror_r(errno, errbuf, 2048));
        } else if (n > 0) {
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

                if (pfd[i].revents & POLLOUT) {
                    RspWrite(i, confd[i].con);
                }

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
                            lprintf(LOG_CRIT, "can't handle read on dsp#%i\n",
                                    i);
                        case FDTYPE_CMD:
                            CmdRead(i, confd[i].con);
                            break;
                        case FDTYPE_DAT:
                            lprintf(LOG_CRIT, "can't handle read on dat#%i\n",
                                    i);
                        default:
                            lprintf(LOG_CRIT, "unhandled read type: %i\n",
                                    confd[i].type);
                    }
                }
            }
        }

        /* process pending closes */
        for (i = 0; i < ncon; ++i)
            if (con[i].flags & CON_DROP)
                DropConnection(i);
    }
}
