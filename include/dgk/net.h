/* dgk/net.h - the network, wherever the game runs: ENet (third_party/enet) for the game's own links, and UDP
 * datagrams for finding games on the LAN.
 *
 *   under GLOS 2.2 or later   GLOS's sockets (API 2.2's UDP): an app's (a window on GLOS's desktop), and from
 *                             2.3 a full-screen program's too
 *   Linux and headless        the host's sockets
 *   plain DOS                 none (DGK_NET_NONE): no packet driver is looked for, no port or interrupt touched
 *
 * dgk_net_status says which, and why not; each game words that to its players itself. A game's links are ENet's
 * own calls (<enet/enet.h>, with $(DGK_NET_CFLAGS)) after dgk_net_start(); a wait on an ENet host goes through
 * dgk_net_service, which on DOS waits in slices of 4 ms with dgk serviced between (under GLOS a socket call that
 * waits holds the program's own interrupt handlers, the timer's, the keyboard's and the sound's, till it
 * returns). The UDP calls keep a game clear of BSD socket headers: addresses are IPv4 in host order,
 * DGK_NET_BROADCAST the LAN's broadcast and DGK_NET_LOOPBACK this machine. */
#ifndef DGK_NET_H
#define DGK_NET_H
#include "dgk/base.h"
#include <enet/enet.h>

enum {
    DGK_NET_OK,                         /* ENet and UDP work here */
    DGK_NET_NONE,                       /* plain DOS: no network */
    DGK_NET_OLD,                        /* GLOS before 2.2 (no UDP) */
    DGK_NET_WINDOW                      /* GLOS 2.2: its sockets are an app's; a full-screen program has none */
};
#define DGK_NET_BROADCAST 0xFFFFFFFFu
#define DGK_NET_LOOPBACK  0x7F000001u

int dgk_net_status(void);               /* asked once, then remembered */
int dgk_net_start(void);                /* 0: ENet ready (status OK); -1 otherwise */
uint32_t dgk_net_ms(void);              /* ENet's clock in ms (0 without a network) */
/* enet_host_service, ms at most (0: what has come), in slices on DOS with dgk serviced between: > 0 an event */
int dgk_net_service(ENetHost *host, ENetEvent *event, int ms);

/* UDP: a non-blocking socket, broadcasts allowed; bound to port (shared with others, SO_REUSEADDR) unless 0
 * (a sender's: the system picks). -1: none (or no network). */
int  dgk_udp_open(int port);
int  dgk_udp_send(int s, uint32_t addr, int port, const void *data, int n);     /* bytes sent, -1 */
/* One datagram, waiting up to ms for it (0: none waited for): its length (cut to max), 0 none, -1 an error;
 * where it came from in *addr and *port (either may be NULL). */
int  dgk_udp_recv(int s, void *data, int max, uint32_t *addr, int *port, int ms);
void dgk_udp_close(int s);
void dgk_net_addr(uint32_t addr, char *out, int n);    /* "a.b.c.d" */

#endif
