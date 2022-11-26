#ifndef MCT_DAEMON_SERIAL_H
#define MCT_DAEMON_SERIAL_H

#include <limits.h>
#include <semaphore.h>
#include "mct_common.h"
#include "mct_user.h"

int mct_daemon_serial_send(int sock,
                           void *data1,
                           int size1,
                           void *data2,
                           int size2,
                           char serialheader);

#endif /* MCT_DAEMON_SERIAL_H */
