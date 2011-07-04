/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>
#include <linux/if_ether.h>

#include <mce_library.h>
#include <mce/types.h>

/* We need to access some low level stuff from the mce_library */
#include "context.h"
#include "eth.h"


int main()
{
    /* Create an MCE context, but hotwire set the ethernet stuff to serve. */
    mce_context_t mce;
    if ((mce = mcelib_create(0, "./mas.cfg", 0)) == NULL) {
        printf("Failed to mcelib_create.\n");
        return -1;
    }
    if (mcecmd_open(mce) || mcedata_open(mce)) {
        printf("Could not open MCE cmd/data device for %s.\n",
                mcelib_dev(mce));
        return -1;
	}

    // Set for MCE spoofing
    eth_transport* transport = (eth_transport*)mce->transport_data;
    eth_set_service(transport, 2);
    
    /* Set non-blocking.  Note the hack into mce->transport_data,
     * which we know the secret structure of */
    int socket = *(int*)(mce->transport_data);
    int flags = fcntl(socket, F_GETFL, 0);
    printf("flags %x\n", flags);
    //fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(socket, F_GETFL, 0);
    printf("flags %x\n", flags);

    /* Get buffer for a whole ethernet frame */
    unsigned char* buffer = malloc(ETH_FRAME_LEN);

    /* Listen for commands. */
    while (1) {
        printf("Listen.\n");
        int n = recvfrom(socket, buffer, ETH_FRAME_LEN, 0, NULL, NULL);
        
        uint32_t* mce_eth_packet = (uint32_t*)(buffer+16);
        uint32_t uid = ntohl(mce_eth_packet[0]);
        printf("Recieved %4i bytes from uid=%08x\n", n, uid);

        /* Interpret as a command */
        mce_command *cmd = (mce_command *)(mce_eth_packet+1);
        
        mce_reply rep;
        memset(&rep,0,sizeof(rep));
        rep.ok_er = MCE_OK;
        rep.command = cmd->command & 0xffff;
        rep.para_id = cmd->para_id;
        rep.card_id = cmd->card_id;
        rep.data[0] = 0x00000900;
        rep.data[1] = mcecmd_checksum((u32*)&rep, 3);
        
        eth_write(transport, socket,
                  0, 0, uid, &rep, sizeof(rep));
    }        
    return 0;
}
