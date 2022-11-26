#ifndef MCT_DAEMON_CONNECTION_TYPES_H
#define MCT_DAEMON_CONNECTION_TYPES_H
#include "mct_common.h"

typedef enum {
    UNDEFINED,  /* Undefined status */
    INACTIVE,   /* Connection is inactive, excluded from poll handling */
    ACTIVE,     /* Connection is actively handled by poll */
    DEACTIVATE, /* Request for deactivation of the connection */
    ACTIVATE    /* Request for activation of the connection */
} MctConnectionStatus;

typedef enum {
    MCT_CONNECTION_NONE = 0,
    MCT_CONNECTION_CLIENT_CONNECT,
    MCT_CONNECTION_CLIENT_MSG_TCP,
    MCT_CONNECTION_CLIENT_MSG_SERIAL,
    MCT_CONNECTION_APP_CONNECT,
    MCT_CONNECTION_APP_MSG,
    MCT_CONNECTION_ONE_S_TIMER,
    MCT_CONNECTION_SIXTY_S_TIMER,
    MCT_CONNECTION_SYSTEMD_TIMER,
    MCT_CONNECTION_CONTROL_CONNECT,
    MCT_CONNECTION_CONTROL_MSG,
    MCT_CONNECTION_CLIENT_MSG_OFFLINE_TRACE,
    MCT_CONNECTION_CLIENT_MSG_OFFLINE_LOGSTORAGE,
    MCT_CONNECTION_FILTER,
    MCT_CONNECTION_GATEWAY,
    MCT_CONNECTION_GATEWAY_TIMER,
    MCT_CONNECTION_TYPE_MAX
} MctConnectionType;

#define MCT_CON_MASK_CLIENT_CONNECT     (1 << MCT_CONNECTION_CLIENT_CONNECT)
#define MCT_CON_MASK_CLIENT_MSG_TCP     (1 << MCT_CONNECTION_CLIENT_MSG_TCP)
#define MCT_CON_MASK_CLIENT_MSG_SERIAL  (1 << MCT_CONNECTION_CLIENT_MSG_SERIAL)
#define MCT_CON_MASK_APP_MSG            (1 << MCT_CONNECTION_APP_MSG)
#define MCT_CON_MASK_APP_CONNECT        (1 << MCT_CONNECTION_APP_CONNECT)
#define MCT_CON_MASK_ONE_S_TIMER        (1 << MCT_CONNECTION_ONE_S_TIMER)
#define MCT_CON_MASK_SIXTY_S_TIMER      (1 << MCT_CONNECTION_SIXTY_S_TIMER)
#define MCT_CON_MASK_SYSTEMD_TIMER      (1 << MCT_CONNECTION_SYSTEMD_TIMER)
#define MCT_CON_MASK_CONTROL_CONNECT    (1 << MCT_CONNECTION_CONTROL_CONNECT)
#define MCT_CON_MASK_CONTROL_MSG        (1 << MCT_CONNECTION_CONTROL_MSG)
#define MCT_CON_MASK_CLIENT_MSG_OFFLINE_TRACE \
    (1 << MCT_CONNECTION_CLIENT_MSG_OFFLINE_TRACE)
#define MCT_CON_MASK_CLIENT_MSG_OFFLINE_LOGSTORAGE \
    (1 << MCT_CONNECTION_CLIENT_MSG_OFFLINE_LOGSTORAGE)
#define MCT_CON_MASK_FILTER             (1 << MCT_CONNECTION_FILTER)
#define MCT_CON_MASK_GATEWAY            (1 << MCT_CONNECTION_GATEWAY)
#define MCT_CON_MASK_GATEWAY_TIMER      (1 << MCT_CONNECTION_GATEWAY_TIMER)
#define MCT_CON_MASK_ALL                (0xffff)

#define MCT_CONNECTION_TO_MASK(C)        (1 << (C))

typedef uintptr_t MctConnectionId;

/* TODO: squash the MctReceiver structure in there
 * and remove any other duplicates of FDs
 */
typedef struct MctConnection {
    MctConnectionId id;
    MctReceiver *receiver;      /**< Receiver structure for this connection */
    MctConnectionType type;     /**< Represents what type of handle is this (like FIFO, serial, client, server) */
    MctConnectionStatus status; /**< Status of connection */
    struct MctConnection *next; /**< For multiple client connection using linked list */
    int ev_mask;                /**< Mask to set when registering the connection for events */
} MctConnection;

#endif /* MCT_DAEMON_CONNECTION_TYPES_H */
