#include <netdb.h>
#include <ctype.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), (), and recv() */
#include <sys/stat.h>   /* for stat() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>

#ifdef linux
#include <sys/timerfd.h>
#endif
#include <sys/time.h>
#if defined(linux) && defined(__NR_statx)
#include <linux/stat.h>
#endif

#include "mct_types.h"
#include "mct-daemon.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_common_cfg.h"
#include "mct_protocol.h"

#include "mct_daemon_socket.h"
#include "mct_daemon_serial.h"

#include "mct_daemon_client.h"
#include "mct_daemon_connection.h"
#include "mct_daemon_filter.h"
#include "mct_daemon_event_handler.h"

#include "mct_daemon_offline_logstorage.h"

/** Inline function to calculate/set the requested log level or traces status
 *  with default log level or trace status when "ForceContextLogLevelAndTraceStatus"
 *  is enabled and set to 1 in mct.conf file.
 *
 * @param request_log The requested log level (or) trace status
 * @param context_log The default log level (or) trace status
 *
 * @return The log level if requested log level is lower or equal to ContextLogLevel
 */
static inline int8_t getStatus(uint8_t request_log, int context_log)
{
    return (request_log <= context_log) ? request_log : context_log;
}


/** @brief Sends up to 2 messages to all the clients.
 *
 * Runs through the client list and sends the messages to them. If the message
 * transfer fails and the connection is a socket connection, the socket is closed.
 * Takes and release mct_daemon_mutex.
 *
 * @param daemon Daemon structure needed for socket closure.
 * @param daemon_local Daemon local structure
 * @param data1 The first message to be sent.
 * @param size1 The size of the first message.
 * @param data2 The second message to be send.
 * @param size2 The second message size.
 * @param verbose Needed for socket closure.
 *
 * @return The amount of data transfered.
 */
static int mct_daemon_client_send_all_multiple(MctDaemon *daemon,
                                               MctDaemonLocal *daemon_local,
                                               void *data1,
                                               int size1,
                                               void *data2,
                                               int size2,
                                               int verbose)
{
    int sent = 0;
    unsigned int i = 0;
    int ret = 0;
    MctConnection *temp = NULL;
    int type_mask = MCT_CONNECTION_NONE;
    int client_mask = MCT_FILTER_CLIENT_CONNECTION_DEFAULT_MASK;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return 0;
    }

    /* get current client mask to avoid multiple function calls */
    client_mask |= daemon_local->pFilter.current->client_mask;

    /* check for TCP and Serial if allowed in current filter configuration*/
    if (client_mask & MCT_CONNECTION_TO_MASK(MCT_CONNECTION_CLIENT_MSG_TCP)) {
        type_mask |= MCT_CON_MASK_CLIENT_MSG_TCP;
    }

    if (client_mask & MCT_CONNECTION_TO_MASK(MCT_CONNECTION_CLIENT_MSG_SERIAL)) {
        type_mask |= MCT_CON_MASK_CLIENT_MSG_SERIAL;
    }

    for (i = 0; i < daemon_local->pEvent.nfds; i++) {
        temp = mct_event_handler_find_connection(&(daemon_local->pEvent),
                                                 daemon_local->pEvent.pfd[i].fd);

        if ((temp == NULL) || (temp->receiver == NULL) ||
            !((1 << temp->type) & type_mask)) {
            mct_log(LOG_DEBUG, "The connection not found or the connection type not TCP/Serial.\n");
            continue;
        }

        ret = mct_connection_send_multiple(temp,
                                           data1,
                                           size1,
                                           data2,
                                           size2,
                                           daemon->sendserialheader);

        if ((ret != MCT_DAEMON_ERROR_OK) &&
            (MCT_CONNECTION_CLIENT_MSG_TCP == temp->type)) {
            mct_daemon_close_socket(temp->receiver->fd,
                                    daemon,
                                    daemon_local,
                                    verbose);
        }

        if (ret != MCT_DAEMON_ERROR_OK) {
            mct_vlog(LOG_WARNING, "%s: send mct message failed\n", __func__);
        } else {
            /* If sent to at  least one client,
             * then do not store in ring buffer
             */
            sent = 1;
        }
    } /* for */

    return sent;
}

int mct_daemon_client_send(int sock,
                           MctDaemon *daemon,
                           MctDaemonLocal *daemon_local,
                           void *storage_header,
                           int storage_header_size,
                           void *data1,
                           int size1,
                           void *data2,
                           int size2,
                           int verbose)
{
    int sent, ret;
    int ret_logstorage = 0;
    int client_mask = MCT_FILTER_CLIENT_CONNECTION_DEFAULT_MASK;
    static int sent_message_overflow_cnt = 0;

    if ((daemon == NULL) || (daemon_local == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid arguments\n", __func__);
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* get current client mask to avoid multiple function calls */
    client_mask |= daemon_local->pFilter.current->client_mask;

    if ((sock != MCT_DAEMON_SEND_TO_ALL) && (sock != MCT_DAEMON_SEND_FORCE)) {
        /* FIXME:
         * maybe we need to check here as well if the sock belongs to an allowed
         * connection or not. Currently, the MCT Viewer receives some daemon
         * status messages also in states where no log messages are received */

        /* Send message to specific socket */
        if (isatty(sock)) {
            if ((ret =
                     mct_daemon_serial_send(sock, data1, size1, data2, size2,
                                            daemon->sendserialheader))) {
                mct_vlog(LOG_WARNING, "%s: serial send mct message failed\n", __func__);
                return ret;
            }
        } else {
            if ((ret =
                     mct_daemon_socket_send(sock, data1, size1, data2, size2,
                                            daemon->sendserialheader))) {
                mct_vlog(LOG_WARNING, "%s: socket send mct message failed\n", __func__);
                return ret;
            }
        }

        return MCT_DAEMON_ERROR_OK;
    }

    /* write message to offline trace */
    /* In the SEND_BUFFER state we must skip offline tracing because the offline traces */
    /* are going without buffering directly to the offline trace. Thus we have to filter out */
    /* the traces that are coming from the buffer. */
    if ((sock != MCT_DAEMON_SEND_FORCE) && (daemon->state != MCT_DAEMON_STATE_SEND_BUFFER)) {
        if ((client_mask & MCT_CONNECTION_TO_MASK(MCT_CONNECTION_CLIENT_MSG_OFFLINE_TRACE)) &&
            daemon_local->flags.offlineTraceDirectory[0]) {
            if (mct_offline_trace_write(&(daemon_local->offlineTrace), storage_header,
                                        storage_header_size, data1,
                                        size1, data2, size2)) {
                static int error_mct_offline_trace_write_failed = 0;

                if (!error_mct_offline_trace_write_failed) {
                    mct_vlog(LOG_ERR, "%s: mct_offline_trace_write failed!\n", __func__);
                    error_mct_offline_trace_write_failed = 1;
                }

                /*return MCT_DAEMON_ERROR_WRITE_FAILED; */
            }
        }

        /* write messages to offline logstorage only if there is an extended header set
         * this need to be checked because the function is mct_daemon_client_send is called by
         * newly introduced mct_daemon_log_internal */
        if ((client_mask & MCT_CONNECTION_TO_MASK(MCT_CONNECTION_CLIENT_MSG_OFFLINE_LOGSTORAGE)) &&
            (daemon_local->flags.offlineLogstorageMaxDevices > 0)) {
            ret_logstorage = mct_daemon_logstorage_write(daemon,
                                                         &daemon_local->flags,
                                                         storage_header,
                                                         storage_header_size,
                                                         data1,
                                                         size1,
                                                         data2,
                                                         size2);
        }
    }

    /* send messages to daemon socket */
    if ((client_mask & MCT_CONNECTION_TO_MASK(MCT_CONNECTION_CLIENT_MSG_TCP)) ||
        (client_mask & MCT_CONNECTION_TO_MASK(MCT_CONNECTION_CLIENT_MSG_SERIAL))) {

        if ((sock == MCT_DAEMON_SEND_FORCE) || (daemon->state == MCT_DAEMON_STATE_SEND_DIRECT)) {
            /* Forward message to network client if network routing is not disabled */
            if (ret_logstorage != 1) {
                sent = mct_daemon_client_send_all_multiple(daemon,
                                                           daemon_local,
                                                           data1,
                                                           size1,
                                                           data2,
                                                           size2,
                                                           verbose);

                if ((sock == MCT_DAEMON_SEND_FORCE) && !sent) {
                    return MCT_DAEMON_ERROR_SEND_FAILED;
                }
            }
        }
    }

    /* Message was not sent to client, so store it in client ringbuffer */
    if ((sock != MCT_DAEMON_SEND_FORCE) &&
        ((daemon->state == MCT_DAEMON_STATE_BUFFER) ||
         (daemon->state == MCT_DAEMON_STATE_SEND_BUFFER) ||
         (daemon->state == MCT_DAEMON_STATE_BUFFER_FULL))) {
        if (daemon->state != MCT_DAEMON_STATE_BUFFER_FULL) {
            /* Store message in history buffer */
            ret = mct_buffer_push3(&(daemon->client_ringbuffer), data1, size1, data2, size2, 0, 0);
            if (ret < MCT_RETURN_OK) {
                mct_daemon_change_state(daemon, MCT_DAEMON_STATE_BUFFER_FULL);
            }
        }

        if (daemon->state == MCT_DAEMON_STATE_BUFFER_FULL) {
            daemon->overflow_counter += 1;

            if (daemon->overflow_counter == 1) {
                mct_vlog(LOG_INFO, "%s: Buffer is full! Messages will be discarded.\n", __func__);
            }

            return MCT_DAEMON_ERROR_BUFFER_FULL;
        }
    } else {
        if ((daemon->overflow_counter > 0) &&
            (daemon_local->client_connections > 0)) {
            sent_message_overflow_cnt++;
            if (sent_message_overflow_cnt >= 2) {
                sent_message_overflow_cnt--;
            }
            else {
                if (mct_daemon_send_message_overflow(daemon, daemon_local,
                                          verbose) == MCT_DAEMON_ERROR_OK) {
                    mct_vlog(LOG_WARNING,
                             "%s: %u messages discarded! Now able to send messages to the client.\n",
                             __func__,
                             daemon->overflow_counter);
                    daemon->overflow_counter = 0;
                    sent_message_overflow_cnt--;
                }
            }
        }
    }

    return MCT_DAEMON_ERROR_OK;
}

int mct_daemon_client_send_message_to_all_client(MctDaemon *daemon,
                                                 MctDaemonLocal *daemon_local,
                                                 int verbose)
{
    static char text[MCT_DAEMON_TEXTSIZE];
    char *ecu_ptr = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL)) {
        mct_vlog(LOG_ERR, "%s: invalid arguments\n", __func__);
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* set overwrite ecu id */
    if ((daemon_local->flags.evalue[0]) &&
        (strncmp(daemon_local->msg.headerextra.ecu,
                 MCT_DAEMON_ECU_ID, MCT_ID_SIZE) == 0)) {
        /* Set header extra parameters */
        mct_set_id(daemon_local->msg.headerextra.ecu, daemon->ecuid);

        /*msg.headerextra.seid = 0; */
        if (mct_message_set_extraparameters(&(daemon_local->msg), 0)) {
            mct_vlog(LOG_WARNING,
                     "%s: failed to set message extra parameters.\n", __func__);
            return MCT_DAEMON_ERROR_UNKNOWN;
        }

        /* Correct value of timestamp, this was changed by mct_message_set_extraparameters() */
        daemon_local->msg.headerextra.tmsp =
            MCT_BETOH_32(daemon_local->msg.headerextra.tmsp);
    }

    /* prepare storage header */
    if (MCT_IS_HTYP_WEID(daemon_local->msg.standardheader->htyp)) {
        ecu_ptr = daemon_local->msg.headerextra.ecu;
    } else {
        ecu_ptr = daemon->ecuid;
    }

    if (mct_set_storageheader(daemon_local->msg.storageheader, ecu_ptr)) {
        mct_vlog(LOG_WARNING,
                 "%s: failed to set storage header with header type: 0x%x\n",
                 __func__, daemon_local->msg.standardheader->htyp);
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* if no filter set or filter is matching display message */
    if (daemon_local->flags.xflag) {
        if (MCT_RETURN_OK !=
            mct_message_print_hex(&(daemon_local->msg), text,
                                  MCT_DAEMON_TEXTSIZE, verbose)) {
            mct_log(LOG_WARNING, "mct_message_print_hex() failed!\n");
        }
    } else if (daemon_local->flags.aflag) {
        if (MCT_RETURN_OK !=
            mct_message_print_ascii(&(daemon_local->msg), text,
                                    MCT_DAEMON_TEXTSIZE, verbose)) {
            mct_log(LOG_WARNING, "mct_message_print_ascii() failed!\n");
        }
    } else if (daemon_local->flags.sflag) {
        if (MCT_RETURN_OK !=
            mct_message_print_header(&(daemon_local->msg), text,
                                     MCT_DAEMON_TEXTSIZE, verbose)) {
            mct_log(LOG_WARNING, "mct_message_print_header() failed!\n");
        }
    }

    /* send message to client or write to log file */
    return mct_daemon_client_send(MCT_DAEMON_SEND_TO_ALL, daemon, daemon_local,
                                  daemon_local->msg.headerbuffer, sizeof(MctStorageHeader),
                                  daemon_local->msg.headerbuffer + sizeof(MctStorageHeader),
                                  daemon_local->msg.headersize - sizeof(MctStorageHeader),
                                  daemon_local->msg.databuffer, daemon_local->msg.datasize, verbose);
}

int mct_daemon_client_send_control_message(int sock,
                                           MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctMessage *msg,
                                           char *apid,
                                           char *ctid,
                                           int verbose)
{
    int ret;
    int32_t len;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == 0) || (msg == 0) || (apid == 0) || (ctid == 0)) {
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* prepare storage header */
    msg->storageheader = (MctStorageHeader *)msg->headerbuffer;

    if (mct_set_storageheader(msg->storageheader, daemon->ecuid) == MCT_RETURN_ERROR) {
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* prepare standard header */
    msg->standardheader = (MctStandardHeader *)(msg->headerbuffer + sizeof(MctStorageHeader));
    msg->standardheader->htyp = MCT_HTYP_WEID | MCT_HTYP_WTMS | MCT_HTYP_UEH |
        MCT_HTYP_PROTOCOL_VERSION1;

#if (BYTE_ORDER == BIG_ENDIAN)
    msg->standardheader->htyp = (msg->standardheader->htyp | MCT_HTYP_MSBF);
#endif

    msg->standardheader->mcnt = 0;

    /* Set header extra parameters */
    mct_set_id(msg->headerextra.ecu, daemon->ecuid);

    /*msg->headerextra.seid = 0; */

    msg->headerextra.tmsp = mct_uptime();

    mct_message_set_extraparameters(msg, verbose);

    /* prepare extended header */
    msg->extendedheader =
        (MctExtendedHeader *)(msg->headerbuffer + sizeof(MctStorageHeader) +
                              sizeof(MctStandardHeader) +
                              MCT_STANDARD_HEADER_EXTRA_SIZE(msg->standardheader->htyp));
    msg->extendedheader->msin = MCT_MSIN_CONTROL_RESPONSE;

    msg->extendedheader->noar = 1; /* number of arguments */

    if (strcmp(apid, "") == 0) {
        mct_set_id(msg->extendedheader->apid, MCT_DAEMON_CTRL_APID);       /* application id */
    } else {
        mct_set_id(msg->extendedheader->apid, apid);
    }

    if (strcmp(ctid, "") == 0) {
        mct_set_id(msg->extendedheader->ctid, MCT_DAEMON_CTRL_CTID);       /* context id */
    } else {
        mct_set_id(msg->extendedheader->ctid, ctid);
    }

    /* prepare length information */
    msg->headersize = sizeof(MctStorageHeader) + sizeof(MctStandardHeader) +
        sizeof(MctExtendedHeader) +
        MCT_STANDARD_HEADER_EXTRA_SIZE(msg->standardheader->htyp);

    len = msg->headersize - sizeof(MctStorageHeader) + msg->datasize;

    if (len > UINT16_MAX) {
        mct_log(LOG_WARNING, "Huge control message discarded!\n");
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    msg->standardheader->len = MCT_HTOBE_16(((uint16_t)len));

    if ((ret =
             mct_daemon_client_send(sock, daemon, daemon_local, msg->headerbuffer,
                                    sizeof(MctStorageHeader),
                                    msg->headerbuffer + sizeof(MctStorageHeader),
                                    msg->headersize - sizeof(MctStorageHeader),
                                    msg->databuffer, msg->datasize, verbose))) {
        mct_log(LOG_DEBUG,
                "mct_daemon_control_send_control_message: MCT message send to all failed!.\n");
        return ret;
    }

    return MCT_DAEMON_ERROR_OK;
}

int mct_daemon_client_process_control(int sock,
                                      MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      MctMessage *msg,
                                      int verbose)
{
    uint32_t id, id_tmp = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (msg == NULL)) {
        return -1;
    }

    if (msg->datasize < (int32_t)sizeof(uint32_t)) {
        return -1;
    }

    MctConnection *con =
        mct_event_handler_find_connection(&daemon_local->pEvent, sock);

    if (con == NULL) {
        /* this definitively should not happen */
        mct_log(LOG_ERR, "Unexpected nullptr when searching for connection\n");
        return -1;
    }

    id_tmp = *((uint32_t *)(msg->databuffer));
    id = MCT_ENDIAN_GET_32(msg->standardheader->htyp, id_tmp);

    if ((id > MCT_SERVICE_ID) && (id < MCT_SERVICE_ID_CALLSW_CINJECTION)) {
        if (mct_daemon_filter_is_control_allowed(&daemon_local->pFilter, id) <= 0) {
            /* in case of a filter level change message and no backend is
             * configured and request comes from control socket, allow handling
             * of message even if not explicitly configured in filter
             * configuration.
             * Otherwise a filter level change deadlock occurs which only can
             * resolved by restarting the daemon which is not intended behavior.
             * Reading the level shall always be allowed from control socket.
             */
            /* TODO: Refactor and move to is_control_allowed after additional */
            /* requirements are clarified */
            if ((con->type == MCT_CONNECTION_CONTROL_MSG) &&
                (((daemon_local->pFilter.backend == NULL) &&
                  (id == MCT_SERVICE_ID_SET_FILTER_LEVEL)) ||
                 (id == MCT_SERVICE_ID_GET_FILTER_STATUS))) {
                mct_vlog(LOG_INFO, "Set Filter Level request received\n");
            } else {
                mct_vlog(LOG_WARNING,
                         "Received control message not permitted: %s\n",
                         mct_get_service_name(id));

                mct_daemon_control_service_response(sock, daemon, daemon_local,
                                                    id, MCT_SERVICE_RESPONSE_PERM_DENIED, verbose);

                return 0;
            }
        }

        /* Control message handling */
        switch (id) {
            case MCT_SERVICE_ID_SET_LOG_LEVEL:
            {
                mct_daemon_control_set_log_level(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_TRACE_STATUS:
            {
                mct_daemon_control_set_trace_status(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_GET_LOG_INFO:
            {
                mct_daemon_control_get_log_info(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL:
            {
                mct_daemon_control_get_default_log_level(sock, daemon, daemon_local, verbose);
                break;
            }
            case MCT_SERVICE_ID_STORE_CONFIG:
            {
                if (mct_daemon_applications_save(daemon, daemon->runtime_application_cfg,
                                                 verbose) == 0) {
                    if (mct_daemon_contexts_save(daemon, daemon->runtime_context_cfg,
                                                 verbose) == 0) {
                        mct_daemon_control_service_response(sock,
                                                            daemon,
                                                            daemon_local,
                                                            id,
                                                            MCT_SERVICE_RESPONSE_OK,
                                                            verbose);
                    } else {
                        /* Delete saved files */
                        mct_daemon_control_reset_to_factory_default(
                            daemon,
                            daemon->runtime_application_cfg,
                            daemon->runtime_context_cfg,
                            daemon_local->flags.
                            contextLogLevel,
                            daemon_local->flags.
                            contextTraceStatus,
                            daemon_local->flags.
                            enforceContextLLAndTS,
                            verbose);
                        mct_daemon_control_service_response(sock,
                                                            daemon,
                                                            daemon_local,
                                                            id,
                                                            MCT_SERVICE_RESPONSE_ERROR,
                                                            verbose);
                    }
                } else {
                    mct_daemon_control_service_response(sock,
                                                        daemon,
                                                        daemon_local,
                                                        id,
                                                        MCT_SERVICE_RESPONSE_ERROR,
                                                        verbose);
                }

                break;
            }
            case MCT_SERVICE_ID_RESET_TO_FACTORY_DEFAULT:
            {
                mct_daemon_control_reset_to_factory_default(
                    daemon,
                    daemon->runtime_application_cfg,
                    daemon->runtime_context_cfg,
                    daemon_local->flags.contextLogLevel,
                    daemon_local->flags.contextTraceStatus,
                    daemon_local->flags.
                    enforceContextLLAndTS,
                    verbose);
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_OK,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_COM_INTERFACE_STATUS:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_COM_INTERFACE_MAX_BANDWIDTH:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_VERBOSE_MODE:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_MESSAGE_FILTERING:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_TIMING_PACKETS:
            {
                mct_daemon_control_set_timing_packets(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_GET_LOCAL_TIME:
            {
                /* Send response with valid timestamp (TMSP) field */
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_OK,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_USE_ECU_ID:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_USE_SESSION_ID:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_USE_TIMESTAMP:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_USE_EXTENDED_HEADER:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_DEFAULT_LOG_LEVEL:
            {
                mct_daemon_control_set_default_log_level(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_DEFAULT_TRACE_STATUS:
            {
                mct_daemon_control_set_default_trace_status(sock,
                                                            daemon,
                                                            daemon_local,
                                                            msg,
                                                            verbose);
                break;
            }
            case MCT_SERVICE_ID_GET_SOFTWARE_VERSION:
            {
                mct_daemon_control_get_software_version(sock, daemon, daemon_local, verbose);
                break;
            }
            case MCT_SERVICE_ID_MESSAGE_BUFFER_OVERFLOW:
            {
                mct_daemon_control_message_buffer_overflow(sock,
                                                           daemon,
                                                           daemon_local,
                                                           daemon->overflow_counter,
                                                           "",
                                                           verbose);
                break;
            }
            case MCT_SERVICE_ID_OFFLINE_LOGSTORAGE:
            {
                mct_daemon_control_service_logstorage(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_FILTER_LEVEL:
            {
                mct_daemon_control_set_filter_level(sock, daemon, daemon_local,
                                                    msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_GET_FILTER_STATUS:
            {
                mct_daemon_control_get_filter_status(sock, daemon, daemon_local,
                                                     verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_ALL_LOG_LEVEL:
            {
                mct_daemon_control_set_all_log_level(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            case MCT_SERVICE_ID_SET_ALL_TRACE_STATUS:
            {
                mct_daemon_control_set_all_trace_status(sock, daemon, daemon_local, msg, verbose);
                break;
            }
            default:
            {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    id,
                                                    MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                                    verbose);
                break;
            }
        }
    } else {
        /* Injection handling */
        /* We need to inspect the message itself to decide if an injection
         * message is allowed or not. To not do it twice, the check if the
         * injection will be handled or discarded is done inside the function.
         */
        mct_daemon_control_callsw_cinjection(sock, daemon, daemon_local, msg, verbose);
    }

    return 0;
}

void mct_daemon_control_get_software_version(int sock,
                                             MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             int verbose)
{
    MctMessage msg;
    uint32_t len;
    MctServiceGetSoftwareVersionResponse *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_SOFTWARE_VERSION,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    /* prepare payload of data */
    len = strlen(daemon->ECUVersionString);

    /* msg.datasize = sizeof(serviceID) + sizeof(status) + sizeof(length) + len */
    msg.datasize = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + len;

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_SOFTWARE_VERSION,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    resp = (MctServiceGetSoftwareVersionResponse *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_GET_SOFTWARE_VERSION;
    resp->status = MCT_SERVICE_RESPONSE_OK;
    resp->length = len;
    memcpy(msg.databuffer + msg.datasize - len, daemon->ECUVersionString, len);

    /* send message */
    mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "", verbose);

    /* free message */
    mct_message_free(&msg, 0);
}

void mct_daemon_control_get_default_log_level(int sock,
                                              MctDaemon *daemon,
                                              MctDaemonLocal *daemon_local,
                                              int verbose)
{
    MctMessage msg;
    MctServiceGetDefaultLogLevelResponse *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    msg.datasize = sizeof(MctServiceGetDefaultLogLevelResponse);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    resp = (MctServiceGetDefaultLogLevelResponse *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL;
    resp->status = MCT_SERVICE_RESPONSE_OK;
    resp->log_level = daemon->default_log_level;

    /* send message */
    mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "", verbose);

    /* free message */
    mct_message_free(&msg, 0);
}

void mct_daemon_control_get_log_info(int sock,
                                     MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctMessage *msg,
                                     int verbose)
{
    MctServiceGetLogInfoRequest *req;
    MctMessage resp;
    MctDaemonContext *context = 0;
    MctDaemonApplication *application = 0;

    int num_applications = 0, num_contexts = 0;
    uint16_t count_app_ids = 0, count_con_ids = 0;

#if (MCT_DEBUG_GETLOGINFO == 1)
    char buf[255];
#endif

    int32_t i, j, offset = 0;
    char *apid = 0;
    int8_t ll, ts;
    uint16_t len;
    int8_t value;
    int32_t sizecont = 0;
    int offset_base;

    uint32_t sid;

    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceGetLogInfoRequest)) < 0) {
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    /* prepare pointer to message request */
    req = (MctServiceGetLogInfoRequest *)(msg->databuffer);

    /* initialise new message */
    if (mct_message_init(&resp, 0) == MCT_RETURN_ERROR) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_LOG_INFO,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    /* check request */
    if ((req->options < 3) || (req->options > 7)) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_LOG_INFO,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    if (req->apid[0] != '\0') {
        application = mct_daemon_application_find(daemon,
                                                  req->apid,
                                                  daemon->ecuid,
                                                  verbose);

        if (application) {
            num_applications = 1;

            if (req->ctid[0] != '\0') {
                context = mct_daemon_context_find(daemon,
                                                  req->apid,
                                                  req->ctid,
                                                  daemon->ecuid,
                                                  verbose);

                num_contexts = ((context) ? 1 : 0);
            } else {
                num_contexts = application->num_contexts;
            }
        } else {
            num_applications = 0;
            num_contexts = 0;
        }
    } else {
        /* Request all applications and contexts */
        num_applications = user_list->num_applications;
        num_contexts = user_list->num_contexts;
    }

    /* prepare payload of data */

    /* Calculate maximum size for a response */
    resp.datasize = sizeof(uint32_t) /* SID */ + sizeof(int8_t) /* status*/ + sizeof(ID4) /* MCT_DAEMON_REMO_STRING */;

    sizecont = sizeof(uint32_t) /* context_id */;

    /* Add additional size for response of Mode 4, 6, 7 */
    if ((req->options == 4) || (req->options == 6) || (req->options == 7)) {
        sizecont += sizeof(int8_t); /* log level */
    }

    /* Add additional size for response of Mode 5, 6, 7 */
    if ((req->options == 5) || (req->options == 6) || (req->options == 7)) {
        sizecont += sizeof(int8_t); /* trace status */
    }

    resp.datasize +=
        (num_applications *
         (sizeof(uint32_t) /* app_id */ + sizeof(uint16_t) /* count_con_ids */)) +
        (num_contexts * sizecont);

    resp.datasize += sizeof(uint16_t) /* count_app_ids */;

    /* Add additional size for response of Mode 7 */
    if (req->options == 7) {
        if (req->apid[0] != '\0') {
            if (req->ctid[0] != '\0') {
                /* One application, one context */
                /* context = mct_daemon_context_find(daemon, req->apid, req->ctid, verbose); */
                if (context) {
                    resp.datasize += sizeof(uint16_t) /* len_context_description */;

                    if (context->context_description != 0) {
                        resp.datasize += strlen(context->context_description); /* context_description */
                    }
                }
            } else if ((user_list->applications) && (application)) {
                /* One application, all contexts */
                /* Calculate start offset within contexts[] */
                offset_base = 0;

                for (i = 0; i < (application - (user_list->applications)); i++) {
                    offset_base += user_list->applications[i].num_contexts;
                }

                /* Iterate over all contexts belonging to this application */
                for (j = 0; j < application->num_contexts; j++) {

                    context = &(user_list->contexts[offset_base + j]);

                    if (context) {
                        resp.datasize += sizeof(uint16_t) /* len_context_description */;

                        if (context->context_description != 0) {
                            resp.datasize += strlen(context->context_description);   /* context_description */
                        }
                    }
                }
            }

            /* Space for application description */
            if (application) {
                resp.datasize += sizeof(uint16_t) /* len_app_description */;

                if (application->application_description != 0) {
                    resp.datasize += strlen(application->application_description); /* app_description */
                }
            }
        } else {
            /* All applications, all contexts */
            for (i = 0; i < user_list->num_contexts; i++) {
                resp.datasize += sizeof(uint16_t) /* len_context_description */;

                if (user_list->contexts[i].context_description != 0) {
                    resp.datasize +=
                        strlen(user_list->contexts[i].context_description);
                }
            }

            for (i = 0; i < user_list->num_applications; i++) {
                resp.datasize += sizeof(uint16_t) /* len_app_description */;

                if (user_list->applications[i].application_description != 0) {
                    resp.datasize += strlen(user_list->applications[i].application_description); /* app_description */
                }
            }
        }
    }

    if (verbose) {
        mct_vlog(LOG_DEBUG,
                 "Allocate %u bytes for response msg databuffer\n",
                 resp.datasize);
    }

    /* Allocate buffer for response message */
    resp.databuffer = (uint8_t *)malloc(resp.datasize);
    resp.databuffersize = resp.datasize;

    if (resp.databuffer == 0) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_GET_LOG_INFO,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return;
    }

    memset(resp.databuffer, 0, resp.datasize);
    /* Preparation finished */

    /* Prepare response */
    sid = MCT_SERVICE_ID_GET_LOG_INFO;
    memcpy(resp.databuffer, &sid, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    value = (((num_applications != 0) && (num_contexts != 0)) ? req->options : 8); /* 8 = no matching context found */

    memcpy(resp.databuffer + offset, &value, sizeof(int8_t));
    offset += sizeof(int8_t);

    count_app_ids = num_applications;

    if (count_app_ids != 0) {
        memcpy(resp.databuffer + offset, &count_app_ids, sizeof(uint16_t));
        offset += sizeof(uint16_t);

#if (MCT_DEBUG_GETLOGINFO == 1)
        mct_vlog(LOG_DEBUG, "#apid: %d \n", count_app_ids);
#endif

        for (i = 0; i < count_app_ids; i++) {
            if (req->apid[0] != '\0') {
                apid = req->apid;
            } else {
                if (user_list->applications) {
                    apid = user_list->applications[i].apid;
                } else {
                    /* This should never occur! */
                    apid = 0;
                }
            }

            application = mct_daemon_application_find(daemon,
                                                      apid,
                                                      daemon->ecuid,
                                                      verbose);

            if (application) {
                /* Calculate start offset within contexts[] */
                offset_base = 0;

                for (j = 0; j < (application - (user_list->applications)); j++) {
                    offset_base += user_list->applications[j].num_contexts;
                }

                mct_set_id((char *)(resp.databuffer + offset), apid);
                offset += sizeof(ID4);

#if (MCT_DEBUG_GETLOGINFO == 1)
                mct_print_id(buf, apid);
                mct_vlog(LOG_DEBUG, "apid: %s\n", buf);
#endif

                if (req->apid[0] != '\0') {
                    count_con_ids = num_contexts;
                } else {
                    count_con_ids = application->num_contexts;
                }

                memcpy(resp.databuffer + offset, &count_con_ids, sizeof(uint16_t));
                offset += sizeof(uint16_t);

#if (MCT_DEBUG_GETLOGINFO == 1)
                mct_vlog(LOG_DEBUG, "#ctid: %d \n", count_con_ids);
#endif

                for (j = 0; j < count_con_ids; j++) {
#if (MCT_DEBUG_GETLOGINFO == 1)
                    mct_vlog(LOG_DEBUG, "j: %d \n", j);
#endif

                    if (!((count_con_ids == 1) && (req->apid[0] != '\0') &&
                          (req->ctid[0] != '\0'))) {
                        context = &(user_list->contexts[offset_base + j]);
                    }

                    /* else: context was already searched and found
                     *       (one application (found) with one context (found))*/

                    if ((context) &&
                        ((req->ctid[0] == '\0') || ((req->ctid[0] != '\0') &&
                                                    (memcmp(context->ctid, req->ctid,
                                                            MCT_ID_SIZE) == 0)))
                        ) {
                        mct_set_id((char *)(resp.databuffer + offset), context->ctid);
                        offset += sizeof(ID4);

#if (MCT_DEBUG_GETLOGINFO == 1)
                        mct_print_id(buf, context->ctid);
                        mct_vlog(LOG_DEBUG, "ctid: %s \n", buf);
#endif

                        /* Mode 4, 6, 7 */
                        if ((req->options == 4) || (req->options == 6) || (req->options == 7)) {
                            ll = context->log_level;
                            memcpy(resp.databuffer + offset, &ll, sizeof(int8_t));
                            offset += sizeof(int8_t);
                        }

                        /* Mode 5, 6, 7 */
                        if ((req->options == 5) || (req->options == 6) || (req->options == 7)) {
                            ts = context->trace_status;
                            memcpy(resp.databuffer + offset, &ts, sizeof(int8_t));
                            offset += sizeof(int8_t);
                        }

                        /* Mode 7 */
                        if (req->options == 7) {
                            if (context->context_description) {
                                len = strlen(context->context_description);
                                memcpy(resp.databuffer + offset, &len, sizeof(uint16_t));
                                offset += sizeof(uint16_t);
                                memcpy(resp.databuffer + offset, context->context_description,
                                       strlen(context->context_description));
                                offset += strlen(context->context_description);
                            } else {
                                len = 0;
                                memcpy(resp.databuffer + offset, &len, sizeof(uint16_t));
                                offset += sizeof(uint16_t);
                            }
                        }

#if (MCT_DEBUG_GETLOGINFO == 1)
                        mct_vlog(LOG_DEBUG, "ll=%d ts=%d \n", (int32_t)ll,
                                 (int32_t)ts);
#endif
                    }

#if (MCT_DEBUG_GETLOGINFO == 1)
                    mct_log(LOG_DEBUG, "\n");
#endif
                }

                /* Mode 7 */
                if (req->options == 7) {
                    if (application->application_description) {
                        len = strlen(application->application_description);
                        memcpy(resp.databuffer + offset, &len, sizeof(uint16_t));
                        offset += sizeof(uint16_t);
                        memcpy(resp.databuffer + offset, application->application_description,
                               strlen(application->application_description));
                        offset += strlen(application->application_description);
                    } else {
                        len = 0;
                        memcpy(resp.databuffer + offset, &len, sizeof(uint16_t));
                        offset += sizeof(uint16_t);
                    }
                }
            } /* if (application) */
        }     /* for (i=0;i<count_app_ids;i++) */
    }         /* if (count_app_ids!=0) */

    mct_set_id((char *)(resp.databuffer + offset), MCT_DAEMON_REMO_STRING);

    /* send message */
    mct_daemon_client_send_control_message(sock, daemon, daemon_local, &resp, "", "", verbose);

    /* free message */
    mct_message_free(&resp, 0);
}

int mct_daemon_control_message_buffer_overflow(int sock,
                                               MctDaemon *daemon,
                                               MctDaemonLocal *daemon_local,
                                               unsigned int overflow_counter,
                                               char *apid,
                                               int verbose)
{
    int ret;
    MctMessage msg;
    MctServiceMessageBufferOverflowResponse *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_MESSAGE_BUFFER_OVERFLOW,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceMessageBufferOverflowResponse);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    resp = (MctServiceMessageBufferOverflowResponse *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_MESSAGE_BUFFER_OVERFLOW;
    resp->status = MCT_SERVICE_RESPONSE_OK;
    resp->overflow = MCT_MESSAGE_BUFFER_OVERFLOW;
    resp->overflow_counter = overflow_counter;

    /* send message */
    if ((ret =
             mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, apid, "",
                                                    verbose))) {
        mct_message_free(&msg, 0);
        return ret;
    }

    /* free message */
    mct_message_free(&msg, 0);

    return MCT_DAEMON_ERROR_OK;
}

void mct_daemon_control_service_response(int sock,
                                         MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         uint32_t service_id,
                                         int8_t status,
                                         int verbose)
{
    MctMessage msg;
    MctServiceResponse *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return;
    }

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceResponse);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        return;
    }

    resp = (MctServiceResponse *)msg.databuffer;
    resp->service_id = service_id;
    resp->status = status;

    /* send message */
    mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "", verbose);

    /* free message */
    mct_message_free(&msg, 0);
}

int mct_daemon_control_message_unregister_context(int sock,
                                                  MctDaemon *daemon,
                                                  MctDaemonLocal *daemon_local,
                                                  char *apid,
                                                  char *ctid,
                                                  char *comid,
                                                  int verbose)
{
    MctMessage msg;
    MctServiceUnregisterContext *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return -1;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return -1;
    }

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceUnregisterContext);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        return -1;
    }

    resp = (MctServiceUnregisterContext *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_UNREGISTER_CONTEXT;
    resp->status = MCT_SERVICE_RESPONSE_OK;
    mct_set_id(resp->apid, apid);
    mct_set_id(resp->ctid, ctid);
    mct_set_id(resp->comid, comid);

    /* send message */
    if (mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "",
                                               verbose)) {
        mct_message_free(&msg, 0);
        return -1;
    }

    /* free message */
    mct_message_free(&msg, 0);

    return 0;
}

int mct_daemon_control_message_connection_info(int sock,
                                               MctDaemon *daemon,
                                               MctDaemonLocal *daemon_local,
                                               uint8_t state,
                                               char *comid,
                                               int verbose)
{
    MctMessage msg;
    MctServiceConnectionInfo *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return -1;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return -1;
    }

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceConnectionInfo);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        return -1;
    }

    resp = (MctServiceConnectionInfo *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_CONNECTION_INFO;
    resp->status = MCT_SERVICE_RESPONSE_OK;
    resp->state = state;
    mct_set_id(resp->comid, comid);

    /* send message */
    if (mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "",
                                               verbose)) {
        mct_message_free(&msg, 0);
        return -1;
    }

    /* free message */
    mct_message_free(&msg, 0);

    return 0;
}

int mct_daemon_control_message_timezone(int sock,
                                        MctDaemon *daemon,
                                        MctDaemonLocal *daemon_local,
                                        int verbose)
{
    MctMessage msg;
    MctServiceTimezone *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return -1;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return -1;
    }

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceTimezone);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        return -1;
    }

    resp = (MctServiceTimezone *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_TIMEZONE;
    resp->status = MCT_SERVICE_RESPONSE_OK;

    time_t t = time(NULL);
    struct tm lt;
    tzset();
    localtime_r(&t, &lt);
#if !defined(__CYGWIN__)
    resp->timezone = (int32_t)lt.tm_gmtoff;
#endif
    resp->isdst = (uint8_t)lt.tm_isdst;

    /* send message */
    if (mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "",
                                               verbose)) {
        mct_message_free(&msg, 0);
        return -1;
    }

    /* free message */
    mct_message_free(&msg, 0);

    return 0;
}

int mct_daemon_control_message_marker(int sock,
                                      MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      int verbose)
{
    MctMessage msg;
    MctServiceMarker *resp;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return -1;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return -1;
    }

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceMarker);

    if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
        free(msg.databuffer);
        msg.databuffer = 0;
    }

    if (msg.databuffer == 0) {
        msg.databuffer = (uint8_t *)malloc(msg.datasize);
        msg.databuffersize = msg.datasize;
    }

    if (msg.databuffer == 0) {
        return -1;
    }

    resp = (MctServiceMarker *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_MARKER;
    resp->status = MCT_SERVICE_RESPONSE_OK;

    /* send message */
    if (mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg, "", "",
                                               verbose)) {
        mct_message_free(&msg, 0);
        return -1;
    }

    /* free message */
    mct_message_free(&msg, 0);

    return 0;
}

void mct_daemon_control_callsw_cinjection(int sock,
                                          MctDaemon *daemon,
                                          MctDaemonLocal *daemon_local,
                                          MctMessage *msg,
                                          int verbose)
{
    char apid[MCT_ID_SIZE], ctid[MCT_ID_SIZE];
    uint32_t id = 0, id_tmp = 0;
    uint8_t *ptr;
    MctDaemonContext *context;
    int32_t data_length_inject = 0;
    uint32_t data_length_inject_tmp = 0;

    int32_t datalength;

    MctUserHeader userheader;
    MctStandardHeaderExtra extra;
    MctUserControlMsgInjection usercontext;
    uint8_t *userbuffer;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    datalength = msg->datasize;
    ptr = msg->databuffer;

    MCT_MSG_READ_VALUE(id_tmp, ptr, datalength, uint32_t); /* Get service id */
    id = MCT_ENDIAN_GET_32(msg->standardheader->htyp, id_tmp);

    /* injectionMode is disabled */
    if (daemon_local->flags.injectionMode == 0) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_PERM_DENIED,
                                            verbose);
        return;
    }

    /* id is always less than MCT_DAEMON_INJECTION_MAX since its type is uinit32_t */
    if (id >= MCT_DAEMON_INJECTION_MIN) {
        /* This a a real SW-C injection call */
        data_length_inject = 0;
        data_length_inject_tmp = 0;

        MCT_MSG_READ_VALUE(data_length_inject_tmp, ptr, datalength, uint32_t); /* Get data length */
        data_length_inject = MCT_ENDIAN_GET_32(msg->standardheader->htyp, data_length_inject_tmp);

        /* Get context handle for apid, ctid (and seid) */
        /* Warning: seid is ignored in this implementation! */
        if (MCT_IS_HTYP_UEH(msg->standardheader->htyp)) {
            mct_set_id(apid, msg->extendedheader->apid);
            mct_set_id(ctid, msg->extendedheader->ctid);
        } else {
            /* No extended header, and therefore no apid and ctid available */
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
            return;
        }

        /* At this point, apid and ctid is available */
        context = mct_daemon_context_find(daemon,
                                          apid,
                                          ctid,
                                          daemon->ecuid,
                                          verbose);

        if (context == 0) {
            /* mct_log(LOG_INFO,"No context found!\n"); */
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
            return;
        }

        /* at this point we can intercept with message filter handling. Above
         * we have some "natural" filtering (out of range Service ID, unknown
         * context etc.)
         */

        /* get ecu id information from message */
        extra = msg->headerextra;

        /* to have the same logic with other "is_allowed" functions, the
         * function will return a value > 0 if it is allowed */
        if (mct_daemon_filter_is_injection_allowed(&daemon_local->pFilter,
                                                   apid, ctid, extra.ecu, id) <= 0) {
            mct_log(LOG_WARNING, "Not allowed injection message received!\n");
            /* send a permission denied response when not allowed */
            mct_daemon_control_service_response(sock, daemon, daemon_local, id,
                                                MCT_SERVICE_RESPONSE_PERM_DENIED, verbose);
            return;
        }

        /* Send user message to handle, specified in context */
        if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_INJECTION) < MCT_RETURN_OK) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
            return;
        }

        usercontext.log_level_pos = context->log_level_pos;

        if (data_length_inject > msg->databuffersize) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
            return;
        }

        userbuffer = malloc(data_length_inject);

        if (userbuffer == 0) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
            return;
        }

        usercontext.data_length_inject = data_length_inject;
        usercontext.service_id = id;

        memcpy(userbuffer, ptr, data_length_inject);  /* Copy received injection to send buffer */

        /* write to FIFO */
        MctReturnValue ret =
            mct_user_log_out3(context->user_handle, &(userheader), sizeof(MctUserHeader),
                              &(usercontext), sizeof(MctUserControlMsgInjection),
                              userbuffer, data_length_inject);

        if (ret < MCT_RETURN_OK) {
            if (ret == MCT_RETURN_PIPE_ERROR) {
                /* Close connection */
                close(context->user_handle);
                context->user_handle = MCT_FD_INIT;
            }

            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
        } else {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                id,
                                                MCT_SERVICE_RESPONSE_OK,
                                                verbose);
        }

        free(userbuffer);
        userbuffer = 0;
    } else {
        /* Invalid ID */
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_NOT_SUPPORTED,
                                            verbose);
    }
}

void mct_daemon_send_log_level(int sock,
                               MctDaemon *daemon,
                               MctDaemonLocal *daemon_local,
                               MctDaemonContext *context,
                               int8_t loglevel,
                               int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    int32_t id = MCT_SERVICE_ID_SET_LOG_LEVEL;
    int8_t old_log_level = 0;

    old_log_level = context->log_level;
    context->log_level = loglevel; /* No endianess conversion necessary*/

    if ((context->user_handle >= MCT_FD_MINIMUM) &&
        (mct_daemon_user_send_log_level(daemon, context, verbose) == 0)) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_log(LOG_ERR, "Log level could not be sent!\n");
        context->log_level = old_log_level;
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_find_multiple_context_and_send_log_level(int sock,
                                                         MctDaemon *daemon,
                                                         MctDaemonLocal *daemon_local,
                                                         int8_t app_flag,
                                                         char *str,
                                                         int8_t len,
                                                         int8_t loglevel,
                                                         int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    int count = 0;
    MctDaemonContext *context = NULL;
    char src_str[MCT_ID_SIZE + 1] = {0};
    int ret = 0;
    MctDaemonRegisteredUsers *user_list = NULL;

    if (daemon == 0) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    for (count = 0; count < user_list->num_contexts; count++) {
        context = &(user_list->contexts[count]);

        if (context) {
            if (app_flag == 1) {
                strncpy(src_str, context->apid, MCT_ID_SIZE);
            } else {
                strncpy(src_str, context->ctid, MCT_ID_SIZE);
            }

            ret = strncmp(src_str, str, len);

            if (ret == 0) {
                mct_daemon_send_log_level(sock, daemon, daemon_local, context, loglevel, verbose);
            } else if ((ret > 0) && (app_flag == 1)) {
                break;
            } else {
                continue;
            }
        }
    }
}

void mct_daemon_control_set_log_level(int sock,
                                      MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      MctMessage *msg,
                                      int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    char apid[MCT_ID_SIZE + 1] = {0};
    char ctid[MCT_ID_SIZE + 1] = {0};
    MctServiceSetLogLevel *req = NULL;
    MctDaemonContext *context = NULL;
    int8_t apid_length = 0;
    int8_t ctid_length = 0;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetLogLevel)) < 0) {
        return;
    }

    req = (MctServiceSetLogLevel *)(msg->databuffer);

    if (daemon_local->flags.enforceContextLLAndTS) {
        req->log_level = getStatus(req->log_level, daemon_local->flags.contextLogLevel);
    }

    mct_set_id(apid, req->apid);
    mct_set_id(ctid, req->ctid);
    apid_length = strlen(apid);
    ctid_length = strlen(ctid);

    if ((apid_length != 0) && (apid[apid_length - 1] == '*') && (ctid[0] == 0)) { /*apid provided having '*' in it and ctid is null*/
        mct_daemon_find_multiple_context_and_send_log_level(sock,
                                                            daemon,
                                                            daemon_local,
                                                            1,
                                                            apid,
                                                            apid_length - 1,
                                                            req->log_level,
                                                            verbose);
    } else if ((ctid_length != 0) && (ctid[ctid_length - 1] == '*') && (apid[0] == 0)) { /*ctid provided is having '*' in it and apid is null*/
        mct_daemon_find_multiple_context_and_send_log_level(sock,
                                                            daemon,
                                                            daemon_local,
                                                            0,
                                                            ctid,
                                                            ctid_length - 1,
                                                            req->log_level,
                                                            verbose);
    } else if ((apid_length != 0) && (apid[apid_length - 1] != '*') && (ctid[0] == 0)) { /*only app id case*/
        mct_daemon_find_multiple_context_and_send_log_level(sock,
                                                            daemon,
                                                            daemon_local,
                                                            1,
                                                            apid,
                                                            MCT_ID_SIZE,
                                                            req->log_level,
                                                            verbose);
    } else if ((ctid_length != 0) && (ctid[ctid_length - 1] != '*') && (apid[0] == 0)) { /*only context id case*/
        mct_daemon_find_multiple_context_and_send_log_level(sock,
                                                            daemon,
                                                            daemon_local,
                                                            0,
                                                            ctid,
                                                            MCT_ID_SIZE,
                                                            req->log_level,
                                                            verbose);
    } else {
        context = mct_daemon_context_find(daemon,
                                          apid,
                                          ctid,
                                          daemon->ecuid,
                                          verbose);

        /* Set log level */
        if (context != 0) {
            mct_daemon_send_log_level(sock, daemon, daemon_local, context, req->log_level, verbose);
        } else {
            mct_vlog(LOG_ERR,
                     "Could not set log level: %d. Context [%.4s:%.4s] not found:",
                     req->log_level,
                     apid,
                     ctid);
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_SET_LOG_LEVEL,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
        }
    }
}


void mct_daemon_send_trace_status(int sock,
                                  MctDaemon *daemon,
                                  MctDaemonLocal *daemon_local,
                                  MctDaemonContext *context,
                                  int8_t tracestatus,
                                  int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    int32_t id = MCT_SERVICE_ID_SET_TRACE_STATUS;
    int8_t old_trace_status = 0;

    old_trace_status = context->trace_status;
    context->trace_status = tracestatus; /* No endianess conversion necessary*/

    if ((context->user_handle >= MCT_FD_MINIMUM) &&
        (mct_daemon_user_send_log_level(daemon, context, verbose) == 0)) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_log(LOG_ERR, "Trace status could not be sent!\n");
        context->trace_status = old_trace_status;
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_find_multiple_context_and_send_trace_status(int sock,
                                                            MctDaemon *daemon,
                                                            MctDaemonLocal *daemon_local,
                                                            int8_t app_flag,
                                                            char *str,
                                                            int8_t len,
                                                            int8_t tracestatus,
                                                            int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    int count = 0;
    MctDaemonContext *context = NULL;
    char src_str[MCT_ID_SIZE + 1] = {0};
    int ret = 0;
    MctDaemonRegisteredUsers *user_list = NULL;

    if (daemon == 0) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    for (count = 0; count < user_list->num_contexts; count++) {
        context = &(user_list->contexts[count]);

        if (context) {
            if (app_flag == 1) {
                strncpy(src_str, context->apid, MCT_ID_SIZE);
            } else {
                strncpy(src_str, context->ctid, MCT_ID_SIZE);
            }

            ret = strncmp(src_str, str, len);

            if (ret == 0) {
                mct_daemon_send_trace_status(sock,
                                             daemon,
                                             daemon_local,
                                             context,
                                             tracestatus,
                                             verbose);
            } else if ((ret > 0) && (app_flag == 1)) {
                break;
            } else {
                continue;
            }
        }
    }
}

void mct_daemon_control_set_trace_status(int sock,
                                         MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         MctMessage *msg,
                                         int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    char apid[MCT_ID_SIZE + 1] = {0};
    char ctid[MCT_ID_SIZE + 1] = {0};
    MctServiceSetLogLevel *req = NULL;
    MctDaemonContext *context = NULL;
    int8_t apid_length = 0;
    int8_t ctid_length = 0;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetLogLevel)) < 0) {
        return;
    }

    req = (MctServiceSetLogLevel *)(msg->databuffer);

    if (daemon_local->flags.enforceContextLLAndTS) {
        req->log_level = getStatus(req->log_level, daemon_local->flags.contextTraceStatus);
    }

    mct_set_id(apid, req->apid);
    mct_set_id(ctid, req->ctid);
    apid_length = strlen(apid);
    ctid_length = strlen(ctid);

    if ((apid_length != 0) && (apid[apid_length - 1] == '*') && (ctid[0] == 0)) { /*apid provided having '*' in it and ctid is null*/
        mct_daemon_find_multiple_context_and_send_trace_status(sock,
                                                               daemon,
                                                               daemon_local,
                                                               1,
                                                               apid,
                                                               apid_length - 1,
                                                               req->log_level,
                                                               verbose);
    } else if ((ctid_length != 0) && (ctid[ctid_length - 1] == '*') && (apid[0] == 0)) { /*ctid provided is having '*' in it and apid is null*/
        mct_daemon_find_multiple_context_and_send_trace_status(sock,
                                                               daemon,
                                                               daemon_local,
                                                               0,
                                                               ctid,
                                                               ctid_length - 1,
                                                               req->log_level,
                                                               verbose);
    } else if ((apid_length != 0) && (apid[apid_length - 1] != '*') && (ctid[0] == 0)) { /*only app id case*/
        mct_daemon_find_multiple_context_and_send_trace_status(sock,
                                                               daemon,
                                                               daemon_local,
                                                               1,
                                                               apid,
                                                               MCT_ID_SIZE,
                                                               req->log_level,
                                                               verbose);
    } else if ((ctid_length != 0) && (ctid[ctid_length - 1] != '*') && (apid[0] == 0)) { /*only context id case*/
        mct_daemon_find_multiple_context_and_send_trace_status(sock,
                                                               daemon,
                                                               daemon_local,
                                                               0,
                                                               ctid,
                                                               MCT_ID_SIZE,
                                                               req->log_level,
                                                               verbose);
    } else {
        context = mct_daemon_context_find(daemon, apid, ctid, daemon->ecuid, verbose);

        /* Set trace status */
        if (context != 0) {
            mct_daemon_send_trace_status(sock,
                                         daemon,
                                         daemon_local,
                                         context,
                                         req->log_level,
                                         verbose);
        } else {
            mct_vlog(LOG_ERR,
                     "Could not set trace status: %d. Context [%.4s:%.4s] not found:",
                     req->log_level,
                     apid,
                     ctid);
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_SET_LOG_LEVEL,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
        }
    }
}

void mct_daemon_control_set_default_log_level(int sock,
                                              MctDaemon *daemon,
                                              MctDaemonLocal *daemon_local,
                                              MctMessage *msg,
                                              int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    MctServiceSetDefaultLogLevel *req;
    int32_t id = MCT_SERVICE_ID_SET_DEFAULT_LOG_LEVEL;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetDefaultLogLevel)) < 0) {
        return;
    }

    req = (MctServiceSetDefaultLogLevel *)(msg->databuffer);

    /* No endianess conversion necessary */
    if ((req->log_level <= MCT_LOG_VERBOSE)) {
        if (daemon_local->flags.enforceContextLLAndTS) {
            daemon->default_log_level = getStatus(req->log_level,
                                                  daemon_local->flags.contextLogLevel);
        } else {
            daemon->default_log_level = req->log_level; /* No endianess conversion necessary */
        }

        /* Send Update to all contexts using the default log level */
        mct_daemon_user_send_default_update(daemon, verbose);

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_control_set_all_log_level(int sock,
                                          MctDaemon *daemon,
                                          MctDaemonLocal *daemon_local,
                                          MctMessage *msg,
                                          int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    MctServiceSetDefaultLogLevel *req = NULL;
    int32_t id = MCT_SERVICE_ID_SET_ALL_LOG_LEVEL;
    int8_t loglevel = 0;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetDefaultLogLevel)) < 0) {
        return;
    }

    req = (MctServiceSetDefaultLogLevel *)(msg->databuffer);

    /* No endianess conversion necessary */
    if ((req != NULL) &&
        ((req->log_level <= MCT_LOG_VERBOSE) || (req->log_level == (uint8_t)MCT_LOG_DEFAULT))) {
        if (daemon_local->flags.enforceContextLLAndTS) {
            loglevel = getStatus(req->log_level, daemon_local->flags.contextLogLevel);
        } else {
            loglevel = req->log_level; /* No endianess conversion necessary */
        }

        /* Send Update to all contexts using the new log level */
        mct_daemon_user_send_all_log_level_update(daemon, loglevel, verbose);

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_control_set_default_trace_status(int sock,
                                                 MctDaemon *daemon,
                                                 MctDaemonLocal *daemon_local,
                                                 MctMessage *msg,
                                                 int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    /* Payload of request message */
    MctServiceSetDefaultLogLevel *req;
    int32_t id = MCT_SERVICE_ID_SET_DEFAULT_TRACE_STATUS;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetDefaultLogLevel)) < 0) {
        return;
    }

    req = (MctServiceSetDefaultLogLevel *)(msg->databuffer);

    /* No endianess conversion necessary */
    if ((req->log_level == MCT_TRACE_STATUS_OFF) ||
        (req->log_level == MCT_TRACE_STATUS_ON)) {
        if (daemon_local->flags.enforceContextLLAndTS) {
            daemon->default_trace_status = getStatus(req->log_level,
                                                     daemon_local->flags.contextTraceStatus);
        } else {
            daemon->default_trace_status = req->log_level; /* No endianess conversion necessary*/
        }

        /* Send Update to all contexts using the default trace status */
        mct_daemon_user_send_default_update(daemon, verbose);

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_control_set_all_trace_status(int sock,
                                             MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             MctMessage *msg,
                                             int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    MctServiceSetDefaultLogLevel *req = NULL;
    int32_t id = MCT_SERVICE_ID_SET_ALL_TRACE_STATUS;
    int8_t tracestatus = 0;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetDefaultLogLevel)) < 0) {
        return;
    }

    req = (MctServiceSetDefaultLogLevel *)(msg->databuffer);

    /* No endianess conversion necessary */
    if ((req != NULL) &&
        ((req->log_level <= MCT_TRACE_STATUS_ON) ||
         (req->log_level == (uint8_t)MCT_TRACE_STATUS_DEFAULT))) {
        if (daemon_local->flags.enforceContextLLAndTS) {
            tracestatus = getStatus(req->log_level, daemon_local->flags.contextTraceStatus);
        } else {
            tracestatus = req->log_level; /* No endianess conversion necessary */
        }

        /* Send Update to all contexts using the new log level */
        mct_daemon_user_send_all_trace_status_update(daemon, tracestatus, verbose);

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_control_set_timing_packets(int sock,
                                           MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctMessage *msg,
                                           int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    MctServiceSetVerboseMode *req;  /* request uses same struct as set verbose mode */
    int32_t id = MCT_SERVICE_ID_SET_TIMING_PACKETS;

    if ((daemon == NULL) || (msg == NULL) || (msg->databuffer == NULL)) {
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceSetVerboseMode)) < 0) {
        return;
    }

    req = (MctServiceSetVerboseMode *)(msg->databuffer);

    if ((req->new_status == 0) || (req->new_status == 1)) {
        daemon->timingpackets = req->new_status;

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);
    } else {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            id,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}

void mct_daemon_control_message_time(int sock,
                                     MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     int verbose)
{
    MctMessage msg;
    int32_t len;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == 0) {
        return;
    }

    /* initialise new message */
    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return;
    }

    /* send message */

    /* prepare storage header */
    msg.storageheader = (MctStorageHeader *)msg.headerbuffer;
    mct_set_storageheader(msg.storageheader, daemon->ecuid);

    /* prepare standard header */
    msg.standardheader = (MctStandardHeader *)(msg.headerbuffer + sizeof(MctStorageHeader));
    msg.standardheader->htyp = MCT_HTYP_WEID | MCT_HTYP_WTMS | MCT_HTYP_UEH |
        MCT_HTYP_PROTOCOL_VERSION1;

#if (BYTE_ORDER == BIG_ENDIAN)
    msg.standardheader->htyp = (msg.standardheader->htyp | MCT_HTYP_MSBF);
#endif

    msg.standardheader->mcnt = 0;

    /* Set header extra parameters */
    mct_set_id(msg.headerextra.ecu, daemon->ecuid);
    msg.headerextra.tmsp = mct_uptime();

    mct_message_set_extraparameters(&msg, verbose);

    /* prepare extended header */
    msg.extendedheader =
        (MctExtendedHeader *)(msg.headerbuffer + sizeof(MctStorageHeader) +
                              sizeof(MctStandardHeader) +
                              MCT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp));
    msg.extendedheader->msin = MCT_MSIN_CONTROL_TIME;

    msg.extendedheader->noar = 0;                  /* number of arguments */
    mct_set_id(msg.extendedheader->apid, "");      /* application id */
    mct_set_id(msg.extendedheader->ctid, "");      /* context id */

    /* prepare length information */
    msg.headersize = sizeof(MctStorageHeader) + sizeof(MctStandardHeader) +
        sizeof(MctExtendedHeader) +
        MCT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp);

    len = msg.headersize - sizeof(MctStorageHeader) + msg.datasize;

    if (len > UINT16_MAX) {
        mct_log(LOG_WARNING, "Huge control message discarded!\n");

        /* free message */
        mct_message_free(&msg, 0);

        return;
    }

    msg.standardheader->len = MCT_HTOBE_16(((uint16_t)len));

    /* Send message, ignore return value */
    mct_daemon_client_send(sock, daemon, daemon_local, msg.headerbuffer,
                           sizeof(MctStorageHeader),
                           msg.headerbuffer + sizeof(MctStorageHeader),
                           msg.headersize - sizeof(MctStorageHeader),
                           msg.databuffer, msg.datasize, verbose);

    /* free message */
    mct_message_free(&msg, 0);
}

void mct_daemon_control_set_filter_level(int sock, MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         MctMessage *msg, int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    MctServiceSetFilterLevel *req;
    uint32_t id = MCT_SERVICE_ID_SET_FILTER_LEVEL;
    int new_level = 0;

    if ((daemon == NULL) || (daemon_local == NULL) || (msg == NULL) ||
        (msg->databuffer == NULL)) {
        return;
    }

    req = (MctServiceSetFilterLevel *)msg->databuffer;
    new_level = req->level;

    if (mct_daemon_filter_change_filter_level(daemon_local, new_level,
                                              verbose) < 0) {
        mct_daemon_control_service_response(sock, daemon, daemon_local, id,
                                            MCT_SERVICE_RESPONSE_ERROR, verbose);
    } else {
        mct_daemon_control_service_response(sock, daemon, daemon_local, id,
                                            MCT_SERVICE_RESPONSE_OK, verbose);
    }
}

void mct_daemon_control_get_filter_status(int sock, MctDaemon *daemon,
                                          MctDaemonLocal *daemon_local,
                                          int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    MctMessage msg = {0};
    MctServiceGetCurrentFilterInfo *resp;
    int num_injections = 0;
    char **injections = NULL;

    if ((daemon == NULL) || (daemon_local == NULL)) {
        return;
    }

    if (mct_message_init(&msg, verbose) == -1) {
        return;
    }

    num_injections = daemon_local->pFilter.current->num_injections;
    injections = daemon_local->pFilter.current->injections;

    /* prepare payload of data */
    msg.datasize = sizeof(MctServiceGetCurrentFilterInfo);

    msg.databuffer = (uint8_t *)calloc(msg.datasize, sizeof(uint8_t));

    if (msg.databuffer == NULL) {
        mct_log(LOG_CRIT, "Cannot allocate memory for message response\n");
        return;
    }

    msg.databuffersize = msg.datasize;

    resp = (MctServiceGetCurrentFilterInfo *)msg.databuffer;
    resp->service_id = MCT_SERVICE_ID_GET_FILTER_STATUS;
    resp->status = MCT_SERVICE_RESPONSE_OK;
    strncpy(resp->name, daemon_local->pFilter.current->name, sizeof(resp->name));
    resp->level_min = daemon_local->pFilter.current->level_min;
    resp->level_max = daemon_local->pFilter.current->level_max;
    resp->client_mask = daemon_local->pFilter.current->client_mask;
    memcpy(resp->ctrl_mask_lower,
           daemon_local->pFilter.current->ctrl_mask.lower,
           sizeof(resp->ctrl_mask_lower));
    memcpy(resp->ctrl_mask_upper,
           daemon_local->pFilter.current->ctrl_mask.upper,
           sizeof(resp->ctrl_mask_upper));

    /* create a list of injection names for the filter config */
    if (num_injections == -1) {
        memcpy(resp->injections,
               MCT_FILTER_CLIENT_ALL,
               strlen(MCT_FILTER_CLIENT_ALL));
    } else if (num_injections == 0) {
        memcpy(resp->injections,
               MCT_FILTER_CLIENT_NONE,
               strlen(MCT_FILTER_CLIENT_NONE));
    } else {
        char *p = resp->injections;
        int i = 0;
        int string_len = 0;

        for (i = 0; i < num_injections; i++) {
            int len = strlen(injections[i]);

            if ((string_len + len + 1) < MCT_CONFIG_FILE_ENTRY_MAX_LEN) {
                memcpy(p, injections[i], len);
                p += len;

                if ((i + 1) < num_injections) {
                    *p = ',';
                    p += 1;
                }

                string_len = string_len + len + 1;
            } else {
                mct_log(LOG_WARNING,
                        "Injection string is too long. Skip further injections\n");
                break;
            }
        }
    }

    mct_daemon_client_send_control_message(sock, daemon, daemon_local, &msg,
                                           "", "", verbose);
    /* free message */
    mct_message_free(&msg, verbose);
}

int mct_daemon_process_one_s_timer(MctDaemon *daemon,
                                   MctDaemonLocal *daemon_local,
                                   MctReceiver *receiver,
                                   int verbose)
{
    uint64_t expir = 0;
    ssize_t res = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon_local == NULL) || (daemon == NULL) || (receiver == NULL)) {
        mct_vlog(LOG_ERR, "%s: invalid parameters", __func__);
        return -1;
    }

    res = read(receiver->fd, &expir, sizeof(expir));

    if (res < 0) {
        mct_vlog(LOG_WARNING, "%s: Fail to read timer (%s)\n", __func__,
                 strerror(errno));
        /* Activity received on timer_wd, but unable to read the fd:
         * let's go on sending notification */
    }

    if ((daemon->state == MCT_DAEMON_STATE_SEND_BUFFER) ||
        (daemon->state == MCT_DAEMON_STATE_BUFFER_FULL)) {
        if (mct_daemon_send_ringbuffer_to_client(daemon,
                                                 daemon_local,
                                                 daemon_local->flags.vflag)) {
            mct_log(LOG_DEBUG,
                    "Can't send contents of ring buffer to clients\n");
        }
    }

    if ((daemon->timingpackets) &&
        (daemon->state == MCT_DAEMON_STATE_SEND_DIRECT)) {
        mct_daemon_control_message_time(MCT_DAEMON_SEND_TO_ALL,
                                        daemon,
                                        daemon_local,
                                        daemon_local->flags.vflag);
    }

    mct_log(LOG_DEBUG, "Timer timingpacket\n");

    return 0;
}

int mct_daemon_process_sixty_s_timer(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *receiver,
                                     int verbose)
{
    uint64_t expir = 0;
    ssize_t res = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon_local == NULL) || (daemon == NULL) || (receiver == NULL)) {
        mct_vlog(LOG_ERR, "%s: invalid parameters", __func__);
        return -1;
    }

    res = read(receiver->fd, &expir, sizeof(expir));

    if (res < 0) {
        mct_vlog(LOG_WARNING, "%s: Fail to read timer (%s)\n", __func__,
                 strerror(errno));
        /* Activity received on timer_wd, but unable to read the fd:
         * let's go on sending notification */
    }

    if (daemon_local->flags.sendECUSoftwareVersion > 0) {
        mct_daemon_control_get_software_version(MCT_DAEMON_SEND_TO_ALL,
                                                daemon,
                                                daemon_local,
                                                daemon_local->flags.vflag);
    }

    if (daemon_local->flags.sendTimezone > 0) {
        /* send timezone information */
        time_t t = time(NULL);
        struct tm lt;

        /*Added memset to avoid compiler warning for near initialization */
        memset((void *)&lt, 0, sizeof(lt));
        tzset();
        localtime_r(&t, &lt);

        mct_daemon_control_message_timezone(MCT_DAEMON_SEND_TO_ALL,
                                            daemon,
                                            daemon_local,
                                            daemon_local->flags.vflag);
    }

    mct_log(LOG_DEBUG, "Timer ecuversion\n");

    return 0;
}

#ifdef MCT_SYSTEMD_WATCHDOG_ENABLE
int mct_daemon_process_systemd_timer(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *receiver,
                                     int verbose)
{
    uint64_t expir = 0;
    ssize_t res = -1;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon_local == NULL) || (daemon == NULL) || (receiver == NULL)) {
        mct_vlog(LOG_ERR, "%s: invalid parameters", __func__);
        return res;
    }

    res = read(receiver->fd, &expir, sizeof(expir));

    if (res < 0) {
        mct_vlog(LOG_WARNING, "Failed to read timer_wd; %s\n", strerror(errno));
        /* Activity received on timer_wd, but unable to read the fd:
         * let's go on sending notification */
    }

    if (sd_notify(0, "WATCHDOG=1") < 0) {
        mct_log(LOG_CRIT, "Could not reset systemd watchdog\n");
    }

    mct_log(LOG_DEBUG, "Timer watchdog\n");

    return 0;
}
#else
int mct_daemon_process_systemd_timer(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *receiver,
                                     int verbose)
{
    (void)daemon;
    (void)daemon_local;
    (void)receiver;
    (void)verbose;

    mct_log(LOG_DEBUG, "Timer watchdog not enabled\n");

    return -1;
}
#endif

void mct_daemon_control_service_logstorage(int sock,
                                           MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctMessage *msg,
                                           int verbose)
{
    MctServiceOfflineLogstorage *req;
    int ret;
    unsigned int connection_type = 0;
    MctLogStorage *device = NULL;
    int device_index = -1;
    int i = 0;

    int tmp_errno = 0;

    struct stat daemon_mpoint_st = {0};
    int daemon_st_status = 0;

    struct stat req_mpoint_st = {0};
    int req_st_status = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (msg == NULL) || (daemon_local == NULL)) {
        mct_vlog(LOG_ERR,
                 "%s: Invalid function parameters\n",
                 __func__);
        return;
    }

    if ((daemon_local->flags.offlineLogstorageMaxDevices <= 0) || (msg->databuffer == NULL)) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);

        mct_log(LOG_INFO,
                "Logstorage functionality not enabled or MAX device set is 0\n");
        return;
    }

    if (mct_check_rcv_data_size(msg->datasize, sizeof(MctServiceOfflineLogstorage)) < 0) {
        return;
    }

    req = (MctServiceOfflineLogstorage *)(msg->databuffer);

    if(req->connection_type != MCT_OFFLINE_LOGSTORAGE_SYNC_CACHES) {
        req_st_status = stat(req->mount_point, &req_mpoint_st);
        tmp_errno = errno;
        if (req_st_status < 0) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);

            mct_vlog(LOG_WARNING,
                     "%s: Failed to stat requested mount point [%s] with error [%s]\n",
                     __func__, req->mount_point, strerror(tmp_errno));
            return;
        }
    }

    for (i = 0; i < daemon_local->flags.offlineLogstorageMaxDevices; i++) {
        connection_type = daemon->storage_handle[i].connection_type;

        memset(&daemon_mpoint_st, 0, sizeof(struct stat));

        if (strlen(daemon->storage_handle[i].device_mount_point) > 1) {
            daemon_st_status = stat(daemon->storage_handle[i].device_mount_point,
                                    &daemon_mpoint_st);
            tmp_errno = errno;

            if (daemon_st_status < 0) {
                mct_daemon_control_service_response(sock,
                                                    daemon,
                                                    daemon_local,
                                                    MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                                    MCT_SERVICE_RESPONSE_ERROR,
                                                    verbose);
                mct_vlog(LOG_WARNING,
                         "%s: Failed to stat daemon mount point [%s] with error [%s]\n",
                         __func__, daemon->storage_handle[i].device_mount_point,
                         strerror(tmp_errno));
                return;
            }

            /* Check if the requested device path is already used as log storage device */
            if ((req_mpoint_st.st_dev == daemon_mpoint_st.st_dev) &&
                (req_mpoint_st.st_ino == daemon_mpoint_st.st_ino)) {
                device_index = i;
                break;
            }
        }

        /* Get first available device index here */
        if ((connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) &&
            (device_index == -1)) {
            device_index = i;
        }
    }

    /* It might be possible to sync all caches of all devices */
    if ((req->connection_type == MCT_OFFLINE_LOGSTORAGE_SYNC_CACHES) &&
        (strlen(req->mount_point) == 0)) {
        /* It is expected to receive an empty mount point to sync all Logstorage
         * devices in this case. */
    } else if (device_index == -1) {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
        mct_log(LOG_WARNING, "MAX devices already in use  \n");
        return;
    }

    /* Check for device connection request from log storage ctrl app  */
    device = &daemon->storage_handle[device_index];

    if (req->connection_type == MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) {
        ret = mct_logstorage_device_connected(device, req->mount_point);

        if (ret == 1) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                                MCT_SERVICE_RESPONSE_WARNING,
                                                verbose);
            return;
        } else if (ret != 0) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
            return;
        }

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);

        /* Update maintain logstorage loglevel if necessary */
        if (daemon->storage_handle[device_index].maintain_logstorage_loglevel !=
            MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_UNDEF) {
            daemon->maintain_logstorage_loglevel =
                daemon->storage_handle[device_index].maintain_logstorage_loglevel;
        }

        /* update BlockMode if necessary */
        if (daemon_local->flags.blockModeAllowed == MCT_DAEMON_BLOCK_MODE_ENABLED) {
            if (daemon->storage_handle[device_index].block_mode != MCT_MODE_BLOCKING_UNDEF) {
                if (mct_daemon_user_update_blockmode(daemon,
                                                     MCT_ALL_APPLICATIONS,
                                                     daemon->storage_handle[device_index].
                                                     block_mode,
                                                     verbose) != MCT_RETURN_OK) {
                    mct_log(LOG_WARNING,
                            "BlockMode request of internal logstorage device failed\n");
                }
            }
        } else {
            mct_log(LOG_INFO,
                    "BlockMode requested by Logstorage device, but not allowed");
        }

        /* Check if log level of running application needs an update */
        mct_daemon_logstorage_update_application_loglevel(daemon,
                                                          daemon_local,
                                                          device_index,
                                                          verbose);

        daemon_local->internal_client_connections++;
    }
    /* Check for device disconnection request from log storage ctrl app  */
    else if (req->connection_type == MCT_OFFLINE_LOGSTORAGE_DEVICE_DISCONNECTED) {
        /* Check if log level of running application needs to be reset */
        mct_daemon_logstorage_reset_application_loglevel(
            daemon,
            daemon_local,
            device_index,
            daemon_local->flags.offlineLogstorageMaxDevices,
            verbose);

        mct_logstorage_device_disconnected(&(daemon->storage_handle[device_index]),
                                           MCT_LOGSTORAGE_SYNC_ON_DEVICE_DISCONNECT);

        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                            MCT_SERVICE_RESPONSE_OK,
                                            verbose);

        daemon_local->internal_client_connections--;

        /* switch to NON-BLOCKING if no other client is connected */
        if ((daemon_local->flags.blockModeAllowed == MCT_DAEMON_BLOCK_MODE_ENABLED) &&
            (daemon_local->client_connections == 0) &&
            (daemon_local->internal_client_connections == 0)) {
            if (mct_daemon_user_update_blockmode(daemon,
                                                 MCT_ALL_APPLICATIONS,
                                                 MCT_MODE_NON_BLOCKING,
                                                 verbose) != MCT_RETURN_OK) {
                mct_log(LOG_WARNING,
                        "BlockMode request of internal logstorage device failed\n");
            }
        }
    }
    /* Check for cache synchronization request from log storage ctrl app */
    else if (req->connection_type == MCT_OFFLINE_LOGSTORAGE_SYNC_CACHES) {
        ret = 0;

        if (device_index == -1) { /* sync all Logstorage devices */

            for (i = 0; i < daemon_local->flags.offlineLogstorageMaxDevices; i++) {
                if (daemon->storage_handle[i].connection_type ==
                    MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) {
                    ret = mct_daemon_logstorage_sync_cache(
                            daemon,
                            daemon_local,
                            daemon->storage_handle[i].device_mount_point,
                            verbose);
                }
            }
        } else {
            /* trigger logstorage to sync caches */
            ret = mct_daemon_logstorage_sync_cache(daemon,
                                                   daemon_local,
                                                   req->mount_point,
                                                   verbose);
        }

        if (ret == 0) {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                                MCT_SERVICE_RESPONSE_OK,
                                                verbose);
        } else {
            mct_daemon_control_service_response(sock,
                                                daemon,
                                                daemon_local,
                                                MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                                MCT_SERVICE_RESPONSE_ERROR,
                                                verbose);
        }
    } else {
        mct_daemon_control_service_response(sock,
                                            daemon,
                                            daemon_local,
                                            MCT_SERVICE_ID_OFFLINE_LOGSTORAGE,
                                            MCT_SERVICE_RESPONSE_ERROR,
                                            verbose);
    }
}