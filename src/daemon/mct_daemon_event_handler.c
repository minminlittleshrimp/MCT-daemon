#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <poll.h>
#include <syslog.h>

#include "mct_common.h"

#include "mct-daemon.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_common.h"
#include "mct_daemon_connection.h"
#include "mct_daemon_connection_types.h"
#include "mct_daemon_event_handler.h"
#include "mct_daemon_event_handler_types.h"
#include "mct_daemon_filter.h"

/**
 * \def MCT_EV_TIMEOUT_MSEC
 * The maximum amount of time to wait for a poll event.
 * Set to 1 second to avoid unnecessary wake ups.
 */
#define MCT_EV_TIMEOUT_MSEC 1000
#define MCT_EV_BASE_FD      16

#define MCT_EV_MASK_REJECTED (POLLERR | POLLNVAL)

/** @brief Initialize a pollfd structure
 *
 * That ensures that no event will be mis-watched.
 *
 * @param pfd The element to initialize
 */
static void init_poll_fd(struct pollfd *pfd)
{
    pfd->fd = -1;
    pfd->events = 0;
    pfd->revents = 0;
}

/** @brief Prepare the event handler
 *
 * This will create the base poll file descriptor list.
 *
 * @param ev The event handler to prepare.
 *
 * @return 0 on success, -1 otherwise.
 */
int mct_daemon_prepare_event_handling(MctEventHandler *ev)
{
    int i = 0;

    if (ev == NULL) {
        return MCT_RETURN_ERROR;
    }

    ev->pfd = calloc(MCT_EV_BASE_FD, sizeof(struct pollfd));

    if (ev->pfd == NULL) {
        mct_log(LOG_CRIT, "Creation of poll instance failed!\n");
        return -1;
    }

    for (i = 0; i < MCT_EV_BASE_FD; i++) {
        init_poll_fd(&ev->pfd[i]);
    }

    ev->nfds = 0;
    ev->max_nfds = MCT_EV_BASE_FD;

    return 0;
}

/** @brief Enable a file descriptor to be watched
 *
 * Adds a file descriptor to the descriptor list. If the list is to small,
 * increase its size.
 *
 * @param ev The event handler structure, containing the list
 * @param fd The file descriptor to add
 * @param mask The mask of event to be watched
 */
static void mct_event_handler_enable_fd(MctEventHandler *ev, int fd, int mask)
{
    if (ev->max_nfds <= ev->nfds) {
        int i = ev->nfds;
        int max = 2 * ev->max_nfds;
        struct pollfd *tmp = realloc(ev->pfd, max * sizeof(*ev->pfd));

        if (!tmp) {
            mct_log(LOG_CRIT,
                    "Unable to register new fd for the event handler.\n");
            return;
        }

        ev->pfd = tmp;
        ev->max_nfds = max;

        for (; i < max; i++) {
            init_poll_fd(&ev->pfd[i]);
        }
    }

    ev->pfd[ev->nfds].fd = fd;
    ev->pfd[ev->nfds].events = mask;
    ev->nfds++;
}

/** @brief Disable a file descriptor for watching
 *
 * The file descriptor is removed from the descriptor list, the list is
 * compressed during the process.
 *
 * @param ev The event handler structure containing the list
 * @param fd The file descriptor to be removed
 */
static void mct_event_handler_disable_fd(MctEventHandler *ev, int fd)
{
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int nfds = ev->nfds;

    for (; i < nfds; i++, j++) {
        if (ev->pfd[i].fd == fd) {
            init_poll_fd(&ev->pfd[i]);
            j++;
            ev->nfds--;
        }

        if (i == j) {
            continue;
        }

        /* Compressing the table */
        if (i < ev->nfds) {
            ev->pfd[i].fd = ev->pfd[j].fd;
            ev->pfd[i].events = ev->pfd[j].events;
            ev->pfd[i].revents = ev->pfd[j].revents;
        } else {
            init_poll_fd(&ev->pfd[i]);
        }
    }
}

/** @brief Catch and process incoming events.
 *
 * This function waits for events on all connections. Once an event raise,
 * the callback for the specific connection is called, or the connection is
 * destroyed if a hangup occurs.
 *
 * @param daemon Structure to be passed to the callback.
 * @param daemon_local Structure containing needed information.
 * @param pEvent Event handler structure.
 *
 * @return 0 on success, -1 otherwise. May be interrupted.
 */
int mct_daemon_handle_event(MctEventHandler *pEvent,
                            MctDaemon *daemon,
                            MctDaemonLocal *daemon_local)
{
    int ret = 0;
    unsigned int i = 0;
    int (*callback)(MctDaemon *, MctDaemonLocal *, MctReceiver *, int) = NULL;

    if ((pEvent == NULL) || (daemon == NULL) || (daemon_local == NULL)) {
        return MCT_RETURN_ERROR;
    }

    ret = poll(pEvent->pfd, pEvent->nfds, MCT_EV_TIMEOUT_MSEC);

    if (ret <= 0) {
        /* We are not interested in EINTR has it comes
         * either from timeout or signal.
         */
        if (errno == EINTR) {
            ret = 0;
        }

        if (ret < 0) {
            mct_vlog(LOG_CRIT, "poll() failed: %s\n", strerror(errno));
        }

        return ret;
    }

    for (i = 0; i < pEvent->nfds; i++) {
        int fd = 0;
        MctConnection *con = NULL;
        MctConnectionType type = MCT_CONNECTION_TYPE_MAX;

        if (pEvent->pfd[i].revents == 0) {
            continue;
        }

        con = mct_event_handler_find_connection(pEvent, pEvent->pfd[i].fd);

        if (con && con->receiver) {
            type = con->type;
            fd = con->receiver->fd;
        } else { /* connection might have been destroyed in the meanwhile */
            mct_event_handler_disable_fd(pEvent, pEvent->pfd[i].fd);
            continue;
        }

        /* First of all handle error events */
        if (pEvent->pfd[i].revents & MCT_EV_MASK_REJECTED) {
            /* An error occurred, we need to clean-up the concerned event
             */
            if (type == MCT_CONNECTION_CLIENT_MSG_TCP) {
                /* To transition to BUFFER state if this is final TCP client connection,
                 * call dedicated function. this function also calls
                 * mct_event_handler_unregister_connection() inside the function.
                 */
                mct_daemon_close_socket(fd, daemon, daemon_local, 0);
            } else {
                mct_event_handler_unregister_connection(pEvent,
                                                        daemon_local,
                                                        fd);
            }

            continue;
        }

        /* Get the function to be used to handle the event */
        callback = mct_connection_get_callback(con);

        if (!callback) {
            mct_vlog(LOG_CRIT, "Unable to find function for %u handle type.\n",
                     type);
            return -1;
        }

        /* From now on, callback is correct */
        if (callback(daemon,
                     daemon_local,
                     con->receiver,
                     daemon_local->flags.vflag) == -1) {
            mct_vlog(LOG_CRIT, "Processing from %u handle type failed!\n",
                     type);
            return -1;
        }
    }

    return 0;
}

/** @brief Find connection with a specific \a fd in the connection list.
 *
 * There can be only one event per \a fd. We can then find a specific connection
 * based on this \a fd. That allows to check if a specific \a fd has already been
 * registered.
 *
 * @param ev The event handler structure where the list of connection is.
 * @param fd The file descriptor of the connection to be found.
 *
 * @return The found connection pointer, NULL otherwise.
 */
MctConnection *mct_event_handler_find_connection(MctEventHandler *ev, int fd)
{
    MctConnection *temp = ev->connections;

    while (temp != NULL) {
        if ((temp->receiver != NULL) && (temp->receiver->fd == fd)) {
            return temp;
        }

        temp = temp->next;
    }

    return temp;
}

/** @brief Remove a connection from the list and destroy it.
 *
 * This function will first look for the connection in the event handler list,
 * remove it from the list and then destroy it.
 *
 * @param ev The event handler structure where the list of connection is.
 * @param to_remove The connection to remove from the list.
 *
 * @return 0 on success, -1 if the connection is not found.
 */
static int mct_daemon_remove_connection(MctEventHandler *ev,
                                            MctConnection *to_remove)
{
    if ((ev == NULL) || (to_remove == NULL)) {
        return MCT_RETURN_ERROR;
    }

    MctConnection *curr = ev->connections;
    MctConnection *prev = curr;

    /* Find the address where to_remove value is registered */
    while (curr && (curr != to_remove)) {
        prev = curr;
        curr = curr->next;
    }

    if (!curr) {
        /* Must not be possible as we check for existence before */
        mct_log(LOG_CRIT, "Connection not found for removal.\n");
        return -1;
    } else if (curr == ev->connections) {
        ev->connections = curr->next;
    } else {
        prev->next = curr->next;
    }

    /* Now we can destroy our pointer */
    mct_connection_destroy(to_remove);

    return 0;
}

/** @brief Destroy the connection list.
 *
 * This function runs through the connection list and destroy them one by one.
 *
 * @param ev Pointer to the event handler structure.
 */
void mct_event_handler_cleanup_connections(MctEventHandler *ev)
{
    unsigned int i = 0;

    if (ev == NULL) {
        /* Nothing to do. */
        return;
    }

    while (ev->connections != NULL)
        /* We don really care on failure */
        (void)mct_daemon_remove_connection(ev, ev->connections);

    for (i = 0; i < ev->nfds; i++) {
        init_poll_fd(&ev->pfd[i]);
    }

    free(ev->pfd);
}

/** @brief Add a new connection to the list.
 *
 * The connection is added at the tail of the list.
 *
 * @param ev The event handler structure where the connection list is.
 * @param connection The connection to be added.
 */
static void mct_daemon_add_connection(MctEventHandler *ev,
                                          MctConnection *connection)
{

    MctConnection **temp = &ev->connections;

    while (*temp != NULL)
        temp = &(*temp)->next;

    *temp = connection;
}

/** @brief Check for connection activation
 *
 * If the connection is active and it's not allowed anymore or it the user
 * ask for deactivation, the connection will be deactivated.
 * If the connection is inactive, the user asks for activation and it's
 * allowed for it to be activated, the connection will be activated.
 *
 * @param evhdl The event handler structure.
 * @param con The connection to act on
 * @param filter The filter to know the actual allowance state
 * @param activation_type The type of activation requested ((DE)ACTIVATE)
 *
 * @return 0 on success, -1 otherwise
 */
int mct_connection_check_activate(MctEventHandler *evhdl,
                                  MctConnection *con,
                                  MctMessageFilter *filter,
                                  int activation_type)
{
    if (!evhdl || !con || !con->receiver || !filter) {
        mct_vlog(LOG_ERR, "%s: wrong parameters.\n", __func__);
        return -1;
    }

    switch (con->status) {
        case ACTIVE:

            if ((mct_daemon_filter_is_connection_allowed(filter, con->type) <= 0) ||
                (activation_type == DEACTIVATE)) {
                mct_vlog(LOG_INFO, "Deactivate connection type: %u\n", con->type);
                mct_event_handler_disable_fd(evhdl, con->receiver->fd);

                if (con->type == MCT_CONNECTION_CLIENT_CONNECT) {
                    con->receiver->fd = -1;
                }

                con->status = INACTIVE;
            }

            break;
        case INACTIVE:

            if ((mct_daemon_filter_is_connection_allowed(filter, con->type) > 0) &&
                (activation_type == ACTIVATE)) {
                mct_vlog(LOG_INFO, "Activate connection type: %u\n", con->type);
                mct_event_handler_enable_fd(evhdl, con->receiver->fd, con->ev_mask);
                con->status = ACTIVE;
            }

            break;
        default:
            mct_vlog(LOG_ERR, "Unknown connection status: %u\n", con->status);
            return -1;
    }

    return 0;
}

/** @brief Registers a connection for event handling and takes its ownership.
 *
 * As we add the connection to the list of connection, we take its ownership.
 * That's the only place where the connection pointer is stored.
 * The connection is then used to create a new event trigger.
 * If the connection is of type MCT_CONNECTION_CLIENT_MSG_TCP, we increase
 * the daemon_local->client_connections counter. TODO: Move this counter inside
 * the event handler structure.
 *
 * @param evhdl The event handler structure where the connection list is.
 * @param daemon_local Structure containing needed information.
 * @param connection The connection to be registered.
 * @param mask The bit mask of event to be registered.
 *
 * @return 0 on success, -1 otherwise.
 */
int mct_event_handler_register_connection(MctEventHandler *evhdl,
                                          MctDaemonLocal *daemon_local,
                                          MctConnection *connection,
                                          int mask)
{
    if (!evhdl || !connection || !connection->receiver) {
        mct_log(LOG_ERR, "Wrong parameters when registering connection.\n");
        return -1;
    }

    mct_daemon_add_connection(evhdl, connection);

    if ((connection->type == MCT_CONNECTION_CLIENT_MSG_TCP) ||
        (connection->type == MCT_CONNECTION_CLIENT_MSG_SERIAL)) {
        daemon_local->client_connections++;
    }

    /* On creation the connection is not active by default */
    connection->status = INACTIVE;

    connection->next = NULL;
    connection->ev_mask = mask;

    return mct_connection_check_activate(evhdl, connection,
                                         &daemon_local->pFilter, ACTIVATE);
}

/** @brief Unregisters a connection from the event handler and destroys it.
 *
 * We first look for the connection to be unregistered, delete the event
 * corresponding and then destroy the connection.
 * If the connection is of type MCT_CONNECTION_CLIENT_MSG_TCP, we decrease
 * the daemon_local->client_connections counter. TODO: Move this counter inside
 * the event handler structure.
 *
 * @param evhdl The event handler structure where the connection list is.
 * @param daemon_local Structure containing needed information.
 * @param fd The file descriptor of the connection to be unregistered.
 *
 * @return 0 on success, -1 otherwise.
 */
int mct_event_handler_unregister_connection(MctEventHandler *evhdl,
                                            MctDaemonLocal *daemon_local,
                                            int fd)
{
    if ((evhdl == NULL) || (daemon_local == NULL)) {
        return MCT_RETURN_ERROR;
    }

    /* Look for the pointer in the client list.
     * There shall be only one event handler with the same fd.
     */
    MctConnection *temp = mct_event_handler_find_connection(evhdl, fd);

    if (!temp) {
        mct_log(LOG_ERR, "Connection not found for unregistration.\n");
        return -1;
    }

    if ((temp->type == MCT_CONNECTION_CLIENT_MSG_TCP) ||
        (temp->type == MCT_CONNECTION_CLIENT_MSG_SERIAL)) {
        daemon_local->client_connections--;

        if (daemon_local->client_connections < 0) {
            daemon_local->client_connections = 0;
            mct_log(LOG_CRIT, "Unregistering more client than registered!\n");
        }
    }

    if (mct_connection_check_activate(evhdl, temp, &daemon_local->pFilter,
                                      DEACTIVATE) < 0) {
        mct_log(LOG_ERR, "Unable to unregister event.\n");
    }

    /* Cannot fail as far as mct_daemon_find_connection succeed */
    return mct_daemon_remove_connection(evhdl, temp);
}
