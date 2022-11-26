#ifndef DLT_DAEMON_OFFLINE_LOGSTORAGE_H
#define DLT_DAEMON_OFFLINE_LOGSTORAGE_H

#include "mct-daemon.h"
#include "mct_daemon_common.h"

#include "mct_offline_logstorage.h"

#define DLT_DAEMON_LOGSTORAGE_RESET_LOGLEVEL            -1
#define DLT_DAEMON_LOGSTORAGE_RESET_SEND_LOGLEVEL        0

typedef enum {
    DLT_DAEMON_LOGSTORAGE_CMP_MIN = 0,
    DLT_DAEMON_LOGSTORAGE_CMP_APID = 1,
    DLT_DAEMON_LOGSTORAGE_CMP_CTID = 2,
    DLT_DAEMON_LOGSTORAGE_CMP_ECID = 3,
    DLT_DAEMON_LOGSTORAGE_CMP_MAX,
} DltCompareFlags;

/**
 * mct_daemon_logstorage_get_loglevel
 *
 * Obtain log level as a union of all configured storage devices and filters for
 * the provided application id and context id
 *
 * @param daemon        Pointer to DLT Daemon structure
 * @param max_device    Maximum storage devices setup by the daemon
 * @param apid          Application ID
 * @param ctid          Context ID
 * @return              Log level on success, -1 on error
 */
int mct_daemon_logstorage_get_loglevel(DltDaemon *daemon,
                                       int max_device,
                                       char *apid,
                                       char *ctid);
/**
 * mct_daemon_logstorage_reset_application_loglevel
 *
 * Reset storage log level of all running applications with -1
 *
 * @param daemon        Pointer to DLT Daemon structure
 * @param daemon_local  Pointer to DLT Daemon local structure
 * @param dev_num       Number of attached DLT Logstorage device
 * @param verbose       If set to true verbose information is printed out
 */
void mct_daemon_logstorage_reset_application_loglevel(DltDaemon *daemon,
                                                      DltDaemonLocal *daemon_local,
                                                      int dev_num,
                                                      int max_device,
                                                      int verbose);
/**
 * mct_daemon_logstorage_update_application_loglevel
 *
 * Update log level of all running applications with new filter configuration
 * available due to newly attached DltLogstorage device. The log level is only
 * updated when the current application log level is less than the log level
 * obtained from the storage configuration file.
 *
 * @param daemon        Pointer to DLT Daemon structure
 * @param daemon_local  Pointer to DLT Daemon local structure
 * @param dev_num       Number of attached DLT Logstorage device
 * @param verbose       if set to true verbose information is printed out
 */
void mct_daemon_logstorage_update_application_loglevel(DltDaemon *daemon,
                                                       DltDaemonLocal *daemon_local,
                                                       int dev_num,
                                                       int verbose);

/**
 * mct_daemon_logstorage_write
 *
 * Write log message to all attached storage device. If the called
 * mct_logstorage_write function is not able to write to the device,
 * DltDaemon will disconnect this device.
 *
 * @param daemon        Pointer to Dlt Daemon structure
 * @param user_config   DltDaemon configuration
 * @param data1         message header buffer
 * @param size1         message header buffer size
 * @param data2         message extended data buffer
 * @param size2         message extended data size
 * @param data3         message data buffer
 * @param size3         message data size
 * @return              0 on success, -1 on error, 1 on disable network routing
 */
int mct_daemon_logstorage_write(DltDaemon *daemon,
                                 DltDaemonFlags *user_config,
                                 unsigned char *data1,
                                 int size1,
                                 unsigned char *data2,
                                 int size2,
                                 unsigned char *data3,
                                 int size3);

/**
 * mct_daemon_logstorage_setup_internal_storage
 *
 * Setup user defined path as offline log storage device
 *
 * @param daemon        Pointer to Dlt Daemon structure
 * @param daemon_local  Pointer to Dlt Daemon Local structure
 * @param path          User configured internal storage path
 * @param bm_allowed    Flag to indicate blockMode allowance
 * @param verbose       If set to true verbose information is printed out
 */
int mct_daemon_logstorage_setup_internal_storage(DltDaemon *daemon,
                                                 DltDaemonLocal *daemon_local,
                                                 char *path,
                                                 int bm_allowed,
                                                 int verbose);

/**
 * Set max size of logstorage cache. Stored internally in bytes
 *
 * @param size  Size of logstorage cache [in KB]
 */
void mct_daemon_logstorage_set_logstorage_cache_size(unsigned int size);

/**
 * Cleanup mct logstorage
 *
 * @param daemon       Pointer to Dlt Daemon structure
 * @param daemon_local Pointer to Dlt Daemon Local structure
 * @param verbose      If set to true verbose information is printed out
 */
int mct_daemon_logstorage_cleanup(DltDaemon *daemon,
                                  DltDaemonLocal *daemon_local,
                                  int verbose);

/**
 * Sync logstorage caches
 *
 * @param daemon        Pointer to Dlt Daemon structure
 * @param daemon_local  Pointer to Dlt Daemon Local structure
 * @param mnt_point     Logstorage device mount point
 * @param verbose       If set to true verbose information is printed out
 * @return 0 on success, -1 otherwise
 */
int mct_daemon_logstorage_sync_cache(DltDaemon *daemon,
                                     DltDaemonLocal *daemon_local,
                                     char *mnt_point,
                                     int verbose);

/**
 * mct_logstorage_get_device
 *
 * Get a Logstorage device handle for given the mount point.
 *
 * @param daemon        Pointer to Dlt Daemon structure
 * @param daemon_local  Pointer to Dlt Daemon Local structure
 * @param mnt_point     Logstorage device mount point
 * @param verbose       If set to true verbose information is printed out
 * @return handle to Logstorage device on success, NULL otherwise
 */
DltLogStorage *mct_daemon_logstorage_get_device(DltDaemon *daemon,
                                                DltDaemonLocal *daemon_local,
                                                char *mnt_point,
                                                int verbose);

#endif /* DLT_DAEMON_OFFLINE_LOGSTORAGE_H */
