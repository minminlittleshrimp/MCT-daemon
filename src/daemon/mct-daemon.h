#ifndef MCT_DAEMON_H
#define MCT_DAEMON_H

#include <limits.h> /* for NAME_MAX */
#include <sys/time.h>

#include "mct_daemon_common.h"
#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"
#include "mct_daemon_event_handler_types.h"
#include "mct_daemon_filter_types.h"
#include "mct_offline_trace.h"

#define MCT_DAEMON_FLAG_MAX 256

#define MCT_DAEMON_BLOCK_MODE_DISABLED 0
#define MCT_DAEMON_BLOCK_MODE_ENABLED 1

/**
 * The flags of a mct daemon.
 */
typedef struct
{
    int aflag;                                          /**< (Boolean) Print MCT messages; payload as ASCII */
    int sflag;                                          /**< (Boolean) Print MCT messages; payload as hex */
    int xflag;                                          /**< (Boolean) Print MCT messages; only headers */
    int vflag;                                          /**< (Boolean) Verbose mode */
    int dflag;                                          /**< (Boolean) Daemonize */
    int lflag;                                          /**< (Boolean) Send MCT messages with serial header */
    int rflag;                                          /**< (Boolean) Send automatic get log info response during context registration */
    int mflag;                                          /**< (Boolean) Sync to serial header on serial connection */
    int nflag;                                          /**< (Boolean) Sync to serial header on all TCP connections */
    char evalue[NAME_MAX + 1];                          /**< (String: ECU ID) Set ECU ID (Default: ECU1) */
    char bvalue[NAME_MAX + 1];                          /**< (String: Baudrate) Serial device baudrate (Default: 115200) */
    char yvalue[NAME_MAX + 1];                          /**< (String: Devicename) Additional support for serial device */
    char ivalue[NAME_MAX + 1];                          /**< (String: Directory) Directory where to store the persistant configuration (Default: /tmp) */
    char cvalue[NAME_MAX + 1];                          /**< (String: Directory) Filename of MCT configuration file (Default: /etc/mct.conf) */
    int sharedMemorySize;                               /**< (int) Size of shared memory (Default: 100000) */
    int sendMessageTime;                                /**< (Boolean) Send periodic Message Time if client is connected (Default: 0) */
    char offlineTraceDirectory[MCT_DAEMON_FLAG_MAX];    /**< (String: Directory) Store MCT messages to local directory (Default: /etc/mct.conf) */
    int offlineTraceFileSize;                           /**< (int) Maximum size in bytes of one trace file (Default: 1000000) */
    int offlineTraceMaxSize;                            /**< (int) Maximum size of all trace files (Default: 4000000) */
    int offlineTraceFilenameTimestampBased;             /**< (int) timestamp based or index based (Default: 1 Timestamp based) */
    int loggingMode;                                    /**< (int) The logging console for internal logging of mct-daemon (Default: 0) */
    int loggingLevel;                                   /**< (int) The logging level for internal logging of mct-daemon (Default: 6) */
    char loggingFilename[MCT_DAEMON_FLAG_MAX];          /**< (String: Filename) The logging filename if internal logging mode is log to file (Default: /tmp/log) */
    int sendECUSoftwareVersion;                         /**< (Boolean) Send ECU software version perdiodically */
    char pathToECUSoftwareVersion[MCT_DAEMON_FLAG_MAX]; /**< (String: Filename) The file from which to read the ECU version from. */
    int sendTimezone;                                   /**< (Boolean) Send Timezone perdiodically */
    int offlineLogstorageMaxDevices;                    /**< (int) Maximum devices to be used as offline logstorage devices */
    char offlineLogstorageDirPath[MCT_MOUNT_PATH_MAX];  /**< (String: Directory) DIR path to store offline logs  */
    int offlineLogstorageTimestamp;                     /**< (int) Append timestamp in offline logstorage filename */
    char offlineLogstorageDelimiter;                    /**< (char) Append delimeter character in offline logstorage filename  */
    unsigned int offlineLogstorageMaxCounter;           /**< (int) Maximum offline logstorage file counter index until wraparound  */
    unsigned int offlineLogstorageMaxCounterIdx;        /**< (int) String len of  offlineLogstorageMaxCounter*/
    unsigned int offlineLogstorageCacheSize;            /**< (int) Max cache size offline logstorage cache */
    int offlineLogstorageOptionalCounter;               /**< (Boolean) Do not append index to filename if NOFiles=1 */
#ifdef MCT_DAEMON_USE_UNIX_SOCKET_IPC
    char appSockPath[MCT_DAEMON_FLAG_MAX]; /**< Path to User socket */
#else /* MCT_DAEMON_USE_FIFO_IPC */
    char userPipesDir[MCT_PATH_MAX];    /**< (String: Directory) directory where mctpipes reside (Default: /tmp/mctpipes) */
    char daemonFifoName[MCT_PATH_MAX];  /**< (String: Filename) name of local fifo (Default: /tmp/mct) */
    char daemonFifoGroup[MCT_PATH_MAX]; /**< (String: Group name) Owner group of local fifo (Default: Primary Group) */
#endif

    unsigned int port;                           /**< port number */
    char ctrlSockPath[MCT_DAEMON_FLAG_MAX];      /**< Path to Control socket */
    int autoResponseGetLogInfoOption;            /**< (int) The Option of automatic get log info response during context registration. (Default: 7)*/
    int contextLogLevel;                         /**< (int) log level sent to context if registered with default log-level or if enforced*/
    int contextTraceStatus;                      /**< (int) trace status sent to context if registered with default trace status  or if enforced*/
    int enforceContextLLAndTS;                   /**< (Boolean) Enforce log-level, trace-status not to exceed contextLogLevel, contextTraceStatus */
    MctBindAddress_t *ipNodes;                   /**< (String: BindAddress) The daemon accepts connections only on this list of IP addresses */
    int injectionMode;                           /**< (Boolean) Injection mode */
    char msgFilterConfFile[MCT_DAEMON_FLAG_MAX]; /**< Filter config file path */
    int blockModeAllowed;                        /** (int) The BlockMode Allowance flag (Default: 0 - Not allowed) */
} MctDaemonFlags;
/**
 * The global parameters of a mct daemon.
 */
typedef struct
{
    MctDaemonFlags flags;            /**< flags of the daemon */
    MctFile file;                    /**< struct for file access */
    MctEventHandler pEvent;          /**< struct for message producer event handling */
    MctMessage msg;                  /**< one mct message */
    int client_connections;          /**< counter for nr. of client connections */
    int internal_client_connections; /**< counter for nr. of internal client connections */
    size_t baudrate;                 /**< Baudrate of serial connection */

    MctOfflineTrace offlineTrace; /**< Offline trace handling */
    int timeoutOnSend;
    unsigned long RingbufferMinSize;
    unsigned long RingbufferMaxSize;
    unsigned long RingbufferStepSize;
    unsigned long daemonFifoSize;

    MctMessageFilter pFilter; /**< struct for message filter handling */
} MctDaemonLocal;

typedef struct
{
    unsigned long long wakeups_missed;
    int period_sec;
    int starts_in;
    int timer_id;
} MctDaemonPeriodicData;

typedef struct
{
    MctDaemon *daemon;
    MctDaemonLocal *daemon_local;
} MctDaemonTimingPacketThreadData;

typedef MctDaemonTimingPacketThreadData MctDaemonECUVersionThreadData;

#define MCT_DAEMON_ERROR_OK             0
#define MCT_DAEMON_ERROR_UNKNOWN         -1
#define MCT_DAEMON_ERROR_BUFFER_FULL     -2
#define MCT_DAEMON_ERROR_SEND_FAILED     -3
#define MCT_DAEMON_ERROR_WRITE_FAILED     -4

/* Function prototypes */
void mct_daemon_local_cleanup(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
int mct_daemon_local_init_p1(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
int mct_daemon_local_init_p2(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
int mct_daemon_local_connection_init(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
int mct_daemon_local_ecu_version_init(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);

void mct_daemon_daemonize(int verbose);
void mct_daemon_exit_trigger();
void mct_daemon_signal_handler(int sig);
int mct_daemon_process_client_connect(MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      MctReceiver *recv,
                                      int verbose);
int mct_daemon_process_client_messages(MctDaemon *daemon,
                                       MctDaemonLocal *daemon_local,
                                       MctReceiver *revc,
                                       int verbose);
int mct_daemon_process_client_messages_serial(MctDaemon *daemon,
                                              MctDaemonLocal *daemon_local,
                                              MctReceiver *recv,
                                              int verbose);
int mct_daemon_process_user_messages(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *recv,
                                     int verbose);
int mct_daemon_process_one_s_timer(MctDaemon *daemon,
                                   MctDaemonLocal *daemon_local,
                                   MctReceiver *recv,
                                   int verbose);
int mct_daemon_process_sixty_s_timer(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *recv,
                                     int verbose);
int mct_daemon_process_systemd_timer(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *recv,
                                     int verbose);

int mct_daemon_process_control_connect(MctDaemon *daemon,
                                       MctDaemonLocal *daemon_local,
                                       MctReceiver *recv,
                                       int verbose);
#if defined MCT_DAEMON_USE_UNIX_SOCKET_IPC
int mct_daemon_process_app_connect(MctDaemon *daemon,
                                   MctDaemonLocal *daemon_local,
                                   MctReceiver *recv,
                                   int verbose);
#endif
int mct_daemon_process_control_messages(MctDaemon *daemon,
                                        MctDaemonLocal *daemon_local,
                                        MctReceiver *recv,
                                        int verbose);

typedef int (*mct_daemon_process_user_message_func)(MctDaemon *daemon, MctDaemonLocal *daemon_local,
                                                    MctReceiver *rec,
                                                    int verbose);

int mct_daemon_process_user_message_overflow(MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             MctReceiver *rec,
                                             int verbose);
int mct_daemon_send_message_overflow(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
int mct_daemon_process_user_message_register_application(MctDaemon *daemon,
                                                         MctDaemonLocal *daemon_local,
                                                         MctReceiver *rec,
                                                         int verbose);
int mct_daemon_process_user_message_unregister_application(MctDaemon *daemon,
                                                           MctDaemonLocal *daemon_local,
                                                           MctReceiver *rec,
                                                           int verbose);
int mct_daemon_process_user_message_register_context(MctDaemon *daemon,
                                                     MctDaemonLocal *daemon_local,
                                                     MctReceiver *rec,
                                                     int verbose);
int mct_daemon_process_user_message_unregister_context(MctDaemon *daemon,
                                                       MctDaemonLocal *daemon_local,
                                                       MctReceiver *rec,
                                                       int verbose);
int mct_daemon_process_user_message_log(MctDaemon *daemon,
                                        MctDaemonLocal *daemon_local,
                                        MctReceiver *rec,
                                        int verbose);
int mct_daemon_process_user_message_set_app_ll_ts(MctDaemon *daemon,
                                                  MctDaemonLocal *daemon_local,
                                                  MctReceiver *rec,
                                                  int verbose);
int mct_daemon_process_user_message_marker(MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctReceiver *rec,
                                           int verbose);

int mct_daemon_send_ringbuffer_to_client(MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         int verbose);
void mct_daemon_timingpacket_thread(void *ptr);
void mct_daemon_ecu_version_thread(void *ptr);

int create_timer_fd(MctDaemonLocal *daemon_local, int period_sec, int starts_in, MctTimers timer);

int mct_daemon_close_socket(int sock, MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
int mct_daemon_client_update(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose);
#endif /* MCT_DAEMON_H */

