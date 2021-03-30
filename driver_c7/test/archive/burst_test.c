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

#include "dspioctl.h"

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



void dump_regs() {
    printf(" HSTR: %06x\n", read_io(HSTR));
    printf(" HCVR: %06x\n", read_io(HCVR));
    printf(" HCTR: %06x\n", read_io(HCTR));
    printf("\n");
    
}

void stop_test() {
    int x = read_io(HCTR);
    write_io(HCTR, x & ~0x20);
}

void int_handler(int sig) {
    if (sig == SIGINT) {
        printf("Stopping!\n");
        stop_test();
    }
    // Hand it up.
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(void) {
    int x, y, z, i, j, k;
    fd = open("/dev/mce_test0", O_RDWR);
    printf("Hi. fd=%i\n\n", fd);
    if (fd<=0) 
        return -1;

    printf("On entry:\n");
	dump_regs();

    // Begin the test
    x = read_io(HCTR);
    write_io(HCTR, x | 0x20);


    /* signal(SIGINT, int_handler); */
    /* while(1) { */
    /*     usleep(100000); */
    /*     dump_regs(); */
    /*         } */
        

    usleep(100000);
    /* if (get_hf4()) { */
    /*     printf("DSP responding.\n"); */
    /* } else { */
    /*     printf("No DSP ack?\n"); */
    /*     stop_test(); */
    /*     return 1; */
    /* } */

    // Install Ctrl-c handler
    signal(SIGINT, int_handler);

    int n_to_read = 10000000;
    while(n_to_read > 0) {
        if (!get_hf4()) {
            printf("HF4 has fallen\n");
            break;
        }
        i = 0;
        while (get_hrrq()) {
            j = read_io(HRXS);
            i+=1;
            n_to_read -= 1;
            if (n_to_read==0)
                break;
        }
        /* printf("  burst of %i (%i)\n", i, j); */
        /* dump_regs(); */
        /* usleep(10000); */
    }

    /* // Shut down the test */
    stop_test();
    // Pick up strays:
    i = 0;
    while (get_hf4() || get_hrrq()) {
        while (get_hrrq()) {
            j = read_io(HRXS);
            i += 1;
        }
    }
    printf(" %i strays\n", i);

    close(fd);
    return 0;
}
