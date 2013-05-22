/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <linux/types.h>

#include "dspioctl.h"
#include "new_dsp.h"

typedef __s32 int32;

int fd = 0;

int _read_io(int cmd) {
    return ioctl (fd, cmd,0);
}

void _write_io(int cmd, int val) {
    ioctl(fd, cmd, val);
}

#define read_io(KEY) _read_io(DSPIOCT_R_ ## KEY)
#define write_io(KEY,VAL) _write_io(DSPIOCT_W_ ## KEY, VAL)


static inline int get_hrrq() { return read_io(HSTR) & 0x04; }
static inline int get_hf4()  { return read_io(HSTR) & 0x10; }


int write_cmd(struct dsp_command *cmd) {
    cmd->size = cmd->data_size + 1;
    return ioctl(fd, DSPIOCT_COMMAND, cmd);
}

int read_reply(struct dsp_datagram *reply) {
    return ioctl(fd, DSPIOCT_REPLY, reply);
}

int write_read(struct dsp_command *cmd, struct dsp_datagram *reply) {
    cmd->size = cmd->data_size + 1;
    int err = ioctl(fd, DSPIOCT_COMMAND, cmd);
    if (err != 0) {
        printf("cmd err=%i\n", err);
        return err;
    }
    err = ioctl(fd, DSPIOCT_REPLY, reply);
    if (err != 0) 
        printf("rep err=%i\n", err);
    return err;
}

int trigger_fake() {
    return ioctl(fd, DSPIOCT_TRIGGER_FAKE);
}

int dump_buf() {
    return ioctl(fd, DSPIOCT_DUMP_BUF);
}

void dump_regs() {
    printf(" HSTR: %06x\n", read_io(HSTR));
    printf(" HCVR: %06x\n", read_io(HCVR));
    printf(" HCTR: %06x\n", read_io(HCTR));
    printf("\n");
    
}

void mode_on() {
    printf("Raising HF2\n");
    int x = read_io(HCTR);
    write_io(HCTR, x | 0x20);
}

void mode_off() {
    printf("Lowering HF2\n");
    int x = read_io(HCTR);
    write_io(HCTR, x & ~0x20);
}

void int_handler(int sig) {
    if (sig == SIGINT) {
        printf("Stopping!\n");
        mode_off();
    }
    // Hand it up.
    signal(sig, SIG_DFL);
    raise(sig);
}

void exit_now(int err) {
    mode_off();
    exit(err);
}

#pragma push
#pragma pack(1)

#define DSP_STAT 10
#define MCE_DATA 11
#define MCE_REP  12

typedef struct {
    int type;
    int size;
    int32 *data;
} base_packet_t;

typedef struct {
    // Overlay for packet data.
    int32 stuff;
} dspstatus_t;
    
#pragma pop


/*
 * Packet reception
 */

void clear_hrrq() {
    while (get_hrrq()) {
        int i1 = read_io(HRXS);
        usleep(10);
    }
}


int32 read_pair() {
    /* printf( "read pair... (HTSR=%x)\n", read_io(HSTR)); */
    // Reconstruct 32 bit word from pair of hrxs reads.
    while (!get_hrrq());
    int i1 = read_io(HRXS);
    while (!get_hrrq());
    int i2 = read_io(HRXS);
    /* printf( "read pair... %i %i\n", i1, i2); */
    return (i1 & 0xffff) << 16 | (i2 & 0xffff);
}

int recv_packet(base_packet_t *packet, int uwait) {

    // Wait for packet start.
    while (!get_hrrq()) {
        usleep(10000);
        uwait -= 10000;
        if (uwait < 0)
            return -1;
    }

    // Get the first word
    
    int packet_info = read_pair();
    printf("packet_info: %x\n", packet_info);
    int ptype = (packet_info >> 16) & 0xff;
    int plen = (packet_info & 0xffff);

    // Get the rest of the words.
    packet->type = ptype;
    packet->size = plen;
    packet->data = (int32*)malloc(plen*sizeof(int32));
    printf("plen=%i\n", plen);
    for (int i=0; i<plen; i++) {
        printf("i=%i\n", i);
        packet->data[i] = read_pair();
    }

    return 0;
}

void summarize(base_packet_t *bp) {
    printf(" type = %i\n size = %i\n", bp->type, bp->size);
    if (bp->type == DSP_STAT) {
        for (int i=0; i<bp->size; i++) {
            printf("  payload %3i = %08x\n", i, bp->data[i]);
        }
    }
}

int main(void) {
    int x, y, z, i, j, k, err;
    fd = open("/dev/mce_test0", O_RDWR);
    printf("Hi. fd=%i\n\n", fd);
    if (fd<=0) 
        return -1;

    printf("On entry:\n");
	dump_regs();

    if (read_io(HCTR) & 0x20) {
        printf("We were still on; offing and exiting.\n");
        mode_off();
        dump_regs();
        return 0;
    }

    /* printf("Disabling prefetch\n"); */
    /* write_io(HCTR, read_io(HCTR) | 0x80); */

    
    if (0) {
        clear_hrrq();
        dump_regs();
        return 0;
    }

    //IOCT test...
    struct dsp_command cmd;
    struct dsp_reply *rep;

    // Test responsiveness:
    if (get_hf4()) {
        printf("HF4 is high on entry, writing one word...\n");
        write_io(HTXR, 0x0);
        return 0;
    }

    mode_on();
    usleep(100000);
    if (!get_hf4()) {
        printf("No handshake!\n");
        mode_off();
        return 0;
    }

    mode_off();
    usleep(100000);
    if (get_hf4()) {
        printf("HF4 did not drop.\n");
        return 0;
    }

    dump_regs();

/*
 * Set up commanding.
 */

    // Kill this mode on exit?
    mode_on();
    // signal(SIGINT, int_handler);

    // Cause upload of DMA destination
    printf("Setting DMA REP_BUF\n");
    err = ioctl(fd, DSPIOCT_SET_REP_BUF);
    printf(" err=%i\n", err);

    /* printf("Setting DMA DATA_BUF\n"); */
    /* err = ioctl(fd, DSPIOCT_SET_DATA_BUF); */
    /* printf(" err=%i\n", err); */

    /* mode_off(); */
    /* usleep(100000); */
    /* printf(" regs after mode-off:\n"); */
    /* dump_regs(); */

    /* exit_now(0); */


    struct dsp_datagram dgram;
    rep = DSP_REPLY(&dgram);

    /* Write / read test */
    cmd.flags = DSP_EXPECT_DSP_REPLY;
    cmd.cmd = DSP_CMD_READ_Y;
    cmd.data_size = 1;
    cmd.data[0] = 0x1000; // addr to read
    write_read(&cmd, &dgram);

    printf("  read %#x\n", rep->data[0]);
    
    cmd.cmd = DSP_CMD_WRITE_Y;
    cmd.data_size = 2;
    cmd.data[1] = rep->data[0] + 1;
    write_read(&cmd, &dgram);
    printf("  wrote %#x\n", cmd.data[1]);
    
    cmd.cmd = DSP_CMD_READ_Y;
    cmd.data_size = 1;
    write_read(&cmd, &dgram);
    printf("  read %#x\n", rep->data[0]);
    


/*
 * Send a test command
 */

    cmd.flags = DSP_EXPECT_DSP_REPLY;
    cmd.cmd = DSP_CMD_READ_X;
    cmd.data_size = 1;
    cmd.data[0] = 0; // addr to read

    // Dump X data?
    for (int j=0; j<10; j++) {
    /* for (int j=3; j<4; j++) { */
        cmd.data[0] = j;
        /* err = write_cmd(&cmd); */
        /* if (err<0) { */
        /*     printf("write_cmd err=%i\n", err); */
        /* } */
        /* err = read_reply(&dgram); */
        /* if (err<0) { */
        /*     printf("read_reply err=%i\n", err); */
        /* } */
        /* __s32 *x = (__s32*)&dgram; */
        /* /\* for (int k=0; k<16; k++) { *\/ */
        /* /\*     printf("%2i %8x\n", k, x[k]); *\/ */
        /* /\* } *\/ */
        if (write_read(&cmd, &dgram)!=0)
            break;
        rep = DSP_REPLY(&dgram);
        printf("  data %3i = %#x\n", j, rep->data[0]);
    }

    /* mode_off(); */
    /* usleep(100000); */
    /* printf(" regs after mode-off:\n"); */
    /* dump_regs(); */

    /* exit_now(0); */


    if (1) {

        printf("\nMCE command?\n");
        cmd.flags = DSP_EXPECT_DSP_REPLY;
//        cmd.timeout_us = 1000000;
        cmd.cmd = DSP_SEND_MCE;
        cmd.data_size = 64;
        int OFS=0;
        // wb cc led 7 = 20205742 00020099 00000001 00000007
        cmd.data[OFS+0] = 0xa5a5a5a5;
        cmd.data[OFS+1] = 0x5a5a5a5a;
        cmd.data[OFS+2] = 0x20205742;
        cmd.data[OFS+3] = 0x00020099;
        cmd.data[OFS+4] = 0x00000001;
        cmd.data[OFS+5] = 0x00000007;
        for (int j=6; j<64; j++)
            cmd.data[OFS+j] = 0;

        int chksum = 0;
        for (int j=2; j<63; j++)
            chksum ^= cmd.data[OFS+j];
        cmd.data[OFS+63] = chksum;
        printf("chksum=%x\n",chksum);

        err = write_cmd(&cmd);
        if (err<0) {
            printf("write_cmd err=%i\n", err);
            usleep(1000000);
            exit_now(1);
        }
        err = read_reply(&dgram);
        if (err<0) {
            printf("read_reply err=%i\n", err);
            usleep(1000000);
            exit_now(1);
        }
        printf("  data %3i = %#x\n", j, DSP_REPLY(&dgram)->data[0]);
    }

//    printf("\nTrigger frame.\n";)
    /* cmd.flags = 0; */
    /* cmd.data[0] = 0 + DSP_TRIGGER_FAKE; */
    /* cmd.size = 1; */
    /* err = write_cmd(&cmd); */
//    trigger_fake();

//    dump_buf();
    usleep(1000000);
    exit_now(0);

}
