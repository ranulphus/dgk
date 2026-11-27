/* enet_glos.c - ENet's platform layer on GLOS's sockets (glos/net.h, GLOS API 2.2's UDP), for dgk/net.h's DOS
 * library (libs.mk's dos: libdgknet.a). ENet's own unix.c with what GLOS hasn't: no sendmsg or recvmsg (a send's
 * buffers gathered into one datagram for sendto; recvfrom with MSG_TRUNC, which returns a datagram's own length,
 * so a cut one is seen), no poll (select), no listen or accept (ENet's hosts are UDP alone), and the time from
 * DJGPP's uclock (gettimeofday is the BIOS tick's). Adapted from ENet 1.3.18's unix.c (MIT, Copyright (c)
 * 2002-2024 Lee Salzman: third_party/enet); it came to dgk from Echoes of the Void (its EV11). dgk_net_start
 * checks that GLOS answers with its UDP before ENet creates a host. */
#ifdef __DJGPP__

#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <glos/net.h>
#define ENET_BUILDING_LIB 1
#include "enet/enet.h"

static enet_uint32 timeBase = 0;

int enet_initialize(void)
{
    return 0;
}

void enet_deinitialize(void)
{
}

enet_uint32 enet_host_random_seed(void)
{
    return (enet_uint32)time(NULL) ^ (enet_uint32)uclock();
}

static enet_uint32 now_ms(void)
{
    return (enet_uint32)(uclock() * 1000 / UCLOCKS_PER_SEC);
}

enet_uint32 enet_time_get(void)
{
    return now_ms() - timeBase;
}

void enet_time_set(enet_uint32 newTimeBase)
{
    timeBase = now_ms() - newTimeBase;
}

int enet_address_set_host_ip(ENetAddress *address, const char *name)
{
    return inet_pton(AF_INET, name, &address->host) == 1 ? 0 : -1;
}

int enet_address_set_host(ENetAddress *address, const char *name)
{
    struct addrinfo hints, *list = NULL, *r;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    if (getaddrinfo(name, NULL, &hints, &list) == 0) {
        for (r = list; r; r = r->ai_next)
            if (r->ai_family == AF_INET && r->ai_addr && r->ai_addrlen >= sizeof(struct sockaddr_in)) {
                address->host = ((struct sockaddr_in *)r->ai_addr)->sin_addr.s_addr;
                freeaddrinfo(list);
                return 0;
            }
        freeaddrinfo(list);
    }
    return enet_address_set_host_ip(address, name);
}

int enet_address_get_host_ip(const ENetAddress *address, char *name, size_t nameLength)
{
    return inet_ntop(AF_INET, &address->host, name, (socklen_t)nameLength) ? 0 : -1;
}

int enet_address_get_host(const ENetAddress *address, char *name, size_t nameLength)
{
    return enet_address_get_host_ip(address, name, nameLength);    /* (no reverse lookup) */
}

int enet_socket_bind(ENetSocket socket, const ENetAddress *address)
{
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    if (address) {
        sin.sin_port = ENET_HOST_TO_NET_16(address->port);
        sin.sin_addr.s_addr = address->host;
    } else {
        sin.sin_port = 0;
        sin.sin_addr.s_addr = INADDR_ANY;
    }
    return bind(socket, (struct sockaddr *)&sin, sizeof sin);
}

int enet_socket_get_address(ENetSocket socket, ENetAddress *address)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof sin;
    if (getsockname(socket, (struct sockaddr *)&sin, &len) == -1)
        return -1;
    address->host = (enet_uint32)sin.sin_addr.s_addr;
    address->port = ENET_NET_TO_HOST_16(sin.sin_port);
    return 0;
}

int enet_socket_listen(ENetSocket socket, int backlog)
{
    (void)socket;
    (void)backlog;
    return -1;                          /* (TCP's: ENet's hosts don't use it) */
}

ENetSocket enet_socket_create(ENetSocketType type)
{
    return socket(PF_INET, type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int enet_socket_set_option(ENetSocket socket, ENetSocketOption option, int value)
{
    int r = -1;
    switch (option) {
    case ENET_SOCKOPT_NONBLOCK:
        r = fcntl(socket, F_SETFL, value ? O_NONBLOCK : 0);
        break;
    case ENET_SOCKOPT_BROADCAST:
        r = setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &value, sizeof value);
        break;
    case ENET_SOCKOPT_REUSEADDR:
        r = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof value);
        break;
    case ENET_SOCKOPT_RCVBUF:
        r = setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &value, sizeof value);
        break;
    case ENET_SOCKOPT_SNDBUF:
        r = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &value, sizeof value);
        break;
    case ENET_SOCKOPT_RCVTIMEO:
    case ENET_SOCKOPT_SNDTIMEO: {
        struct timeval tv;
        tv.tv_sec = value / 1000;
        tv.tv_usec = (value % 1000) * 1000;
        r = setsockopt(socket, SOL_SOCKET, option == ENET_SOCKOPT_RCVTIMEO ? SO_RCVTIMEO : SO_SNDTIMEO, &tv, sizeof tv);
        break;
    }
    default:                            /* (TCP's no-delay, a TTL: not for ENet's UDP here) */
        r = 0;
        break;
    }
    return r == -1 ? -1 : 0;
}

int enet_socket_get_option(ENetSocket socket, ENetSocketOption option, int *value)
{
    socklen_t len = sizeof(int);
    if (option == ENET_SOCKOPT_ERROR)
        return getsockopt(socket, SOL_SOCKET, SO_ERROR, value, &len) == -1 ? -1 : 0;
    return -1;
}

int enet_socket_connect(ENetSocket socket, const ENetAddress *address)
{
    struct sockaddr_in sin;
    int r;
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = ENET_HOST_TO_NET_16(address->port);
    sin.sin_addr.s_addr = address->host;
    r = connect(socket, (struct sockaddr *)&sin, sizeof sin);
    return r == -1 && errno == EINPROGRESS ? 0 : r;
}

ENetSocket enet_socket_accept(ENetSocket socket, ENetAddress *address)
{
    (void)socket;
    (void)address;
    return ENET_SOCKET_NULL;            /* (TCP's) */
}

int enet_socket_shutdown(ENetSocket socket, ENetSocketShutdown how)
{
    return shutdown(socket, (int)how);
}

void enet_socket_destroy(ENetSocket socket)
{
    if (socket != -1)
        close(socket);
}

/* A send's buffers gathered into one datagram (GLOS has no sendmsg). */
int enet_socket_send(ENetSocket socket, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount)
{
    static enet_uint8 out[ENET_PROTOCOL_MAXIMUM_MTU];
    struct sockaddr_in sin;
    size_t i, n = 0;
    int sent;
    for (i = 0; i < bufferCount; i++) {
        if (n + buffers[i].dataLength > sizeof out)
            return -1;
        memcpy(out + n, buffers[i].data, buffers[i].dataLength);
        n += buffers[i].dataLength;
    }
    if (address) {
        memset(&sin, 0, sizeof sin);
        sin.sin_family = AF_INET;
        sin.sin_port = ENET_HOST_TO_NET_16(address->port);
        sin.sin_addr.s_addr = address->host;
        sent = (int)sendto(socket, out, n, 0, (struct sockaddr *)&sin, sizeof sin);
    } else
        sent = (int)send(socket, out, n, 0);
    if (sent == -1)
        return errno == EWOULDBLOCK ? 0 : -1;
    return sent;
}

/* A datagram into the first buffer (ENet receives into one); -2 if it was
 * cut (MSG_TRUNC: recvfrom gives its own length), as unix.c's. */
int enet_socket_receive(ENetSocket socket, ENetAddress *address, ENetBuffer *buffers, size_t bufferCount)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof sin;
    int got;
    if (bufferCount < 1)
        return -1;
    got = (int)recvfrom(socket, buffers[0].data, buffers[0].dataLength, MSG_TRUNC, (struct sockaddr *)&sin, &len);
    if (got == -1)
        return errno == EWOULDBLOCK ? 0 : -1;
    if ((size_t)got > buffers[0].dataLength)
        return -2;
    if (address) {
        address->host = (enet_uint32)sin.sin_addr.s_addr;
        address->port = ENET_NET_TO_HOST_16(sin.sin_port);
    }
    return got;
}

int enet_socketset_select(ENetSocket maxSocket, ENetSocketSet *readSet, ENetSocketSet *writeSet, enet_uint32 timeout)
{
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    return select(maxSocket + 1, readSet, writeSet, NULL, &tv);
}

int enet_socket_wait(ENetSocket socket, enet_uint32 *condition, enet_uint32 timeout)
{
    fd_set rs, ws;
    struct timeval tv;
    int n;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    if (*condition & ENET_SOCKET_WAIT_SEND)
        FD_SET(socket, &ws);
    if (*condition & ENET_SOCKET_WAIT_RECEIVE)
        FD_SET(socket, &rs);
    n = select(socket + 1, &rs, &ws, NULL, &tv);
    if (n < 0) {
        if (errno == EINTR && *condition & ENET_SOCKET_WAIT_INTERRUPT) {
            *condition = ENET_SOCKET_WAIT_INTERRUPT;
            return 0;
        }
        return -1;
    }
    *condition = ENET_SOCKET_WAIT_NONE;
    if (n == 0)
        return 0;
    if (FD_ISSET(socket, &ws))
        *condition |= ENET_SOCKET_WAIT_SEND;
    if (FD_ISSET(socket, &rs))
        *condition |= ENET_SOCKET_WAIT_RECEIVE;
    return 0;
}

#endif
