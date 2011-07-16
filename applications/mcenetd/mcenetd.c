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
#include <mce/data_ioctl.h>

#include "../../defaults/config.h"

/* maximum chain length */
#define MAX_DEPTH 10

/* number of consecutive request errors we're willing to tolerate. */
#define MAX_ERR 3

/* the size of the data buffer; going over a pagesize (64k) can result in a
 * performance hit */
#define DATBLOB_SIZE 65000

#define DEVID(p) ( ((confd[p].type & 3) == FDTYPE_DAT) ? "DAT" : \
        ((confd[p].type & 3) == FDTYPE_CMD) ? "CMD" : \
        ((confd[p].type & 3) == FDTYPE_DSP) ? "DSP" : "CTL")

/* ITC */
pthread_t *tid;
int itc_ser = 1;

#define ITC_ATTACH   1
#define ITC_CMDIOCTL 2
#define ITC_CMDSEND  3
#define ITC_CMDREPL  4
#define ITC_DSPIOCTL 5
#define ITC_DSPSEND  6
#define ITC_DSPREPL  7
#define ITC_DATIOCTL 8
#define ITC_DATAQ    9
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
#define CON_CMDSNT 0x02
#define CON_CMDREP 0x04
#define CON_DSPSNT 0x08
#define CON_DSPREP 0x10
int ser[256];
unsigned char con_ser = 0;
struct con_struct {
    struct sockaddr_in addr;

    int sock[4];

    struct itc_t *pending;
    int dev_idx;
    int udepth;
    unsigned char ser;

    char cmd_req[sizeof(mce_command)];
    char cmd_rep[sizeof(mce_reply)];
    int32_t cmd_sent;
    int32_t cmd_replied;

    char dsp_req[sizeof(dsp_command)];
    char dsp_rep[sizeof(dsp_message)];
    int32_t dsp_sent;
    int32_t dsp_replied;

    unsigned char *dat_buf;
    uint32_t dat_len; /* how much data we have */
    uint32_t dat_req; /* how much data we need to send to the client */

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
    size_t dat_len; /* how much data we have */
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

#if 1
static char __thread debug_col[1000];
static char __thread debug_col2[1000];
static int __thread col_count;

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

static __thread char spit_buffer[3010];
static const char *SpitMessage(const void *m, ssize_t n)
{
    ssize_t i;
    char *ptr = spit_buffer;
    for (i = 0; i < n; ++i) {
        sprintf(ptr, "%02hhx ", ((unsigned char*)m)[i]);
        ptr += 3;
        if (i == 999) {
            sprintf(ptr, "... [%i]", n - 1000);
            ptr += 3;
            break;
        }
    }

    return spit_buffer;
}

static int AddSocket(int fd, short events, int type, int c,
        struct sockaddr_in addr)
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

static void DropSocket(int p)
{
    if (p >= 0) {
        npfd--;
        shutdown(pfd[p].fd, SHUT_RDWR);
        confd[p] = confd[npfd];
        pfd[p] = pfd[npfd];
    }
}

static void DropConnection(int c) {
    lprintf(LOG_INFO, "Dropping CLx%02hhX.\n", con[c].ser);
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
    dtrace("%p", it);

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

    dreturnvoid();
}

/* return a device interthread response */
static void ITRes(int me, int ser, int res, void *data, size_t dlen)
{
    dtrace("%i, %i, %i, %p, %zi", me, ser, res, data, dlen);

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
        dreturnvoid();
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

    dreturnvoid();
}

/* set up a device interthread request */
static struct itc_t *ITReq(int who, int req, void *data, size_t dlen,
        int nolock)
{
    dtrace("%i, %i, %p, %zi, %i", who, req, data, dlen, nolock);
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
    while (*ptr != NULL) {
        ptr = &((*ptr)->next);
    }

    if (!nolock)
        pthread_mutex_lock(mutex + who);

    /* store the request */
    *ptr = next;

    if (!nolock)
        pthread_mutex_unlock(mutex + who);

    lprintf(LOG_DEBUG, "ITC#%i: %i -> %i: %s\n", next->ser, next->req, who,
            SpitMessage(next->req_data, next->req_dlen));

    dreturn("%p", next);
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
    dev[i].flags = dev[i].dat_len = dev[i].dat_len = 0;
    if (dev[i].dat_buf == NULL) {
        dev[i].dat_buf = malloc(DATBLOB_SIZE);
        if (dev[i].dat_buf == NULL)
            lprintf(LOG_CRIT, "Memory error.\n");
    }

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
static void DevRes(int me, struct itc_t *req)
{
    dtrace("%i, %p [%i]", me, req, req->req);

    int32_t ret;
    ssize_t dat_req;
    char buffer[5];
    mce_reply cmd_rep;
    dsp_message dsp_msg;

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
        case ITC_CMDIOCTL:
            /* ioctl time */
            ret = (int32_t)mcecmd_ioctl(dev[me].context,
                    *(uint32_t*)req->req_data, *(int32_t*)(req->req_data + 4));
            /* send the result back to the command thread */
            memcpy(buffer, &ret, 4);
            ITRes(me, req->ser, ITC_CMDIOCTL, buffer, 4);
            break;
        case ITC_CMDSEND:
            /* send a command ... */
            ret = (int32_t)mcecmd_send_command_now(dev[me].context,
                    (mce_command*)req->req_data);

            /* pass the return value up */
            ITRes(me, req->ser, ITC_CMDSEND, &ret, 4);
            break;
        case ITC_CMDREPL:
            ret = (int32_t)mcecmd_read_reply_now(dev[me].context, &cmd_rep);
            if (ret < 0) {
                /* reply error! */
                ITRes(me, req->ser, ITC_CMDREPL, &ret, 4);
            } else
                /* good response, send it back to the master */
                ITRes(me, req->ser, ITC_CMDREPL, &cmd_rep, sizeof(mce_reply));
            break;
        case ITC_DSPIOCTL:
            /* ioctl time */
            ret = (int32_t)mcedsp_ioctl(dev[me].context,
                    *(uint32_t*)req->req_data, *(int32_t*)(req->req_data + 4));
            /* send the result back to the control thread */
            memcpy(buffer, &ret, 4);
            ITRes(me, req->ser, ITC_DSPIOCTL, buffer, 4);
            break;
        case ITC_DSPSEND:
            /* send a command ... */
            ret = (int32_t)mcedsp_write(dev[me].context,
                    (dsp_command*)req->req_data);

            /* pass the return value up */
            ITRes(me, req->ser, ITC_DSPSEND, &ret, 4);
            break;
        case ITC_DSPREPL:
            ret = (int32_t)mcedsp_read(dev[me].context, &dsp_msg);
            if (ret < 0) {
                /* reply error! */
                ITRes(me, req->ser, ITC_DSPREPL, &ret, 4);
            } else
                /* good response, send it back to the master */
                ITRes(me, req->ser, ITC_DSPREPL, &dsp_msg, sizeof(dsp_msg));
            break;
        case ITC_DATIOCTL:
            /* ioctl time */
            ret = (int32_t)mcedata_ioctl(dev[me].context,
                    *(uint32_t*)req->req_data, *(int32_t*)(req->req_data + 4));

            /* send the result back to the control thread */
            memcpy(buffer, &ret, 4);
            ITRes(me, req->ser, ITC_DATIOCTL, buffer, 4);
            break;
        case ITC_DATAQ:
            dat_req = *(uint32_t*)req->req_data;

            if (dev[me].dat_len < DATBLOB_SIZE) {
                /* try to read as much data as possible */
                ssize_t n = (int32_t)mcedata_read(dev[me].context,
                        dev[me].dat_buf + dev[me].dat_len,
                        DATBLOB_SIZE - dev[me].dat_len);
                if (n < 0) {
                    char errbuf[2048];
                    lprintf(LOG_ERR, "Read error on DAT, %s\n",
                            strerror_r(errno, errbuf, 2048));
                } else
                    dev[me].dat_len += n;
            }

            /* now send back to the controller as much of the request as
             * we can; if we're short, the controller will send us another
             * request */
            if (dat_req > dev[me].dat_len)
                dat_req = dev[me].dat_len;
            ITRes(me, req->ser, ITC_DATAQ, dev[me].dat_buf, dat_req);
            
            /* and then flush the data we sent from the buffer */
            memmove(dev[me].dat_buf, dev[me].dat_buf + dat_req,
                    dev[me].dat_len - dat_req);
            dev[me].dat_len -= dat_req;
            break;
        default:
            lprintf(LOG_CRIT, "Unknown ITC request: %i\n", req->req);
    }

    dreturnvoid();
}

/* this is the start routine for the device threads; the argument is a
 * pseudopointer cryptinteger. */
static void *RunDev(void *arg)
{
    const int me = (int)arg;

    dtrace("%i", me);

    memset(dev + me, 0, sizeof(struct dev_t));

    int need_unlock;
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

    dreturn("%p", NULL);
    return NULL;
}

/* Returns true if the IT response was successfully "handled". */
static int HandleITRes(struct itc_t *itc, int c, int d)
{
    dtrace("%p, %i, %i", itc, c, d);

    const int ctlsock = con[c].sock[FDTYPE_CTL];

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
        case ITC_DSPIOCTL:
        case ITC_DATIOCTL:
            /* send the result */
            confd[ctlsock].rsp[0] = MCENETD_IOCTLRET;
            confd[ctlsock].rsp[1] = 6;
            memcpy(confd[ctlsock].rsp + 2, itc->res_data, 4);
            confd[ctlsock].rsplen = 6;
            pfd[ctlsock].events = POLLOUT;
            con[c].pending = NULL;
            break;
        case ITC_CMDSEND:
            /* report the result of the send, and immediately ask for a reply 
             * to avoid getting confused */
            con[c].cmd_sent = *(int32_t*)(itc->res_data);
            con[c].pending = ITReq(con[c].dev_idx, ITC_CMDREPL, NULL, 0, 1);
            con[c].flags |= CON_CMDSNT;
            pfd[con[c].sock[FDTYPE_CMD]].events = POLLOUT;
            break;
        case ITC_CMDREPL:
            /* store the response and signal we're ready to send it out to the
             * client */
            if (itc->res_dlen == 4) {
                /* an error code */
                con[c].cmd_replied = *(int32_t*)(itc->res_data);
            } else {
                /* no error, things went well */
                con[c].cmd_replied = 0;
                memcpy(con[c].cmd_rep, itc->res_data, itc->res_dlen);
            }
            con[c].flags |= CON_CMDREP;
            pfd[con[c].sock[FDTYPE_CMD]].events = POLLOUT;
            con[c].pending = NULL;
            break;
        case ITC_DSPSEND:
            /* report the result of the send, and immediately ask for a reply 
             * to avoid getting confused */
            con[c].dsp_sent = *(int32_t*)(itc->res_data);
            con[c].pending = ITReq(con[c].dev_idx, ITC_DSPREPL, NULL, 0, 1);
            con[c].flags |= CON_DSPSNT;
            pfd[con[c].sock[FDTYPE_DSP]].events = POLLOUT;
            break;
        case ITC_DSPREPL:
            /* store the response and signal we're ready to send it out to the
             * client */
            if (itc->res_dlen == 4) {
                /* an error code */
                con[c].dsp_replied = *(int32_t*)(itc->res_data);
            } else {
                /* no error, things went well */
                con[c].dsp_replied = 0;
                memcpy(con[c].dsp_rep, itc->res_data, itc->res_dlen);
            }
            con[c].flags |= CON_DSPREP;
            pfd[con[c].sock[FDTYPE_DSP]].events = POLLOUT;
            con[c].pending = NULL;
            break;
        case ITC_DATAQ:
            /* queue the data from the client ... */
            con[c].dat_buf = itc->res_data;
            con[c].dat_len = itc->res_dlen;
            /* to make sure ITC GC doesn't delete our data */
            itc->res_data = NULL;
            con[c].pending = NULL;

            /* indicate we're ready to ship data out */
            pfd[con[c].sock[FDTYPE_DAT]].events = POLLOUT;
            break;
        default:
            lprintf(LOG_CRIT, "Unknown ITC response: %i\n", itc->res);
    }

    pthread_mutex_unlock(mutex + d);

    dreturn("%i", 1);
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
        lprintf(LOG_CRIT, "Memory error.\n");

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
static void Hello(int p, int c, unsigned char *message, ssize_t n)
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
        con[c].pending = ITReq(con[c].dev_idx, ITC_ATTACH, NULL, 0, 0);
    }
}

/* send a response on the control channel, or do handshaking on a dev channel */
void RspWrite(int p, int c)
{
    ssize_t n;
    dtrace("%i, %i", p, c);

    n = write(pfd[p].fd, confd[p].rsp, confd[p].rsplen);

    lprintf(LOG_DEBUG, "%s@CLx%02hhX,%i: f:%x <== %s (%zi)\n", DEVID(p),
            con[c].ser, p, con[c].flags, SpitMessage(confd[p].rsp,
                confd[p].rsplen), (size_t)n);

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

    if (confd[p].type & FDTYPE_HX)
        confd[p].type -= FDTYPE_HX;

    /* done writing */
    pfd[p].events = POLLIN;

    dreturnvoid();
}

static ssize_t NetRead(int d, int c, unsigned char *buf, const char *where,
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

/* process a read on a device port */
static void DevRead(int fd, int c, int itreq, void *buf, size_t bufsize,
        const char *what)
{
    dtrace("%i, %i, %i, %p, %zi, \"%s\"", fd, c, itreq, buf, bufsize, what);
    size_t req_len = 0;
    /* Read a command from the client... basically we're trying to fill up
     * buf with a full mce command.  Once it's full, it's time to send it
     * off downstream */

    if (req_len < bufsize) {
        size_t n = read(fd, buf + req_len, bufsize - req_len);

        if (n < 0) {
            char errbuf[2048];
            lprintf(LOG_ERR, "error reading from %s port: %s\n", what,
                strerror_r(errno, errbuf, 2048));
            return;
        } else
            req_len += n;

        lprintf(LOG_DEBUG, "CMD@CLx%02hhX: ==> %s\n", con[c].ser,
                SpitMessage((const unsigned char*)buf + req_len - n, n));
    }

    /* check for a full command */
    if (req_len == bufsize) {
        /* send an ITC signalling the availability of a command */
        con[c].pending = ITReq(con[c].dev_idx, itreq, buf, bufsize, 0);
    }

    dreturnvoid();
}

/* process a write on a device port */
static void DevWrite(int p, int c, int sflag, int32_t sret, int rflag,
        int32_t rret, void *rdata, size_t rlen, const char *what)
{
    ssize_t n;

    dtrace("%i, %i, %i, %i, %i, %i, %p, %zi, \"%s\"", p, c, sflag, sret,
            rflag, rret, rdata, rlen, what);

    pfd[p].events = POLLIN;

    if (con[c].flags & sflag) {
        lprintf(LOG_DEBUG, "%s@CLx%02hhX: f:%x ==> %s\n", what, con[c].ser,
                con[c].flags, SpitMessage(&sret, 4));

        n = write(pfd[p].fd, &sret, 4);
        if (n < 0) {
            char errbuf[2048];
            lprintf(LOG_ERR, "write error: %s\n", strerror_r(errno, errbuf,
                        2048));
        } else if (n == 0) {
            /* dropped */
            con[c].flags = CON_DROP;
            dreturnvoid();
            return;
        }

        con[c].flags &= ~sflag;
    }

    if (con[c].flags & rflag) {
        lprintf(LOG_DEBUG, "%sD@CLx%02hhX: f:%x ==> %s\n", what, con[c].ser,
                con[c].flags, SpitMessage(&rret, 4));

        n = write(pfd[p].fd, &rret, 4);
        if (n < 0) {
            char errbuf[2048];
            lprintf(LOG_ERR, "write error: %s\n", strerror_r(errno, errbuf,
                        2048));
        } else if (n == 0) {
            /* dropped */
            con[c].flags = CON_DROP;
            dreturnvoid();
            return;
        }

        lprintf(LOG_DEBUG, "%s@CLx%02hhX: f:%x ==> %s\n", what, con[c].ser,
                con[c].flags, SpitMessage(rdata, rlen));

        n = write(pfd[p].fd, rdata, rlen);
        if (n < 0) {
            char errbuf[2048];
            lprintf(LOG_ERR, "write error: %s\n", strerror_r(errno, errbuf,
                        2048));
        } else if (n == 0) {
            /* dropped */
            con[c].flags = CON_DROP;
            dreturnvoid();
            return;
        }

        con[c].flags &= ~rflag;
    }

    dreturnvoid();
}

/* process a read on the command port */
static void CmdRead(int p, int c)
{
    dtrace("%i, %i", p, c);

    DevRead(pfd[p].fd, c, ITC_CMDSEND, con[c].cmd_req, sizeof(mce_command),
            DEVID(p));

    dreturnvoid();
}

/* process a read on the dsp port */
static void DspRead(int p, int c)
{
    dtrace("%i, %i", p, c);

    DevRead(pfd[p].fd, c, ITC_DSPSEND, con[c].dsp_req, sizeof(dsp_command),
            DEVID(p));

    dreturnvoid();
}

/* shove a reply all up into the command port */
static void CmdWrite(int p, int c)
{
    dtrace("%i, %i", p, c);

    DevWrite(p, c, CON_CMDSNT, con[c].cmd_sent, CON_CMDREP, con[c].cmd_replied,
            con[c].cmd_rep, sizeof(mce_reply), DEVID(p));

    dreturnvoid();
}

/* ditto for the dsp port */
static void DspWrite(int p, int c)
{
    dtrace("%i, %i", p, c);

    DevWrite(p, c, CON_DSPSNT, con[c].dsp_sent, CON_DSPREP, con[c].dsp_replied,
            con[c].dsp_rep, sizeof(dsp_message), DEVID(p));

    dreturnvoid();
}

/* The data writer is simpler, it just sends data */
static void DatWrite(int p, int c)
{
    dtrace("%i, %i", p, c);

    pfd[p].events = POLLIN;

    ssize_t n = write(pfd[p].fd, con[c].dat_buf, con[c].dat_len);

    lprintf(LOG_DEBUG, "DAT@CLx%02hhX: f:%x ==> %s\n", con[c].ser, con[c].flags,
            SpitMessage(con[c].dat_buf, con[c].dat_len));

    if (n < 0) {
        char errbuf[2048];
        lprintf(LOG_CRIT, "data write error: %s\n",
                strerror_r(errno, errbuf, 2048));
    } else if (n == con[c].dat_len) {
        /* finished with this particular fragment.  This will trigger another
         * data request from the dev thread, if there's more data to be read
         */
        con[c].dat_len = 0;
        con[c].dat_req -= n;
        free(con[c].dat_buf);
        con[c].dat_buf = NULL;
    } else {
        /* flush sent data */
        memmove(con[c].dat_buf, con[c].dat_buf + n, con[c].dat_len - n);
        con[c].dat_req -= n;
        con[c].dat_len -= n;
    }

    dreturnvoid();
}

/* process a request on the control channel */
static void CtlRead(int p, int c)
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
        /* copy ioctl reqeust # and argument and send it to the dev thread */
        con[c].pending = ITReq(con[c].dev_idx, ITC_CMDIOCTL, message + 1, 8, 0);
    } else if (message[0] == MCENETD_DSPIOCTL) {
        /* copy ioctl reqeust # and argument and send it to the dev thread */
        con[c].pending = ITReq(con[c].dev_idx, ITC_DSPIOCTL, message + 1, 8, 0);
    } else if (message[0] == MCENETD_DATIOCTL) {
        /* copy ioctl reqeust # and argument and send it to the dev thread */
        con[c].pending = ITReq(con[c].dev_idx, ITC_DATIOCTL, message + 1, 8, 0);
    } else if (message[0] == MCENETD_DATAREQ) {
        /* increment the pending data request */
        con[c].dat_req += *(uint32_t*)(message + 1);

        /* and acknowledge */
        confd[p].rsp[0] = MCENETD_DATAACK;
        confd[p].rsplen = 1;
        pfd[p].events = POLLOUT;
    } else {
        lprintf(LOG_CRIT, "Unhandled communication (%x): CTL#%x\n", message[0],
                con->flags);
    }
}

/* accept a new connection on one of the device ports */
static void AcceptDevice(int p)
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
static void HandshakeDevice(int p)
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
        confd[p].con = c;
        confd[p].buf = NULL;
        confd[p].buflen = 0;
        con[c].sock[confd[p].type & 0x3] = p;

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
static void NewClient(void)
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
    con[ncon].sock[FDTYPE_CTL] = AddSocket(fd, POLLIN, FDTYPE_CTL, ncon,
            con[ncon].addr);
    con[ncon].sock[FDTYPE_DSP] = con[ncon].sock[FDTYPE_CMD] =
        con[ncon].sock[FDTYPE_DAT] = -1;

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

    /* poll loop for the control thread */
    for (;;) {
        for (i = 0; i < ncon; ++i) {
            /* check for ITC responses */
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

            /* trigger the dev thread to send us some data, if we need some */
            if (con[i].pending == NULL && con[i].dat_req > 0 &&
                    con[i].dat_buf == NULL)
            {
                con[i].pending = ITReq(con[i].dev_idx, ITC_DATAQ,
                        &(con[i].dat_req), 4, 0);
            }
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
                    switch (confd[i].type) {
                        case FDTYPE_CTL:
                        case FDTYPE_CTL | FDTYPE_HX:
                        case FDTYPE_DSP | FDTYPE_HX:
                        case FDTYPE_CMD | FDTYPE_HX:
                        case FDTYPE_DAT | FDTYPE_HX:
                            RspWrite(i, confd[i].con);
                            break;
                        case FDTYPE_CMD:
                            CmdWrite(i, confd[i].con);
                            break;
                        case FDTYPE_DSP:
                            DspWrite(i, confd[i].con);
                            break;
                        case FDTYPE_DAT:
                            DatWrite(i, confd[i].con);
                            break;
                        default:
                            lprintf(LOG_CRIT,
                                    "Unexpected POLLOUT for fd#%i, type %x\n",
                                    pfd[i].fd, confd[i].type);
                    }
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
                        case FDTYPE_CMD:
                            CmdRead(i, confd[i].con);
                            break;
                        case FDTYPE_DSP:
                            DspRead(i, confd[i].con);
                            break;
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
