#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h> /* send() */

#include "mct-daemon.h"

#include "mct_types.h"

#include "mct_daemon_serial.h"

int mct_daemon_serial_send(int sock,
                           void *data1,
                           int size1,
                           void *data2,
                           int size2,
                           char serialheader)
{
    /* Optional: Send serial header, if requested */
    if (serialheader) {
        if (0 > write(sock, mctSerialHeader, sizeof(mctSerialHeader))) {
            return MCT_DAEMON_ERROR_SEND_FAILED;
        }
    }

    /* Send data */

    if (data1 && (size1 > 0)) {
        if (0 > write(sock, data1, size1)) {
            return MCT_DAEMON_ERROR_SEND_FAILED;
        }
    }

    if (data2 && (size2 > 0)) {
        if (0 > write(sock, data2, size2)) {
            return MCT_DAEMON_ERROR_SEND_FAILED;
        }
    }

    return MCT_DAEMON_ERROR_OK;
}
