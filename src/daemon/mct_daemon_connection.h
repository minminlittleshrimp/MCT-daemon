#ifndef MCT_DAEMON_CONNECTION_H
#define MCT_DAEMON_CONNECTION_H

#include "mct_daemon_connection_types.h"
#include "mct_daemon_event_handler_types.h"
#include "mct-daemon.h"

int mct_connection_send_multiple(MctConnection *, void *, int, void *, int, int);

MctConnection *mct_connection_get_next(MctConnection *, int);
int mct_connection_create_remaining(MctDaemonLocal *);

int mct_connection_create(MctDaemonLocal *,
                          MctEventHandler *,
                          int,
                          int,
                          MctConnectionType);
void mct_connection_destroy(MctConnection *);

void *mct_connection_get_callback(MctConnection *);

#endif /* MCT_DAEMON_CONNECTION_H */
