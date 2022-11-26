#ifndef _DLT_DAEMON_FILTER_H
#define _DLT_DAEMON_FILTER_H

#include "mct_daemon_connection_types.h"
#include "mct_daemon_filter_types.h"
#include "mct-daemon.h"

/**
 * @brief callback for events received from a filter backend
 *
 * @param daemon        DltDaemon structure pointer
 * @param daemon_local  DltDaemonLocal structure pointer
 * @param rec           DltReceiver structure pointer
 * @param verbose       verbose flag
 * @return DLT_RETURN_OK on success, DLT error code otherwise
 */
DltReturnValue mct_daemon_filter_process_filter_control_messages(
    DltDaemon *daemon,
    DltDaemonLocal *daemon_local,
    DltReceiver *rec,
    int verbose);

/**
 * @brief Change the filter to the given level
 *
 * This function changes the filter configuration to the given filter level.
 * It is called by the DLT Daemon directly when it receives ChangeFilterLevel
 * service request send by a control application.
 *
 * @param daemon_local  DltDaemonLocal structure
 * @param level         New filter level
 * @param verbose       verbose flag
 * @return DLT_RETURN_OK on success, DLT error code otherwise
 */
DltReturnValue mct_daemon_filter_change_filter_level(DltDaemonLocal *daemon_local,
                                                     unsigned int level,
                                                     int verbose);

/**
 * @brief Check if a connection is allowed in the current filter level
 *
 * This function uses the currently active filter configuration to check if a
 * certain connection type is allowed.
 *
 * @param filter   Message filter
 * @param type     ConnectionType
 * @return > 0 if allowed, 0 if not allowed or -1 on error
 */
int mct_daemon_filter_is_connection_allowed(DltMessageFilter *filter,
                                            DltConnectionType type);

/**
 * @brief Check if a control message is allowed in the current filter level
 *
 * This function uses the currently active filter configuration to check if
 * the given control message - specified by its identifier - has to be rejected
 * or handled.
 *
 * @param filter   Message filter
 * @param id       Control message identifier
 * @return true > 0 if allowed, 0 if not allowed or -1 on error
 */
int mct_daemon_filter_is_control_allowed(DltMessageFilter *filter, int id);

/**
 * @brief Check if an injection message is allowed
 *
 * This function uses the currently active filter configuration to check if
 * the received injection message has to be rejected or forwarded to the
 * corresponding application.
 *
 * @param filter    Message filter
 * @param apid      Application identifier of the message receiver
 * @param ctid      Context identifier of the message receiver
 * @param ecuid     Node identifier of the message receiver
 * @param id        Service identifier of the injection message
 * @return DLT_RETURN_TRUE if allowed, DLT_RETURN_OK if not allowed or DLT_RETURN_ERROR on error
 */
DltReturnValue mct_daemon_filter_is_injection_allowed(DltMessageFilter *filter,
                                                      char *apid,
                                                      char *ctid,
                                                      char *ecuid,
                                                      int id);

/**
 * @brief Initialize the message filter structure
 *
 * During initialization, the configuration file will be parsed and all
 * specified message filter configurations and injection message configurations
 * inialized. Furthermore, the specified inital filter level will be set.
 *
 * @param daemon_local  DaemonLocal
 * @param verbose       verbose flag
 * @return DLT_RETURN_OK on success, DLT error code otherwise
 */
DltReturnValue mct_daemon_prepare_message_filter(DltDaemonLocal *daemon_local,
                                                 int verbose);

/**
 * @brief Cleanup the message filter structure
 *
 * All allocated memory of the message filter will be freed. The pointer will
 * be set to NULL before the function returns.
 *
 * @param daemon_local  DaemonLocal
 * @param verbose       verbose flag
 */
void mct_daemon_cleanup_message_filter(DltDaemonLocal *daemon_local,
                                       int verbose);
#endif
