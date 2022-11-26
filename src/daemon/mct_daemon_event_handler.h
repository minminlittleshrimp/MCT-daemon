#include <poll.h>

#include "mct_daemon_connection_types.h"
#include "mct_daemon_event_handler_types.h"
#include "mct-daemon.h"

#ifndef DLT_DAEMON_EVENT_HANDLER_H
#define DLT_DAEMON_EVENT_HANDLER_H

int mct_daemon_prepare_event_handling(DltEventHandler *);
int mct_daemon_handle_event(DltEventHandler *, DltDaemon *, DltDaemonLocal *);

DltConnection *mct_event_handler_find_connection_by_id(DltEventHandler *,
                                                       DltConnectionId);
DltConnection *mct_event_handler_find_connection(DltEventHandler *,
                                                 int);

void mct_event_handler_cleanup_connections(DltEventHandler *);

int mct_event_handler_register_connection(DltEventHandler *,
                                          DltDaemonLocal *,
                                          DltConnection *,
                                          int);

int mct_event_handler_unregister_connection(DltEventHandler *,
                                            DltDaemonLocal *,
                                            int);

int mct_connection_check_activate(DltEventHandler *,
                                  DltConnection *,
                                  DltMessageFilter *,
                                  int);
#ifdef DLT_UNIT_TESTS
int mct_daemon_remove_connection(DltEventHandler *ev,
                                 DltConnection *to_remove);

void mct_daemon_add_connection(DltEventHandler *ev,
                               DltConnection *connection);
#endif
#endif /* DLT_DAEMON_EVENT_HANDLER_H */
