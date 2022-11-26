#ifndef DLT_DAEMON_COMMON_H
#define DLT_DAEMON_COMMON_H

/**
 * \defgroup daemonapi DLT Daemon API
 * \addtogroup daemonapi
 * \{
 */

#include <limits.h>
#include <semaphore.h>
#include <stdbool.h>
#include "mct_common.h"
#include "mct_user.h"
#include "mct_offline_logstorage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DLT_DAEMON_RINGBUFFER_MIN_SIZE    500000   /**< Ring buffer size for storing log messages while no client is connected */
#define DLT_DAEMON_RINGBUFFER_MAX_SIZE  10000000   /**< Ring buffer size for storing log messages while no client is connected */
#define DLT_DAEMON_RINGBUFFER_STEP_SIZE   500000   /**< Ring buffer size for storing log messages while no client is connected */

#define DLT_DAEMON_SEND_TO_ALL     -3   /**< Constant value to identify the command "send to all" */
#define DLT_DAEMON_SEND_FORCE      -4   /**< Constant value to identify the command "send force to all" */


/**
 * Definitions of DLT daemon logging states
 */
typedef enum
{
    DLT_DAEMON_STATE_INIT = 0,               /**< Initial state */
    DLT_DAEMON_STATE_BUFFER = 1,             /**< logging is buffered until external logger is connected or internal logging is activated */
    DLT_DAEMON_STATE_BUFFER_FULL = 2,        /**< then internal buffer is full, wait for connect from client */
    DLT_DAEMON_STATE_SEND_BUFFER = 3,        /**< external logger is connected, but buffer is still not empty or external logger queue is full */
    DLT_DAEMON_STATE_SEND_DIRECT = 4         /**< External logger is connected or internal logging is active, and buffer is empty */
} DltDaemonState;

/**
 * The parameters of a daemon application.
 */
typedef struct
{
    char apid[DLT_ID_SIZE];        /**< application id */
    pid_t pid;                     /**< process id of user application */
    int user_handle;               /**< connection handle for connection to user application */
    bool owns_user_handle;         /**< user_handle should be closed when reset */
    char *application_description; /**< context description */
    int num_contexts;              /**< number of contexts for this application */
    int blockmode_status;          /**< blockmode status information */
} DltDaemonApplication;

/**
 * The parameters of a daemon context.
 */
typedef struct
{
    char apid[DLT_ID_SIZE];    /**< application id */
    char ctid[DLT_ID_SIZE];    /**< context id */
    int8_t log_level;          /**< the current log level of the context */
    int8_t trace_status;       /**< the current trace status of the context */
    int log_level_pos;         /**< offset of context in context field on user application */
    int user_handle;           /**< connection handle for connection to user application */
    char *context_description; /**< context description */
    int8_t storage_log_level;  /**< log level set for offline logstorage */
    bool predefined;           /**< set to true if this context is predefined by runtime configuration file */
} DltDaemonContext;

/*
 * The parameter of registered users list
 */
typedef struct
{
    DltDaemonApplication *applications; /**< Pointer to applications */
    int num_applications;               /**< Number of available application */
    DltDaemonContext *contexts;         /**< Pointer to contexts */
    int num_contexts;                   /**< Total number of all contexts in all applications in this list */
    char ecu[DLT_ID_SIZE];              /**< ECU ID of where contexts are registered */
} DltDaemonRegisteredUsers;

/**
 * The parameters of a daemon.
 */
typedef struct
{
    DltDaemonRegisteredUsers *user_list;        /**< registered users per ECU */
    int num_user_lists;                         /** < number of context lists */
    int8_t default_log_level;                   /**< Default log level (of daemon) */
    int8_t default_trace_status;                /**< Default trace status (of daemon) */
    int8_t force_ll_ts;                         /**< Enforce ll and ts to not exceed default_log_level, default_trace_status */
    unsigned int overflow_counter;              /**< counts the number of lost messages. */
    int runtime_context_cfg_loaded;             /**< Set to one, if runtime context configuration has been loaded, zero otherwise */
    char ecuid[DLT_ID_SIZE];                    /**< ECU ID of daemon */
    int sendserialheader;                       /**< 1: send serial header; 0 don't send serial header */
    int timingpackets;                          /**< 1: send continous timing packets; 0 don't send continous timing packets */
    DltBuffer client_ringbuffer;                /**< Ring-buffer for storing received logs while no client connection is available */
    char runtime_application_cfg[PATH_MAX + 1]; /**< Path and filename of persistent application configuration. Set to path max, as it specifies a full path*/
    char runtime_context_cfg[PATH_MAX + 1];     /**< Path and filename of persistent context configuration */
    char runtime_configuration[PATH_MAX + 1];   /**< Path and filename of persistent configuration */
    DltUserLogMode mode;                        /**< Mode used for tracing: off, external, internal, both */
    char connectionState;                       /**< state for tracing: 0 = no client connected, 1 = client connected */
    char *ECUVersionString;                     /**< Version string to send to client. Loaded from a file at startup. May be null. */
    DltDaemonState state;                       /**< the current logging state of mct daemon. */
    DltLogStorage *storage_handle;
    int blockMode;                    /**< current active BlockMode setting. */
    int maintain_logstorage_loglevel; /* Permission to maintain the logstorage loglevel*/
} DltDaemon;

/**
 * Initialise the mct daemon structure
 * This function must be called before using further mct daemon structure
 * @param daemon pointer to mct daemon structure
 * @param RingbufferMinSize ringbuffer size
 * @param RingbufferMaxSize ringbuffer size
 * @param RingbufferStepSize ringbuffer size
 * @param runtime_directory Directory of persistent configuration
 * @param InitialContextLogLevel loglevel to be sent to context when those register with loglevel default, read from mct.conf
 * @param InitialContextTraceStatus tracestatus to be sent to context when those register with tracestatus default, read from mct.conf
 * @param ForceLLTS force default log-level
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_init(DltDaemon *daemon,
                    unsigned long RingbufferMinSize,
                    unsigned long RingbufferMaxSize,
                    unsigned long RingbufferStepSize,
                    const char *runtime_directory,
                    int InitialContextLogLevel,
                    int InitialContextTraceStatus,
                    int ForceLLTS,
                    int verbose);
/**
 * De-Initialise the mct daemon structure
 * @param daemon pointer to mct daemon structure
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_free(DltDaemon *daemon, int verbose);

/**
 * Find information about application/contexts for a specific ECU
 * @param daemon pointer to mct daemon structure
 * @param ecu pointer to node name
 * @param verbose if set to true verbose information is printed out
 * @return pointer to user list, NULL otherwise
 */
DltDaemonRegisteredUsers *mct_daemon_find_users_list(DltDaemon *daemon,
                                                     char *ecu,
                                                     int verbose);
/**
 * Init the user saved configurations to daemon.
 * Since the order of loading runtime config could be different,
 * this function won't be the place to do that.
 * This is just for preparation of real load later.
 * @param daemon pointer to mct daemon structure
 * @param runtime_directory directory path
 * @param verbose if set to true verbose information is printed out
 * @return DLT_RETURN_OK on success, DLT_RETURN_ERROR otherwise
 */
int mct_daemon_init_runtime_configuration(DltDaemon *daemon,
                                          const char *runtime_directory,
                                          int verbose);

/**
 * Add (new) application to internal application management
 * @param daemon pointer to mct daemon structure
 * @param apid pointer to application id
 * @param pid process id of user application
 * @param description description of application
 * @param fd file descriptor of application
 * @param ecu pointer to ecu id of node to add applications
 * @param verbose if set to true verbose information is printed out.
 * @return Pointer to added context, null pointer on error
 */
DltDaemonApplication *mct_daemon_application_add(DltDaemon *daemon,
                                                 char *apid,
                                                 pid_t pid,
                                                 char *description,
                                                 int fd,
                                                 char *ecu,
                                                 int verbose);
/**
 * Delete application from internal application management
 * @param daemon pointer to mct daemon structure
 * @param application pointer to application to be deleted
 * @param ecu pointer to ecu id of node to delete applications
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_application_del(DltDaemon *daemon,
                               DltDaemonApplication *application,
                               char *ecu,
                               int verbose);
/**
 * Find application with specific application id
 * @param daemon pointer to mct daemon structure
 * @param apid pointer to application id
 * @param ecu pointer to ecu id of node to clear applications
 * @param verbose if set to true verbose information is printed out.
 * @return Pointer to application, null pointer on error or not found
 */
DltDaemonApplication *mct_daemon_application_find(DltDaemon *daemon,
                                                  char *apid,
                                                  char *ecu,
                                                  int verbose);
/**
 * Load applications from file to internal context management
 * @param daemon pointer to mct daemon structure
 * @param filename name of file to be used for loading
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_applications_load(DltDaemon *daemon, const char *filename, int verbose);
/**
 * Save applications from internal context management to file
 * @param daemon pointer to mct daemon structure
 * @param filename name of file to be used for saving
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_applications_save(DltDaemon *daemon, const char *filename, int verbose);
/**
 * Invalidate all applications fd, if fd is reused
 * @param daemon pointer to mct daemon structure
 * @param ecu node these applications running on.
 * @param fd file descriptor
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_applications_invalidate_fd(DltDaemon *daemon,
                                          char *ecu,
                                          int fd,
                                          int verbose);
/**
 * Clear all applications in internal application management of specific ecu
 * @param daemon pointer to mct daemon structure
 * @param ecu pointer to ecu id of node to clear applications
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_applications_clear(DltDaemon *daemon, char *ecu, int verbose);

/**
 * Add (new) context to internal context management
 * @param daemon pointer to mct daemon structure
 * @param apid pointer to application id
 * @param ctid pointer to context id
 * @param log_level log level of context
 * @param trace_status trace status of context
 * @param log_level_pos offset of context in context field on user application
 * @param user_handle connection handle for connection to user application
 * @param description description of context
 * @param ecu pointer to ecu id of node to add application
 * @param verbose if set to true verbose information is printed out.
 * @return Pointer to added context, null pointer on error
 */
DltDaemonContext *mct_daemon_context_add(DltDaemon *daemon,
                                         char *apid,
                                         char *ctid,
                                         int8_t log_level,
                                         int8_t trace_status,
                                         int log_level_pos,
                                         int user_handle,
                                         char *description,
                                         char *ecu,
                                         int verbose);
/**
 * Delete context from internal context management
 * @param daemon pointer to mct daemon structure
 * @param context pointer to context to be deleted
 * @param ecu pointer to ecu id of node to delete application
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_context_del(DltDaemon *daemon,
                           DltDaemonContext *context,
                           char *ecu,
                           int verbose);
/**
 * Find context with specific application id and context id
 * @param daemon pointer to mct daemon structure
 * @param apid pointer to application id
 * @param ctid pointer to context id
 * @param ecu pointer to ecu id of node to clear applications
 * @param verbose if set to true verbose information is printed out.
 * @return Pointer to context, null pointer on error or not found
 */
DltDaemonContext *mct_daemon_context_find(DltDaemon *daemon,
                                          char *apid,
                                          char *ctid,
                                          char *ecu,
                                          int verbose);
/**
 * Invalidate all contexts fd, if fd is reused
 * @param daemon pointer to mct daemon structure
 * @param ecu node these contexts running on.
 * @param fd file descriptor
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_contexts_invalidate_fd(DltDaemon *daemon,
                                      char *ecu,
                                      int fd,
                                      int verbose);
/**
 * Clear all contexts in internal context management of specific ecu
 * @param daemon pointer to mct daemon structure
 * @param ecu pointer to ecu id of node to clear contexts
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_contexts_clear(DltDaemon *daemon, char *ecu, int verbose);
/**
 * Load contexts from file to internal context management
 * @param daemon pointer to mct daemon structure
 * @param filename name of file to be used for loading
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_contexts_load(DltDaemon *daemon, const char *filename, int verbose);
/**
 * Save contexts from internal context management to file
 * @param daemon pointer to mct daemon structure
 * @param filename name of file to be used for saving
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_contexts_save(DltDaemon *daemon, const char *filename, int verbose);
/**
 * Load persistant configuration
 * @param daemon pointer to mct daemon structure
 * @param filename name of file to be used for loading
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_configuration_load(DltDaemon *daemon, const char *filename, int verbose);
/**
 * Save configuration persistantly
 * @param daemon pointer to mct daemon structure
 * @param filename name of file to be used for saving
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_configuration_save(DltDaemon *daemon, const char *filename, int verbose);


/**
 * Send user message DLT_USER_MESSAGE_LOG_LEVEL to user application
 * @param daemon pointer to mct daemon structure
 * @param context pointer to context for response
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_user_send_log_level(DltDaemon *daemon, DltDaemonContext *context, int verbose);

/**
 * Send user message DLT_USER_MESSAGE_LOG_STATE to user application
 * @param daemon pointer to mct daemon structure
 * @param app pointer to application for response
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_user_send_log_state(DltDaemon *daemon, DltDaemonApplication *app, int verbose);

/**
 * Send user messages to all user applications using default context, or trace status
 * to update those values
 * @param daemon pointer to mct daemon structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_user_send_default_update(DltDaemon *daemon, int verbose);

/**
 * Send user messages to all user applications context to update with the new log level
 * @param daemon pointer to mct daemon structure
 * @param log_level new log level to be set
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_user_send_all_log_level_update(DltDaemon *daemon, int8_t log_level, int verbose);

/**
 * Send user messages to all user applications context to update with the new trace status
 * @param daemon pointer to mct daemon structure
 * @param trace_status new trace status to be set
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_user_send_all_trace_status_update(DltDaemon *daemon,
                                                  int8_t trace_status,
                                                  int verbose);

/**
 * Send user messages to all user applications the log status
 * everytime the client is connected or disconnected.
 * @param daemon pointer to mct daemon structure
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_user_send_all_log_state(DltDaemon *daemon, int verbose);

/**
 * Process reset to factory default control message
 * @param daemon pointer to mct daemon structure
 * @param filename name of file containing the runtime defaults for applications
 * @param filename1 name of file containing the runtime defaults for contexts
 * @param InitialContextLogLevel loglevel to be sent to context when those register with loglevel default, read from mct.conf
 * @param InitialContextTraceStatus tracestatus to be sent to context when those register with tracestatus default, read from mct.conf
 * @param InitialEnforceLlTsStatus force default log-level
 * @param verbose if set to true verbose information is printed out.
 */
void mct_daemon_control_reset_to_factory_default(DltDaemon *daemon,
                                                 const char *filename,
                                                 const char *filename1,
                                                 int InitialContextLogLevel,
                                                 int InitialContextTraceStatus,
                                                 int InitialEnforceLlTsStatus,
                                                 int verbose);

/**
 * Change the logging state of mct daemon
 * @param daemon pointer to mct daemon structure
 * @param newState the requested new state
 */
void mct_daemon_change_state(DltDaemon *daemon, DltDaemonState newState);

/**
 * Send user message DLT_USER_MESSAGE_SET_BLOCK_MODE to user application with
 * specified BlockMode
 * @param daemon pointer to mct daemon structure
 * @param name pointer to application name to which block mode is sent
 * @param block_mode new blockmode to be send to applications
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_daemon_user_update_blockmode(DltDaemon *daemon,
                                     char *name,
                                     int block_mode,
                                     int verbose);
#ifdef __cplusplus
}
#endif

/**
 * \}
 */

#endif /* DLT_DAEMON_COMMON_H */
