#include <poll.h>

#include "mct_daemon_connection_types.h"
#include "mct_daemon_event_handler_types.h"
#include "mct-daemon.h"

#ifndef MCT_DAEMON_EVENT_HANDLER_H
#define MCT_DAEMON_EVENT_HANDLER_H

int mct_daemon_prepare_event_handling(MctEventHandler *);
int mct_daemon_handle_event(MctEventHandler *, MctDaemon *, MctDaemonLocal *);

MctConnection *mct_event_handler_find_connection_by_id(MctEventHandler *,
                                                       MctConnectionId);
MctConnection *mct_event_handler_find_connection(MctEventHandler *,
                                                 int);

void mct_event_handler_cleanup_connections(MctEventHandler *);

int mct_event_handler_register_connection(MctEventHandler *,
                                          MctDaemonLocal *,
                                          MctConnection *,
                                          int);

int mct_event_handler_unregister_connection(MctEventHandler *,
                                            MctDaemonLocal *,
                                            int);

int mct_connection_check_activate(MctEventHandler *,
                                  MctConnection *,
                                  MctMessageFilter *,
                                  int);
#endif /* MCT_DAEMON_EVENT_HANDLER_H */
