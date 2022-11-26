#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mct_daemon_connection_types.h"
#include "mct_daemon_connection.h"
#include "mct_daemon_event_handler_types.h"
#include "mct_daemon_event_handler.h"
#include "mct_daemon_filter.h"
#include "mct-daemon.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_common.h"
#include "mct_common.h"
#include "mct_daemon_socket.h"

static MctConnectionId connectionId;
extern char *app_recv_buffer;

/** @brief Generic sending function.
 *
 * We manage different type of connection which have similar send/write
 * functions. We can then abstract the data transfer using this function,
 * moreover as we often transfer data to different kind of connection
 * within the same loop.
 *
 * @param conn The connection structure.
 * @param msg The message buffer to be sent
 * @param msg_size The length of the message to be sent
 *
 * @return MCT_DAEMON_ERROR_OK on success, MCT_DAEMON_ERROR_SEND_FAILED
 *         on send failure, MCT_DAEMON_ERROR_UNKNOWN otherwise.
 *         errno is appropriately set.
 */
static int mct_connection_send(MctConnection *conn,
                                   void *msg,
                                   size_t msg_size)
{
    MctConnectionType type = MCT_CONNECTION_TYPE_MAX;
    int ret = 0;

    if ((conn != NULL) && (conn->receiver != NULL)) {
        type = conn->type;
    }

    switch (type) {
        case MCT_CONNECTION_CLIENT_MSG_SERIAL:

            if (write(conn->receiver->fd, msg, msg_size) > 0) {
                return MCT_DAEMON_ERROR_OK;
            }

            return MCT_DAEMON_ERROR_UNKNOWN;

        case MCT_CONNECTION_CLIENT_MSG_TCP:
            ret = mct_daemon_socket_sendreliable(conn->receiver->fd,
                                                 msg,
                                                 msg_size);
            return ret;
        default:
            return MCT_DAEMON_ERROR_UNKNOWN;
    }
}

/** @brief Send up to two messages through a connection.
 *
 * We often need to send 2 messages through a specific connection, plus
 * the serial header. This function groups these different calls.
 *
 * @param con The connection to send the messages through.
 * @param data1 The first message to be sent.
 * @param size1 The size of the first message.
 * @param data2 The second message to be send.
 * @param size2 The second message size.
 * @param sendserialheader Whether we need or not to send the serial header.
 *
 * @return MCT_DAEMON_ERROR_OK on success, -1 otherwise. errno is properly set.
 */
int mct_connection_send_multiple(MctConnection *con,
                                 void *data1,
                                 int size1,
                                 void *data2,
                                 int size2,
                                 int sendserialheader)
{
    int ret = 0;

    if (con == NULL) {
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    if (sendserialheader) {
        ret = mct_connection_send(con,
                                  (void *)mctSerialHeader,
                                  sizeof(mctSerialHeader));
    }

    if ((data1 != NULL) && (ret == MCT_RETURN_OK)) {
        ret = mct_connection_send(con, data1, size1);
    }

    if ((data2 != NULL) && (ret == MCT_RETURN_OK)) {
        ret = mct_connection_send(con, data2, size2);
    }

    return ret;
}

/** @brief Get the next connection filtered with a type mask.
 *
 * In some cases we need the next connection available of a specific type or
 * specific different types. This function returns the next available connection
 * that is of one of the types included in the mask. The current connection can
 * be returned.
 *
 * @param current The current connection pointer.
 * @param type_mask A bit mask representing the connection types to be filtered.
 *
 * @return The next available connection of the considered types or NULL.
 */
MctConnection *mct_connection_get_next(MctConnection *current, int type_mask)
{
    if (type_mask == MCT_CONNECTION_NONE) {
        return NULL;
    }

    while (current && !((1 << current->type) & type_mask))
        current = current->next;

    return current;
}

static void mct_connection_destroy_receiver(MctConnection *con)
{
    if (!con) {
        return;
    }

    switch (con->type) {
        case MCT_CONNECTION_GATEWAY:
            /* We rely on the gateway for clean-up */
            break;
        case MCT_CONNECTION_APP_MSG:
            mct_receiver_free_global_buffer(con->receiver);
            free(con->receiver);
            con->receiver = NULL;
            break;
        default:
            (void)mct_receiver_free(con->receiver);
            free(con->receiver);
            con->receiver = NULL;
            break;
    }
}

/** @brief Get the receiver structure associated to a connection.
 *
 * The receiver structure is sometimes needed while handling the event.
 * This behavior is mainly due to the fact that it's not intended to modify
 * the whole design of the daemon while implementing the new event handling.
 * Based on the connection type provided, this function returns the pointer
 * to the MctReceiver structure corresponding.
 *
 * @param daemon_local Structure where to take the MctReceiver pointer from.
 * @param type Type of the connection.
 * @param fd File descriptor
 *
 * @return MctReceiver structure or NULL if none corresponds to the type.
 */
static MctReceiver *mct_connection_get_receiver(MctConnectionType type,
                                                    int fd)
{
    MctReceiver *ret = NULL;
    MctReceiverType receiver_type = MCT_RECEIVE_FD;
    struct stat statbuf;

    switch (type) {
        case MCT_CONNECTION_CONTROL_CONNECT:
        /* FALL THROUGH */
        case MCT_CONNECTION_CONTROL_MSG:
        /* FALL THROUGH */
        case MCT_CONNECTION_CLIENT_CONNECT:
        /* FALL THROUGH */
        case MCT_CONNECTION_FILTER:
        /* FALL THROUGH */
        case MCT_CONNECTION_CLIENT_MSG_TCP:
            ret = calloc(1, sizeof(MctReceiver));

            if (ret) {
                mct_receiver_init(ret, fd, MCT_RECEIVE_SOCKET, MCT_DAEMON_RCVBUFSIZESOCK);
            }

            break;
        case MCT_CONNECTION_CLIENT_MSG_SERIAL:
            ret = calloc(1, sizeof(MctReceiver));

            if (ret) {
                mct_receiver_init(ret, fd, MCT_RECEIVE_FD, MCT_DAEMON_RCVBUFSIZESERIAL);
            }

            break;
        case MCT_CONNECTION_APP_MSG:
            ret = calloc(1, sizeof(MctReceiver));

            receiver_type = MCT_RECEIVE_FD;

            if (fstat(fd, &statbuf) == 0) {
                if (S_ISSOCK(statbuf.st_mode)) {
                    receiver_type = MCT_RECEIVE_SOCKET;
                }
            } else {
                mct_vlog(
                    LOG_WARNING,
                    "Failed to determine receive type for MCT_CONNECTION_APP_MSG, using \"FD\"\n");
            }

            if (ret) {
                mct_receiver_init_global_buffer(ret, fd, receiver_type, &app_recv_buffer);
            }

            break;
#if defined MCT_DAEMON_USE_UNIX_SOCKET_IPC
        case MCT_CONNECTION_APP_CONNECT:
            /* FALL THROUGH */
#endif
        case MCT_CONNECTION_ONE_S_TIMER:
        /* FALL THROUGH */
        case MCT_CONNECTION_SIXTY_S_TIMER:
        default:
            ret = NULL;
    }

    return ret;
}

/** @brief Get the callback from a specific connection.
 *
 * The callback retrieved that way is used to handle event for this connection.
 * It as been chosen to proceed that way instead of having the callback directly
 * in the structure in order to have some way to check that the structure is
 * still valid, or at least gracefully handle errors instead of crashing.
 *
 * @param con The connection to retrieve the callback from.
 *
 * @return Function pointer or NULL.
 */
void *mct_connection_get_callback(MctConnection *con)
{
    void *ret = NULL;
    MctConnectionType type = MCT_CONNECTION_TYPE_MAX;

    if (con) {
        type = con->type;
    }

    switch (type) {
        case MCT_CONNECTION_CLIENT_CONNECT:
            ret = mct_daemon_process_client_connect;
            break;
        case MCT_CONNECTION_CLIENT_MSG_TCP:
            ret = mct_daemon_process_client_messages;
            break;
        case MCT_CONNECTION_CLIENT_MSG_SERIAL:
            ret = mct_daemon_process_client_messages_serial;
            break;
#if defined MCT_DAEMON_USE_UNIX_SOCKET_IPC
        case MCT_CONNECTION_APP_CONNECT:
            ret = mct_daemon_process_app_connect;
            break;
#endif
        case MCT_CONNECTION_APP_MSG:
            ret = mct_daemon_process_user_messages;
            break;
        case MCT_CONNECTION_ONE_S_TIMER:
            ret = mct_daemon_process_one_s_timer;
            break;
        case MCT_CONNECTION_SIXTY_S_TIMER:
            ret = mct_daemon_process_sixty_s_timer;
            break;
#ifdef MCT_SYSTEMD_WATCHDOG_ENABLE
        case MCT_CONNECTION_SYSTEMD_TIMER:
            ret = mct_daemon_process_systemd_timer;
            break;
#endif
        case MCT_CONNECTION_CONTROL_CONNECT:
            ret = mct_daemon_process_control_connect;
            break;
        case MCT_CONNECTION_CONTROL_MSG:
            ret = mct_daemon_process_control_messages;
            break;
        case MCT_CONNECTION_FILTER:
            ret = mct_daemon_filter_process_filter_control_messages;
            break;
        default:
            ret = NULL;
    }

    return ret;
}

/** @brief Destroys a connection.
 *
 * This function closes and frees the corresponding connection. This is expected
 * to be called by the connection owner: the MctEventHandler.
 * Ownership of the connection is given during the registration to
 * the MctEventHandler.
 *
 * @param to_destroy Connection to be destroyed.
 */
void mct_connection_destroy(MctConnection *to_destroy)
{
    to_destroy->id = 0;
    close(to_destroy->receiver->fd);
    mct_connection_destroy_receiver(to_destroy);
    free(to_destroy);
}

/** @brief Creates a connection and registers it to the MctEventHandler.
 *
 * The function will allocate memory for the connection, and give the pointer
 * to the MctEventHandler in order to register it for incoming events.
 * The connection is then destroyed later on, once it's not needed anymore or
 * it the event handler is destroyed.
 *
 * @param daemon_local Structure were some needed information is.
 * @param evh MctEventHandler to register the connection to.
 * @param fd File descriptor of the connection.
 * @param mask Event list bit mask.
 * @param type Connection type.
 *
 * @return 0 On success, -1 otherwise.
 */
int mct_connection_create(MctDaemonLocal *daemon_local,
                          MctEventHandler *evh,
                          int fd,
                          int mask,
                          MctConnectionType type)
{
    MctConnection *temp = NULL;

    if (fd < 0) {
        /* Nothing to do */
        return 0;
    }

    if (mct_event_handler_find_connection(evh, fd) != NULL) {
        /* No need for the same client to be registered twice
         * for the same event.
         * TODO: If another mask can be expected,
         * we need it to update the poll event here.
         */
        return 0;
    }

    temp = (MctConnection *)malloc(sizeof(MctConnection));

    if (temp == NULL) {
        mct_log(LOG_CRIT, "Allocation of client handle failed\n");
        return -1;
    }

    memset(temp, 0, sizeof(MctConnection));

    temp->receiver = mct_connection_get_receiver(type, fd);

    if (!temp->receiver) {
        mct_vlog(LOG_CRIT, "Unable to get receiver from %u connection.\n",
                 type);
        free(temp);
        return -1;
    }

    /* We are single threaded no need for protection. */
    temp->id = connectionId++;

    if (!temp->id) {
        /* Skipping 0 */
        temp->id = connectionId++;
    }

    temp->type = type;
    temp->status = ACTIVE;

    /* Now give the ownership of the newly created connection
     * to the event handler, by registering for events.
     */
    return mct_event_handler_register_connection(evh, daemon_local, temp, mask);
}
