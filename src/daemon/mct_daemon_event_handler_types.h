#include <poll.h>

#include "mct_daemon_connection_types.h"

#ifndef DLT_DAEMON_EVENT_HANDLER_TYPES_H
#define DLT_DAEMON_EVENT_HANDLER_TYPES_H

/* FIXME: Remove the need for DltDaemonLocal everywhere in the code
 * These typedefs are needed by DltDaemonLocal which is
 * itself needed for functions used by the event handler
 * (as this structure is used everywhere in the code ...)
 */

typedef enum {
    DLT_TIMER_PACKET = 0,
    DLT_TIMER_ECU,
#ifdef DLT_SYSTEMD_WATCHDOG_ENABLE
    DLT_TIMER_SYSTEMD,
#endif
    DLT_TIMER_GATEWAY,
    DLT_TIMER_UNKNOWN
} DltTimers;

typedef struct {
    struct pollfd *pfd;
    nfds_t nfds;
    nfds_t max_nfds;
    DltConnection *connections;
} DltEventHandler;

#endif /* DLT_DAEMON_EVENT_HANDLER_TYPES_H */
