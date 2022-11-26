#ifndef MCT_DAEMON_CLIENT_H
#define MCT_DAEMON_CLIENT_H

#include <limits.h> /* for NAME_MAX */

#include "mct_daemon_common.h"
#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"

#include <mct_offline_trace.h>
#include <sys/time.h>

/**
 * Send out message to client or store message in offline trace.
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param storage_header pointer to data
 * @param storage_header_size size of data
 * @param data1 pointer to data
 * @param size1 size of data
 * @param data2 pointer to data
 * @param size2 size of data
 * @param verbose if set to true verbose information is printed out.
 * @return unequal 0 if there is an error or buffer is full
 */
int mct_daemon_client_send(int sock,
                           MctDaemon *daemon,
                           MctDaemonLocal *daemon_local,
                           void *storage_header,
                           int storage_header_size,
                           void *data1,
                           int size1,
                           void *data2,
                           int size2,
                           int verbose);
/**
 * Send out message to all client or store message in offline trace.
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 * @return 0 if success, less than 0 if there is an error or buffer is full
 */
int mct_daemon_client_send_message_to_all_client(MctDaemon *daemon,
                                                 MctDaemonLocal *daemon_local,
                                                 int verbose);
/**
 * Send out response message to mct client
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to response message
 * @param apid pointer to application id to be used in response message
 * @param ctid pointer to context id to be used in response message
 * @param verbose if set to true verbose information is printed out.
 * @return -1 if there is an error or buffer is full
 */
int mct_daemon_client_send_control_message(int sock,
                                           MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctMessage *msg,
                                           char *apid,
                                           char *ctid,
                                           int verbose);
/**
 * Process and generate response to received get log info control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_get_log_info(int sock,
                                     MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctMessage *msg,
                                     int verbose);
/**
 * Process and generate response to received get software version control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_get_software_version(int sock,
                                             MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             int verbose);
/**
 * Process and generate response to received get default log level control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_get_default_log_level(int sock,
                                              MctDaemon *daemon,
                                              MctDaemonLocal *daemon_local,
                                              int verbose);
/**
 * Process and generate response to message buffer overflow control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param overflow_counter Overflow counter
 * @param apid Application ID
 * @param verbose if set to true verbose information is printed out.
 * @return -1 if there is an error or buffer overflow, else 0
 */
int mct_daemon_control_message_buffer_overflow(int sock,
                                               MctDaemon *daemon,
                                               MctDaemonLocal *daemon_local,
                                               unsigned int overflow_counter,
                                               char *apid,
                                               int verbose);
/**
 * Generate response to control message from mct client
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param service_id service id of control message
 * @param status status of response (e.g. ok, not supported, error)
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_service_response(int sock,
                                         MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         uint32_t service_id,
                                         int8_t status,
                                         int verbose);
/**
 * Send control message unregister context (add on to AUTOSAR standard)
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param apid application id to be unregisteres
 * @param ctid context id to be unregistered
 * @param comid Communication id where apid is unregistered
 * @param verbose if set to true verbose information is printed out.
 */
int mct_daemon_control_message_unregister_context(int sock,
                                                  MctDaemon *daemon,
                                                  MctDaemonLocal *daemon_local,
                                                  char *apid,
                                                  char *ctid,
                                                  char *comid,
                                                  int verbose);
/**
 * Send control message connection info (add on to AUTOSAR standard)
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param state state of connection
 * @param comid Communication id where connection state changed
 * @param verbose if set to true verbose information is printed out.
 */
int mct_daemon_control_message_connection_info(int sock,
                                               MctDaemon *daemon,
                                               MctDaemonLocal *daemon_local,
                                               uint8_t state,
                                               char *comid,
                                               int verbose);
/**
 * Send control message timezone (add on to AUTOSAR standard)
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
int mct_daemon_control_message_timezone(int sock,
                                        MctDaemon *daemon,
                                        MctDaemonLocal *daemon_local,
                                        int verbose);
/**
 * Send control message marker (add on to AUTOSAR standard)
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
int mct_daemon_control_message_marker(int sock,
                                      MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      int verbose);
/**
 * Process received control message from mct client
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
int mct_daemon_client_process_control(int sock,
                                      MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      MctMessage *msg,
                                      int verbose);
/**
 * Process and generate response to received sw injection control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received sw injection control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_callsw_cinjection(int sock,
                                          MctDaemon *daemon,
                                          MctDaemonLocal *daemon_local,
                                          MctMessage *msg,
                                          int verbose);
/**
 * Process and generate response to received set log level control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_log_level(int sock,
                                      MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      MctMessage *msg,
                                      int verbose);
/**
 * Process and generate response to received set trace status control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_trace_status(int sock,
                                         MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         MctMessage *msg,
                                         int verbose);
/**
 * Process and generate response to received set default log level control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_default_log_level(int sock,
                                              MctDaemon *daemon,
                                              MctDaemonLocal *daemon_local,
                                              MctMessage *msg,
                                              int verbose);
/**
 * Process and generate response to received set all log level control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_all_log_level(int sock,
                                          MctDaemon *daemon,
                                          MctDaemonLocal *daemon_local,
                                          MctMessage *msg,
                                          int verbose);

/**
 * Process and generate response to received set default trace status control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_default_trace_status(int sock,
                                                 MctDaemon *daemon,
                                                 MctDaemonLocal *daemon_local,
                                                 MctMessage *msg,
                                                 int verbose);
/**
 * Process and generate response to received set all trace status control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_all_trace_status(int sock,
                                             MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             MctMessage *msg,
                                             int verbose);
/**
 * Process and generate response to set timing packets control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_timing_packets(int sock,
                                           MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctMessage *msg,
                                           int verbose);
/**
 * Send time control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_message_time(int sock,
                                     MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     int verbose);
/**
 * Service offline logstorage command request
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_service_logstorage(int sock,
                                           MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctMessage *msg,
                                           int verbose);

/**
 * Process and generate response to received passive node connect control
 * message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_passive_node_connect(int sock,
                                             MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             MctMessage *msg,
                                             int verbose);
/**
 * Process and generate response to received passive node connection status
 * control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_passive_node_connect_status(int sock,
                                                    MctDaemon *daemon,
                                                    MctDaemonLocal *daemon_local,
                                                    int verbose);
/**
 * Process and generate response to received set filter level control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_filter_level(int sock,
                                         MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         MctMessage *msg,
                                         int verbose);
/**
 * Process and generate response to received get filter status control message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_get_filter_status(int sock,
                                          MctDaemon *daemon,
                                          MctDaemonLocal *daemon_local,
                                          int verbose);
/**
 * Process and generate response to received set BlockMode message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_set_block_mode(int sock,
                                       MctDaemon *daemon,
                                       MctDaemonLocal *daemon_local,
                                       MctMessage *msg,
                                       int verbose);
/**
 * Process and generate response to received get BlockMode message
 * @param sock connection handle used for sending response
 * @param daemon pointer to mct daemon structure
 * @param daemon_local pointer to mct daemon local structure
 * @param msg pointer to received control message
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_get_block_mode(int sock,
                                       MctDaemon *daemon,
                                       MctDaemonLocal *daemon_local,
                                       MctMessage *msg,
                                       int verbose);
#endif /* MCT_DAEMON_CLIENT_H */
