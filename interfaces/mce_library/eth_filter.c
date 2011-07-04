/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/filter.h>


/* Linux packet filters.

   We will want to catch packets that match one or more criteria:
   1. The ethernet protocol ID must always be the MCE-over-ethernet code.
   2. The MCE-over-ethernet packet type must match one of our recognized codes
      (i.e. one of MCE reply, MCE data, or possibly MCE command).
   3. The unique ID must match our chosen unique ID (usually).

   We get the basic structure of the filter from a tcpdump example:

  tcpdump -v  'ether proto 0xa0b0 and ether[14:2] = 1 and ether[16:4] = 0x11223344'   -d
    (000) ldh      [12]
    (001) jeq      #0xa0b0          jt 2jf 7
    (002) ldh      [14]
    (003) jeq      #0x1             jt 4jf 7
    (004) ld       [16]
    (005) jeq      #0x11223344      jt 6jf 7
    (006) ret      #96
    (007) ret      #0
  
  which compiles to 
    { 0x28, 0, 0, 0x0000000c },
    { 0x15, 0, 5, 0x0000a0b0 },
    { 0x28, 0, 0, 0x0000000e },
    { 0x15, 0, 3, 0x00000001 },
    { 0x20, 0, 0, 0x00000010 },
    { 0x15, 0, 1, 0x11223344 },
    { 0x6, 0, 0, 0x00000060 },
    { 0x6, 0, 0, 0x00000000 },

  Note that this code contains hard-wired addresses for the MCE-over-ethernet
  protocol header; the MCE protocol and unique ID are assumed to be at offsets
  14 and 16 respectively.

  In the template, though, we use HEADER_* place-holders for the
  protocol, protocol, and unique ID fields so they can be filled in
  when the filter is created.

  The important thing about these HEADER_ defines is that they are
  different from each other and different from all other data in the
  4th (k) field of the filter_template entries.
*/

#define HEADER_PRO 0x80000000
#define HEADER_TYP 0x80000001
#define HEADER_UID 0x80000002
#define HEADER_EOF 0x8000000f

/* The filter creation code will traverse this template and replace
   the HEADER_ values with the relevant data. */

struct sock_filter filter_template[] = {
    { 0x28, 0, 0, 0x0000000c },  /* Load protocol */
    { 0x15, 0, 5, HEADER_PRO },  
    { 0x28, 0, 0, 0x0000000e },  /* Load MCE packet type */
    { 0x15, 0, 3, HEADER_TYP },
    { 0x20, 0, 0, 0x00000010 },  /* Load unique ID */
    { 0x15, 0, 1, HEADER_UID },
    {  0x6, 0, 0, 0x00000060 },
    {  0x6, 0, 0, 0x00000000 },
    {   0,  0, 0, HEADER_EOF }
};

/* A server doesn't filter on unique ID; rather it sould accept only
   commands, but accept them from anyone. it will take commands from
   anyone. */

struct sock_filter filter_template_server[] = {
    { 0x28, 0, 0, 0x0000000c },  /* Load protocol */
    { 0x15, 0, 3, HEADER_PRO },
    { 0x28, 0, 0, 0x0000000e },  /* Load MCE packet type */
    { 0x15, 0, 1, HEADER_TYP },
    {  0x6, 0, 0, 0x00000060 },
    {  0x6, 0, 0, 0x00000000 },
    {   0,  0, 0, HEADER_EOF }
};

/*
  eth_set_filter -- attach a filter to the target socket.

  The ethernet protocol ID and MCE packet type must be specified.  If
  uid!=0, then the filter will also select on uid.  If uid==0, the
  filter will ignore uid and accept all MCE-over-ethernet packets of
  the specified type.
*/


int eth_set_filter(int socket, int protocol, int type, int uid)
{
    int i, n;
    // If uid is 0, use the server template to match all packets of given type.
    struct sock_filter *template = filter_template;
    if (uid == 0)
        template = filter_template_server;

    // Count and allocate
    for (n=0; template[n].k != HEADER_EOF; n++) ;
    struct sock_filter *filter = malloc(n * sizeof(struct sock_filter));

    // Traverse the template and replace data.
    for (i=0; i<n; i++) {
        filter[i] = template[i];
        switch(filter[i].k) {
        case HEADER_PRO:
            filter[i].k = protocol;
            break;
        case HEADER_TYP:
            filter[i].k = type;
            break;
        case HEADER_UID:
            filter[i].k = uid;
            break;
        }
    }

    // Attach
    struct sock_fprog filter_prog = {
        .len = n,
        .filter = filter
    };
    int err = setsockopt(socket, SOL_SOCKET, SO_ATTACH_FILTER,
                         &filter_prog, sizeof(filter_prog));
    free(filter);
    if (err < 0) {
        fprintf(stderr, "Failed to attach packet filter (%i, %i).\n", err, errno);
        return -1;
    }
    return 0;
}

