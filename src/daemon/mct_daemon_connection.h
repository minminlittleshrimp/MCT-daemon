#ifndef DLT_DAEMON_CONNECTION_H
#define DLT_DAEMON_CONNECTION_H

#include "mct_daemon_connection_types.h"
#include "mct_daemon_event_handler_types.h"
#include "mct-daemon.h"

int mct_connection_send_multiple(DltConnection *, void *, int, void *, int, int);

DltConnection *mct_connection_get_next(DltConnection *, int);
int mct_connection_create_remaining(DltDaemonLocal *);

int mct_connection_create(DltDaemonLocal *,
                          DltEventHandler *,
                          int,
                          int,
                          DltConnectionType);
void mct_connection_destroy(DltConnection *);

void *mct_connection_get_callback(DltConnection *);

#ifdef DLT_UNIT_TESTS
int mct_connection_send(DltConnection *conn,
                        void *msg,
                        size_t msg_size);

void mct_connection_destroy_receiver(DltConnection *con);

DltReceiver *mct_connection_get_receiver(DltDaemonLocal *daemon_local,
                                         DltConnectionType type,
                                         int fd);
#endif

#endif /* DLT_DAEMON_CONNECTION_H */
