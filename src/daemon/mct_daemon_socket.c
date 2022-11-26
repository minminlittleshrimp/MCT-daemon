#include <netdb.h>
#include <ctype.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), (), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>

#ifdef linux
#include <sys/timerfd.h>
#endif
#include <sys/time.h>
#if defined(linux) && defined(__NR_statx)
#include <linux/stat.h>
#endif

#include "mct_types.h"
#include "mct-daemon.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_common_cfg.h"

#include "mct_daemon_socket.h"

int mct_daemon_socket_open(int *sock, unsigned int servPort, char *ip)
{
    int yes = 1;
    int ret_inet_pton = 1;
    int lastErrno = 0;

#ifdef MCT_USE_IPv6

    /* create socket */
    if ((*sock = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
        lastErrno = errno;
        mct_vlog(LOG_ERR, "mct_daemon_socket_open: socket() error %d: %s\n", lastErrno,
                 strerror(lastErrno));
        return -1;
    }

#else

    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        lastErrno = errno;
        mct_vlog(LOG_ERR, "mct_daemon_socket_open: socket() error %d: %s\n", lastErrno,
                 strerror(lastErrno));
        return -1;
    }

#endif

    mct_vlog(LOG_INFO, "%s: Socket created\n", __FUNCTION__);

    /* setsockpt SO_REUSEADDR */
    if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        lastErrno = errno;
        mct_vlog(
            LOG_ERR,
            "mct_daemon_socket_open: Setsockopt error %d in mct_daemon_local_connection_init: %s\n",
            lastErrno,
            strerror(lastErrno));
        return -1;
    }

    /* bind */
#ifdef MCT_USE_IPv6
    struct sockaddr_in6 forced_addr;
    memset(&forced_addr, 0, sizeof(forced_addr));
    forced_addr.sin6_family = AF_INET6;
    forced_addr.sin6_port = htons(servPort);

    if (0 == strcmp(ip, "0.0.0.0")) {
        forced_addr.sin6_addr = in6addr_any;
    } else {
        ret_inet_pton = inet_pton(AF_INET6, ip, &forced_addr.sin6_addr);
    }

#else
    struct sockaddr_in forced_addr;
    memset(&forced_addr, 0, sizeof(forced_addr));
    forced_addr.sin_family = AF_INET;
    forced_addr.sin_port = htons(servPort);
    ret_inet_pton = inet_pton(AF_INET, ip, &forced_addr.sin_addr);
#endif

    /* inet_pton returns 1 on success */
    if (ret_inet_pton != 1) {
        lastErrno = errno;
        mct_vlog(
            LOG_WARNING,
            "mct_daemon_socket_open: inet_pton() error %d: %s. Cannot convert IP address: %s\n",
            lastErrno,
            strerror(lastErrno),
            ip);
        return -1;
    }

    if (bind(*sock, (struct sockaddr *)&forced_addr, sizeof(forced_addr)) == -1) {
        lastErrno = errno;     /*close() may set errno too */
        close(*sock);
        mct_vlog(LOG_WARNING, "mct_daemon_socket_open: bind() error %d: %s\n", lastErrno,
                 strerror(lastErrno));
        return -1;
    }

    /*listen */
    mct_vlog(LOG_INFO, "%s: Listening on ip %s and port: %u\n", __FUNCTION__, ip, servPort);

    /* get socket buffer size */
    mct_vlog(LOG_INFO, "mct_daemon_socket_open: Socket send queue size: %d\n",
             mct_daemon_socket_get_send_qeue_max_size(*sock));

    if (listen(*sock, 3) < 0) {
        lastErrno = errno;
        mct_vlog(LOG_WARNING,
                 "mct_daemon_socket_open: listen() failed with error %d: %s\n",
                 lastErrno,
                 strerror(lastErrno));
        return -1;
    }

    return 0; /* OK */
}

int mct_daemon_socket_close(int sock)
{
    close(sock);

    return 0;
}

int mct_daemon_socket_send(int sock,
                           void *data1,
                           int size1,
                           void *data2,
                           int size2,
                           char serialheader)
{
    int ret = MCT_RETURN_OK;

    /* Optional: Send serial header, if requested */
    if (serialheader) {
        ret = mct_daemon_socket_sendreliable(sock,
                                             (void *)mctSerialHeader,
                                             sizeof(mctSerialHeader));

        if (ret != MCT_RETURN_OK) {
            return ret;
        }
    }

    /* Send data */
    if ((data1 != NULL) && (size1 > 0)) {
        ret = mct_daemon_socket_sendreliable(sock, data1, size1);

        if (ret != MCT_RETURN_OK) {
            return ret;
        }
    }

    if ((data2 != NULL) && (size2 > 0)) {
        ret = mct_daemon_socket_sendreliable(sock, data2, size2);
    }

    return ret;
}

int mct_daemon_socket_get_send_qeue_max_size(int sock)
{
    int n = 0;
    socklen_t m = sizeof(n);
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&n, &m);

    return n;
}

int mct_daemon_socket_sendreliable(int sock, void *data_buffer, int message_size)
{
    int data_sent = 0;

    while (data_sent < message_size) {
        ssize_t ret = send(sock,
                           (uint8_t *)data_buffer + data_sent,
                           message_size - data_sent,
                           0);

        if (ret < 0) {
            mct_vlog(LOG_WARNING,
                     "%s: socket send failed [errno: %d]!\n", __func__, errno);
            return MCT_DAEMON_ERROR_SEND_FAILED;
        } else {
            data_sent += ret;
        }
    }

    return MCT_DAEMON_ERROR_OK;
}

