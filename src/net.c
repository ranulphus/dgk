/* net.c - the network wherever the game runs (dgk/net.h): GLOS's sockets on DOS under GLOS 2.2 or later, the
 * host's on Linux and headless, none on plain DOS. The platform's differences are all here. */
#include "dgk/net.h"
#include "dgk/dgk.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __DJGPP__                        /* (GLOS's sockets: API 2.2's UDP, the BSD calls in glos/posix) */
#include <glos/glos.h>
#include <glos/net.h>
#include <sys/time.h>
#define SLICE_MS 4                      /* (a socket wait holds the program's interrupts: a few ms at a time) */
#define BETWEEN() dgk_service_if_due()  /* (between slices: dgk's timer and sound serviced) */
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#define SLICE_MS 0                      /* (no slices: one wait) */
#define BETWEEN() ((void)0)             /* (a host program without dgk's loop may use this file: nothing here) */
#endif

static int status = -1, started;

int dgk_net_status(void)
{
    if (status >= 0)
        return status;
#ifdef __DJGPP__
    {
        struct glos_version v;
        int s;
        if (!glos_running())            /* plain DOS: no network; no packet driver looked for */
            return status = DGK_NET_NONE;
        memset(&v, 0, sizeof v);
        v.size = sizeof v;
        if (glos_version(&v) != 0 || !(v.major > 2 || (v.major == 2 && v.minor >= 2)) ||
            !GLOS_HAS(&v, GLOS_NET_RECVFROM))
            return status = DGK_NET_OLD;
        s = socket(AF_INET, SOCK_DGRAM, 0);     /* (2.2's sockets are an app's: a full-screen one has none) */
        if (s == -1)
            return status = errno == ENOSYS ? DGK_NET_WINDOW : DGK_NET_OLD;
        close(s);
    }
#endif
    return status = DGK_NET_OK;
}

int dgk_net_start(void)
{
    if (dgk_net_status() != DGK_NET_OK)
        return -1;
    if (!started && enet_initialize() != 0)
        return -1;
    started = 1;
    return 0;
}

uint32_t dgk_net_ms(void)
{
    return dgk_net_start() == 0 ? (uint32_t)enet_time_get() : 0;
}

int dgk_net_service(ENetHost *host, ENetEvent *event, int ms)
{
    int left = ms > 0 ? ms : 0, got;
    if (!SLICE_MS)
        return enet_host_service(host, event, (enet_uint32)left);
    do {                                /* (DOS: a few ms at a time, dgk serviced between: its timer, its sound) */
        int slice = left > SLICE_MS ? SLICE_MS : left;
        got = enet_host_service(host, event, (enet_uint32)slice);
        left -= slice;
        BETWEEN();
    } while (got == 0 && left > 0);
    return got;
}

int dgk_udp_open(int port)
{
    int s, on = 1;
    if (dgk_net_status() != DGK_NET_OK)
        return -1;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
    if (port) {
        struct sockaddr_in sa;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        memset(&sa, 0, sizeof sa);
        sa.sin_family = AF_INET;
        sa.sin_port = htons((uint16_t)port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (struct sockaddr *)&sa, sizeof sa) != 0) {
            close(s);
            return -1;
        }
    }
    fcntl(s, F_SETFL, O_NONBLOCK);
    return s;
}

int dgk_udp_send(int s, uint32_t addr, int port, const void *data, int n)
{
    struct sockaddr_in to;
    if (s < 0)
        return -1;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons((uint16_t)port);
    to.sin_addr.s_addr = htonl(addr);
    return (int)sendto(s, data, (size_t)n, 0, (struct sockaddr *)&to, sizeof to);
}

/* Readable within ms? (select: GLOS has no poll; on DOS in slices, dgk serviced between) */
static int readable(int s, int ms)
{
    int left = ms > 0 ? ms : 0;
    for (;;) {
        int slice = SLICE_MS && left > SLICE_MS ? SLICE_MS : left, r;
        fd_set rs;
        struct timeval tv;
        FD_ZERO(&rs);
        FD_SET(s, &rs);
        tv.tv_sec = slice / 1000;
        tv.tv_usec = (slice % 1000) * 1000;
        r = select(s + 1, &rs, NULL, NULL, &tv);
        left -= slice;
        BETWEEN();
        if (r != 0 || left <= 0)
            return r > 0;
    }
}

int dgk_udp_recv(int s, void *data, int max, uint32_t *addr, int *port, int ms)
{
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    ssize_t n;
    if (s < 0)
        return -1;
    if (ms > 0 && !readable(s, ms))
        return 0;
    n = recvfrom(s, data, (size_t)max, 0, (struct sockaddr *)&from, &fl);
    if (n < 0)
        return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
    if (addr)
        *addr = ntohl(from.sin_addr.s_addr);
    if (port)
        *port = ntohs(from.sin_port);
    return (int)n;
}

void dgk_udp_close(int s)
{
    if (s >= 0)
        close(s);
}

void dgk_net_addr(uint32_t addr, char *out, int n)
{
    snprintf(out, (size_t)n, "%u.%u.%u.%u", (unsigned)(addr >> 24), (unsigned)(addr >> 16 & 255),
             (unsigned)(addr >> 8 & 255), (unsigned)(addr & 255));
}
