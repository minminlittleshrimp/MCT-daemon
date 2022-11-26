#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h> /* for socket(), connect(), (), and recv() */
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>

#include "mct-daemon.h"
#include "mct_common.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_socket.h"
#include "mct_daemon_unix_socket.h"

int mct_daemon_unix_socket_open(int *sock, char *sock_path, int type, int mask)
{
    struct sockaddr_un addr;
    int old_mask;

    if ((sock == NULL) || (sock_path == NULL)) {
        mct_log(LOG_ERR, "mct_daemon_unix_socket_open: arguments invalid");
        return -1;
    }

    if ((*sock = socket(AF_UNIX, type, 0)) == -1) {
        mct_log(LOG_WARNING, "unix socket: socket() error");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, sock_path, sizeof(addr.sun_path));

    unlink(sock_path);

    /* set appropriate access permissions */
    old_mask = umask(mask);

    if (bind(*sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        mct_vlog(LOG_WARNING, "%s: bind() error (%s)\n", __func__,
                 strerror(errno));
        return -1;
    }

    if (listen(*sock, 1) == -1) {
        mct_vlog(LOG_WARNING, "%s: listen error (%s)\n", __func__,
                 strerror(errno));
        return -1;
    }

    /* restore permissions */
    umask(old_mask);

    return 0;
}

int mct_daemon_unix_socket_close(int sock)
{
    int ret = close(sock);

    if (ret != 0) {
        mct_vlog(LOG_WARNING, "unix socket close failed: %s", strerror(errno));
    }

    return ret;
}
