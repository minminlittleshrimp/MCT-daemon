#include <poll.h>

#include "mct_daemon_connection_types.h"

#ifndef MCT_DAEMON_EVENT_HANDLER_TYPES_H
#define MCT_DAEMON_EVENT_HANDLER_TYPES_H

/* FIXME: Remove the need for MctDaemonLocal everywhere in the code
 * These typedefs are needed by MctDaemonLocal which is
 * itself needed for functions used by the event handler
 * (as this structure is used everywhere in the code ...)
 */

typedef enum {
    MCT_TIMER_PACKET = 0,
    MCT_TIMER_ECU,
    MCT_TIMER_GATEWAY,
    MCT_TIMER_UNKNOWN
} MctTimers;

typedef struct {
    struct pollfd *pfd;
    nfds_t nfds;
    nfds_t max_nfds;
    MctConnection *connections;
} MctEventHandler;

#endif /* MCT_DAEMON_EVENT_HANDLER_TYPES_H */
