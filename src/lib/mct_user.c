#include <stdlib.h> /* for getenv(), free(), atexit() */
#include <string.h> /* for strcmp(), strncmp(), strlen(), memset(), memcpy() */
#include <signal.h> /* for signal(), SIGPIPE, SIG_IGN */

#if !defined (__WIN32__)
#include <syslog.h>    /* for LOG_... */
#include <semaphore.h>
#include <pthread.h>    /* POSIX Threads */
#endif

#include <sys/time.h>
#include <math.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/uio.h> /* writev() */
#include <poll.h>

#include <limits.h>
#ifdef linux
#include <sys/prctl.h> /* for PR_SET_NAME */
#endif

#include <sys/types.h> /* needed for getpid() */
#include <unistd.h>

#include <stdbool.h>

#include <stdatomic.h>

#if defined MCT_LIB_USE_UNIX_SOCKET_IPC
#include <sys/socket.h>
#endif
#ifdef MCT_LIB_USE_UNIX_SOCKET_IPC
#include <sys/un.h>
#endif


#include "mct_user.h"
#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"
#include "mct_user_cfg.h"

#ifdef MCT_FATAL_LOG_RESET_ENABLE
#define MCT_LOG_FATAL_RESET_TRAP(LOGLEVEL) \
    do {                                   \
        if (LOGLEVEL == MCT_LOG_FATAL) {   \
            int *p = NULL;                 \
            *p = 0;                        \
        }                                  \
    } while (0)
#else /* MCT_FATAL_LOG_RESET_ENABLE */
#define MCT_LOG_FATAL_RESET_TRAP(LOGLEVEL)
#endif /* MCT_FATAL_LOG_RESET_ENABLE */

#ifndef MCT_HP_LOG_ENABLE
static MctUser mct_user;
#else
MctUser mct_user;
#endif
static atomic_bool mct_user_initialised = false;
static int mct_user_freeing = 0;
static bool mct_user_file_reach_max = false;
static bool mct_user_app_unreg = true;

#ifdef MCT_LIB_USE_FIFO_IPC
static char mct_user_dir[MCT_PATH_MAX];
static char mct_daemon_fifo[MCT_PATH_MAX];
#endif

#ifndef MCT_HP_LOG_ENABLE
static sem_t mct_mutex;
#else
sem_t mct_mutex;
#endif
static pthread_t mct_housekeeperthread_handle;

/* calling mct_user_atexit_handler() second time fails with error message */
static int atexit_registered = 0;

/* used to disallow MCT usage in fork() child */
static int g_mct_is_child = 0;
/* String truncate message */
static const char STR_TRUNCATED_MESSAGE[] = "... <<Message truncated, too long>>";

/* Enum for type of string */
enum StringType
{
    ASCII_STRING = 0,
    UTF8_STRING = 1
};

/* Data type holding "Variable Info" (VARI) properties
 * Some of the supported data types (eg. bool, string, raw) have only "name", but not "unit".
 */
typedef struct VarInfo
{
    const char *name;  // the "name" attribute (can be NULL)
    const char *unit;  // the "unit" attribute (can be NULL)
    bool with_unit;    // true if the "unit" field is to be considered
} VarInfo;

#define MCT_UNUSED(x) (void)(x)

/* Thread definitions */
#define MCT_USER_NO_THREAD            0
#define MCT_USER_HOUSEKEEPER_THREAD  (1 << 1)

/* Mutex to wait on buffer flushed to FIFO */
pthread_mutex_t flush_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_free = PTHREAD_COND_INITIALIZER;
int g_mct_buffer_empty = 1;
int g_mct_buffer_full = 0;

/* use these variables from common.c*/
extern int logging_mode;
extern FILE *logging_handle;

void mct_lock_mutex(pthread_mutex_t *mutex)
{
    int32_t lock_mutex_result = pthread_mutex_lock(mutex);

    if (lock_mutex_result != 0) {
        mct_vlog(LOG_ERR,
                 "Mutex lock failed unexpected pid=%i with result %i!\n",
                 getpid(), lock_mutex_result);
    }
}

void mct_unlock_mutex(pthread_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

/* Structure to pass data to segmented thread */
typedef struct
{
    MctContext *handle;
    uint32_t id;
    MctNetworkTraceType nw_trace_type;
    uint32_t header_len;
    void *header;
    uint32_t payload_len;
    void *payload;
} s_segmented_data;

/* Function prototypes for internally used functions */
static void mct_user_housekeeperthread_function(void *ptr);
static void mct_user_atexit_handler(void);
static MctReturnValue mct_user_log_init(MctContext *handle, MctContextData *log);
static MctReturnValue mct_user_log_send_log(MctContextData *log, int mtype);
static MctReturnValue mct_user_log_send_register_application(void);
static MctReturnValue mct_user_log_send_unregister_application(void);
static MctReturnValue mct_user_log_send_register_context(MctContextData *log);
static MctReturnValue mct_user_log_send_unregister_context(MctContextData *log);
static MctReturnValue mct_send_app_ll_ts_limit(const char *apid,
                                               MctLogLevelType loglevel,
                                               MctTraceStatusType tracestatus);
static MctReturnValue mct_user_log_send_marker();
static MctReturnValue mct_user_print_msg(MctMessage *msg, MctContextData *log);
static MctReturnValue mct_user_log_check_user_message(void);
static void mct_user_log_reattach_to_daemon(void);
static MctReturnValue mct_user_log_send_overflow(void);
static MctReturnValue mct_user_set_blockmode(int8_t mode);
static int mct_user_get_blockmode();
static MctReturnValue mct_user_log_out_error_handling(void *ptr1,
                                                      size_t len1,
                                                      void *ptr2,
                                                      size_t len2,
                                                      void *ptr3,
                                                      size_t len3);
static void mct_user_cleanup_handler(void *arg);
static int mct_start_threads(int id);
static void mct_stop_threads();
static void mct_fork_child_fork_handler();

static MctReturnValue mct_user_log_write_string_utils_attr(MctContextData *log, const char *text, const enum StringType type, const char *name, bool with_var_info);
static MctReturnValue mct_user_log_write_sized_string_utils_attr(MctContextData *log, const char *text, uint16_t length, const enum StringType type, const char *name, bool with_var_info);


static MctReturnValue mct_unregister_app_util(bool force_sending_messages);

MctReturnValue mct_user_check_library_version(const char *user_major_version,
                                              const char *user_minor_version)
{
    char lib_major_version[MCT_USER_MAX_LIB_VERSION_LENGTH];
    char lib_minor_version[MCT_USER_MAX_LIB_VERSION_LENGTH];

    mct_get_major_version(lib_major_version, MCT_USER_MAX_LIB_VERSION_LENGTH);
    mct_get_minor_version(lib_minor_version, MCT_USER_MAX_LIB_VERSION_LENGTH);

    if ((strcmp(lib_major_version,
                user_major_version) != 0) ||
        (strcmp(lib_minor_version, user_minor_version) != 0)) {
        mct_vnlog(
            LOG_WARNING,
            MCT_USER_BUFFER_LENGTH,
            "MCT Library version check failed! Installed MCT library version is %s.%s - Application using MCT library version %s.%s\n",
            lib_major_version,
            lib_minor_version,
            user_major_version,
            user_minor_version);

        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

#if defined MCT_LIB_USE_UNIX_SOCKET_IPC
static MctReturnValue mct_socket_set_nonblock_and_linger(int sockfd)
{
    int status;
    struct linger l_opt;

    status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    if (status == -1) {
        mct_log(LOG_INFO, "Socket cannot be changed to NON BLOCK\n");
        return MCT_RETURN_ERROR;
    }

    l_opt.l_onoff = 1;
    l_opt.l_linger = 10;

    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &l_opt, sizeof l_opt) < 0) {
        mct_log(LOG_WARNING, "Failed to set socket linger option\n");
    }

    return MCT_RETURN_OK;
}
#endif

#ifdef MCT_LIB_USE_UNIX_SOCKET_IPC
static MctReturnValue mct_initialize_socket_connection(void)
{
    struct sockaddr_un remote;
    char mctSockBaseDir[MCT_IPC_PATH_MAX];

    MCT_SEM_LOCK();
    int sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (sockfd == MCT_FD_INIT) {
        mct_log(LOG_CRIT, "Failed to create socket\n");
        MCT_SEM_FREE();
        return MCT_RETURN_ERROR;
    }

    mct_user.mct_log_handle = sockfd;

    /* Change socket mode */
    if (mct_set_socket_mode(mct_user.socket_mode) == MCT_RETURN_ERROR) {
        MCT_SEM_FREE();
        return MCT_RETURN_ERROR;
    }

    if (mct_socket_set_nonblock_and_linger(sockfd) != MCT_RETURN_OK) {
        close(sockfd);
        MCT_SEM_FREE();
        return MCT_RETURN_ERROR;
    }

    remote.sun_family = AF_UNIX;
    snprintf(mctSockBaseDir, MCT_IPC_PATH_MAX, "%s/mct", MCT_USER_IPC_PATH);
    strncpy(remote.sun_path, mctSockBaseDir, sizeof(remote.sun_path));

    if (strlen(MCT_USER_IPC_PATH) > MCT_IPC_PATH_MAX) {
        mct_vlog(LOG_INFO,
                 "Provided path too long...trimming it to path[%s]\n",
                 mctSockBaseDir);
    }

    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
        if (mct_user.connection_state != MCT_USER_RETRY_CONNECT) {
            mct_vlog(LOG_INFO,
                     "Socket %s cannot be opened (errno=%d). Retrying later...\n",
                     mctSockBaseDir, errno);
            mct_user.connection_state = MCT_USER_RETRY_CONNECT;
        }

        close(sockfd);
        mct_user.mct_log_handle = -1;
    } else {
        mct_user.mct_log_handle = sockfd;
        mct_user.connection_state = MCT_USER_CONNECTED;

        if (mct_receiver_init(&(mct_user.receiver),
                              sockfd,
                              MCT_RECEIVE_SOCKET,
                              MCT_USER_RCVBUF_MAX_SIZE) == MCT_RETURN_ERROR) {
            mct_user_initialised = false;
            close(sockfd);
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }
    }

    MCT_SEM_FREE();

    return MCT_RETURN_OK;
}

#elif defined MCT_LIB_USE_VSOCK_IPC
static MctReturnValue mct_initialize_vsock_connection()
{
    struct sockaddr_vm remote;

    MCT_SEM_LOCK();
    int sockfd = socket(AF_VSOCK, SOCK_STREAM, 0);

    if (sockfd == MCT_FD_INIT) {
        mct_log(LOG_CRIT, "Failed to create VSOCK socket\n");
        MCT_SEM_FREE();
        return MCT_RETURN_ERROR;
    }

    memset(&remote, 0, sizeof(remote));
    remote.svm_family = AF_VSOCK;
    remote.svm_port = MCT_VSOCK_PORT;
    remote.svm_cid = VMADDR_CID_HOST;

    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
        if (mct_user.connection_state != MCT_USER_RETRY_CONNECT) {
            mct_vlog(LOG_INFO, "VSOCK socket cannot be opened. Retrying later...\n");
            mct_user.connection_state = MCT_USER_RETRY_CONNECT;
        }

        close(sockfd);
        mct_user.mct_log_handle = -1;
    } else {
        /* Set to non-blocking after connect() to avoid EINPROGRESS. MctUserConntextionState
         * needs "connecting" state if connect() should be non-blocking. */
        if (mct_socket_set_nonblock_and_linger(sockfd) != MCT_RETURN_OK) {
            close(sockfd);
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }

        mct_user.mct_log_handle = sockfd;
        mct_user.connection_state = MCT_USER_CONNECTED;

        if (mct_receiver_init(&(mct_user.receiver),
                              sockfd,
                              MCT_RECEIVE_SOCKET,
                              MCT_USER_RCVBUF_MAX_SIZE) == MCT_RETURN_ERROR) {
            mct_user_initialised = false;
            close(sockfd);
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }
    }

    MCT_SEM_FREE();

    return MCT_RETURN_OK;
}
#else /* MCT_LIB_USE_FIFO_IPC */
static MctReturnValue mct_initialize_fifo_connection(void)
{
    char filename[MCT_PATH_MAX];
    int ret;

    snprintf(mct_user_dir, MCT_PATH_MAX, "%s/mctpipes", mctFifoBaseDir);
    snprintf(mct_daemon_fifo, MCT_PATH_MAX, "%s/mct", mctFifoBaseDir);
    ret = mkdir(mct_user_dir,
                S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_ISVTX);

    if ((ret == -1) && (errno != EEXIST)) {
        mct_vnlog(LOG_ERR,
                  MCT_USER_BUFFER_LENGTH,
                  "FIFO user dir %s cannot be created!\n",
                  mct_user_dir);
        return MCT_RETURN_ERROR;
    }

    /* if mct pipes directory is created by the application also chmod the directory */
    if (ret == 0) {
        /* S_ISGID cannot be set by mkdir, let's reassign right bits */
        ret = chmod(
                mct_user_dir,
                S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH |
                S_IXOTH | S_ISGID |
                S_ISVTX);

        if (ret == -1) {
            mct_vnlog(LOG_ERR,
                      MCT_USER_BUFFER_LENGTH,
                      "FIFO user dir %s cannot be chmoded!\n",
                      mct_user_dir);
            return MCT_RETURN_ERROR;
        }
    }

    /* create and open MCT user FIFO */
    snprintf(filename, MCT_PATH_MAX, "%s/mct%d", mct_user_dir, getpid());

    /* Try to delete existing pipe, ignore result of unlink */
    unlink(filename);

    ret = mkfifo(filename, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

    if (ret == -1) {
        mct_vnlog(LOG_WARNING,
                  MCT_USER_BUFFER_LENGTH,
                  "Loging disabled, FIFO user %s cannot be created!\n",
                  filename);
    }

    /* S_IWGRP cannot be set by mkfifo (???), let's reassign right bits */
    ret = chmod(filename, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

    if (ret == -1) {
        mct_vnlog(LOG_WARNING,
                  MCT_USER_BUFFER_LENGTH,
                  "FIFO user %s cannot be chmoded!\n",
                  mct_user_dir);
        return MCT_RETURN_ERROR;
    }

    mct_user.mct_user_handle = open(filename, O_RDWR | O_NONBLOCK | O_CLOEXEC);

    if (mct_user.mct_user_handle == MCT_FD_INIT) {
        mct_vnlog(LOG_WARNING,
                  MCT_USER_BUFFER_LENGTH,
                  "Logging disabled, FIFO user %s cannot be opened!\n",
                  filename);
        unlink(filename);
        return MCT_RETURN_OK;
    }

    /* open MCT output FIFO */
    mct_user.mct_log_handle = open(mct_daemon_fifo, O_WRONLY | O_NONBLOCK | O_CLOEXEC);

    if (mct_user.mct_log_handle == -1) {
        /* This is a normal usecase. It is OK that the daemon (and thus the FIFO /tmp/mct)
         * starts later and some MCT users have already been started before.
         * Thus it is OK if the FIFO can't be opened. */
        mct_vnlog(LOG_INFO, MCT_USER_BUFFER_LENGTH, "FIFO %s cannot be opened. Retrying later...\n",
                  mct_daemon_fifo);
    }

    return MCT_RETURN_OK;
}
#endif

MctReturnValue mct_set_socket_mode(MctUserSocketMode mode)
{
    int ret = MCT_RETURN_OK;

#ifdef MCT_USE_UNIX_SOCKET_IPC
    int flags = 0;
    int result = 0;
    char mctSockBaseDir[MCT_IPC_PATH_MAX];

    if (mct_user.mct_log_handle != -1) {
        flags = fcntl(mct_user.mct_log_handle, F_GETFL);

        if (mode == MCT_USER_SOCKET_BLOCKING) {
            /* Set socket to Blocking mode */
            mct_vlog(LOG_DEBUG, "%s: Set socket to Blocking mode\n", __func__);
            result = fcntl(mct_user.mct_log_handle, F_SETFL, flags & ~O_NONBLOCK);
            mct_user.socket_mode = MCT_USER_SOCKET_BLOCKING;
        } else {
            /* Set socket to Non-blocking mode */
            mct_vlog(LOG_DEBUG, "%s: Set socket to Non-blocking mode\n", __func__);
            result = fcntl(mct_user.mct_log_handle, F_SETFL, flags | O_NONBLOCK);
            mct_user.socket_mode = MCT_USER_SOCKET_NON_BLOCKING;
        }

        snprintf(mctSockBaseDir, MCT_IPC_PATH_MAX, "%s/mct", MCT_USER_IPC_PATH);

        if (result < 0) {
            mct_vlog(LOG_WARNING,
                     "%s: Failed to change mode of socket %s: %s\n",
                     __func__, mctSockBaseDir, strerror(errno));
            ret = MCT_RETURN_ERROR;
        }
    }

#else
    /* Satisfy Compiler for warnings */
    (void)mode;
#endif

    return ret;
}

MctReturnValue mct_init(void)
{
    /* Compare 'mct_user_initialised' to false. If equal, 'mct_user_initialised' will be set to true.
    Calls retruns true, if 'mct_user_initialised' was false.
    That way it's no problem, if two threads enter this function, because only the very first one will
    pass fully. The other one will immediately return, because when it executes the atomic function
    'mct_user_initialised' will be for sure already set to true.
    */
    bool expected = false;
    if (!(atomic_compare_exchange_strong(&mct_user_initialised, &expected, true)))
        return MCT_RETURN_OK;

    /* check environment variables */
    mct_check_envvar();

    /* Check logging mode and internal log file is opened or not*/
    if(logging_mode == MCT_LOG_TO_FILE && logging_handle == NULL)
    {
        mct_log_init(logging_mode);
    }

    /* process is exiting. Do not allocate new resources. */
    if (mct_user_freeing != 0) {
        mct_vlog(LOG_INFO, "%s logging disabled, process is exiting", __func__);
        /* return negative value, to stop the current log */
        return MCT_RETURN_LOGGING_DISABLED;
    }

    mct_user_app_unreg = false;

    /* Initialize common part of mct_init()/mct_init_file() */
    if (mct_init_common() == MCT_RETURN_ERROR) {
        mct_user_initialised = false;
        return MCT_RETURN_ERROR;
    }

    mct_user.mct_is_file = 0;
    mct_user.filesize_max = UINT_MAX;
    mct_user_file_reach_max = false;

    mct_user.overflow = 0;
    mct_user.overflow_counter = 0;

#ifdef MCT_LIB_USE_UNIX_SOCKET_IPC

    if (mct_initialize_socket_connection() != MCT_RETURN_OK) {
        /* We could connect to the pipe, but not to the socket, which is normally */
        /* open before by the MCT daemon => bad failure => return error code */
        /* in case application is started before daemon, it is expected behaviour */
        return MCT_RETURN_ERROR;
    }

#elif defined MCT_LIB_USE_VSOCK_IPC

    if (mct_initialize_vsock_connection() != MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

#else /* MCT_LIB_USE_FIFO_IPC */

    if (mct_initialize_fifo_connection() != MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    if (mct_receiver_init(&(mct_user.receiver),
                          mct_user.mct_user_handle,
                          MCT_RECEIVE_FD,
                          MCT_USER_RCVBUF_MAX_SIZE) == MCT_RETURN_ERROR) {
        mct_user_initialised = false;
        return MCT_RETURN_ERROR;
    }

#endif

    if (mct_start_threads(MCT_USER_HOUSEKEEPER_THREAD) < 0) {
        mct_user_initialised = false;
        return MCT_RETURN_ERROR;
    }

    /* prepare for fork() call */
    pthread_atfork(NULL, NULL, &mct_fork_child_fork_handler);

    return MCT_RETURN_OK;
}

MctReturnValue mct_get_appid(char *appid)
{
    if (appid != NULL) {
        strncpy(appid, mct_user.appID, 4);
        return MCT_RETURN_OK;
    } else {
        mct_log(LOG_ERR, "Invalid parameter.\n");
        return MCT_RETURN_WRONG_PARAMETER;
    }
}

MctReturnValue mct_init_file(const char *name)
{
    /* check null pointer */
    if (!name) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* Compare 'mct_user_initialised' to false. If equal, 'mct_user_initialised' will be set to true.
    Calls retruns true, if 'mct_user_initialised' was false.
    That way it's no problem, if two threads enter this function, because only the very first one will
    pass fully. The other one will immediately return, because when it executes the atomic function
    'mct_user_initialised' will be for sure already set to true.
    */
    bool expected = false;
    if (!(atomic_compare_exchange_strong(&mct_user_initialised, &expected, true)))
        return MCT_RETURN_OK;

    /* Initialize common part of mct_init()/mct_init_file() */
    if (mct_init_common() == MCT_RETURN_ERROR) {
        mct_user_initialised = false;
        return MCT_RETURN_ERROR;
    }

    mct_user.mct_is_file = 1;

    /* open MCT output file */
    mct_user.mct_log_handle = open(name, O_WRONLY | O_CREAT,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); /* mode: wb */

    if (mct_user.mct_log_handle == -1) {
        mct_vnlog(LOG_ERR, MCT_USER_BUFFER_LENGTH, "Log file %s cannot be opened!\n", name);
        mct_user.mct_is_file = 0;
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_set_filesize_max(unsigned int filesize)
{
    if (mct_user.mct_is_file == 0)
    {
        mct_vlog(LOG_ERR, "%s: Library is not configured to log to file\n",
                 __func__);
        return MCT_RETURN_ERROR;
    }

    if (filesize == 0) {
        mct_user.filesize_max = UINT_MAX;
    }
    else {
        mct_user.filesize_max = filesize;
    }
    mct_vlog(LOG_DEBUG, "%s: Defined filesize_max is [%d]\n", __func__,
             mct_user.filesize_max);

    return MCT_RETURN_OK;
}

/* Return true if verbose mode is to be used for this MctContextData */
static inline bool is_verbose_mode(int8_t mctuser_verbose_mode, const MctContextData* log)
{
    return (mctuser_verbose_mode == 1) || (log != NULL && log->verbose_mode);
}

MctReturnValue mct_init_common(void)
{
    char *env_local_print;
    char *env_initial_log_level;
    char *env_buffer_min;
    uint32_t buffer_min = MCT_USER_RINGBUFFER_MIN_SIZE;
    char *env_buffer_max;
    uint32_t buffer_max = MCT_USER_RINGBUFFER_MAX_SIZE;
    char *env_buffer_step;
    uint32_t buffer_step = MCT_USER_RINGBUFFER_STEP_SIZE;
    char *env_force_block;
    char *env_disable_extended_header_for_nonverbose;
    char *env_log_buffer_len;
    uint32_t buffer_max_configured = 0;
    uint32_t header_size = 0;

    /* Binary semaphore for threads */
    if (sem_init(&mct_mutex, 0, 1) == -1) {
        mct_user_initialised = false;
        return MCT_RETURN_ERROR;
    }

    /* set to unknown state of connected client */
    mct_user.log_state = -1;

    mct_user.mct_log_handle = -1;
    mct_user.mct_user_handle = MCT_FD_INIT;

    mct_set_id(mct_user.ecuID, MCT_USER_DEFAULT_ECU_ID);
    mct_set_id(mct_user.appID, "");

    mct_user.application_description = NULL;

    /* Verbose mode is enabled by default */
    mct_user.verbose_mode = 1;

    /* header_size is used for resend buffer
     * so it won't include MctStorageHeader
     */
    header_size = sizeof(MctUserHeader) + sizeof(MctStandardHeader) +
        sizeof(MctStandardHeaderExtra);

    /* Use extended header for non verbose is enabled by default */
    mct_user.use_extended_header_for_non_verbose =
        MCT_USER_USE_EXTENDED_HEADER_FOR_NONVERBOSE;

    /* Use extended header for non verbose is modified as per environment variable */
    env_disable_extended_header_for_nonverbose =
        getenv(MCT_USER_ENV_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE);

    if (env_disable_extended_header_for_nonverbose) {
        if (strcmp(env_disable_extended_header_for_nonverbose, "1") == 0) {
            mct_user.use_extended_header_for_non_verbose =
                MCT_USER_NO_USE_EXTENDED_HEADER_FOR_NONVERBOSE;
        }
    }

    if (mct_user.use_extended_header_for_non_verbose ==
        MCT_USER_USE_EXTENDED_HEADER_FOR_NONVERBOSE) {
        header_size += sizeof(MctExtendedHeader);
    }

    /* With session id is enabled by default */
    mct_user.with_session_id = MCT_USER_WITH_SESSION_ID;

    /* With timestamp is enabled by default */
    mct_user.with_timestamp = MCT_USER_WITH_TIMESTAMP;

    /* With timestamp is enabled by default */
    mct_user.with_ecu_id = MCT_USER_WITH_ECU_ID;

    /* Local print is disabled by default */
    mct_user.enable_local_print = 0;

    mct_user.local_print_mode = MCT_PM_UNSET;

    mct_user.timeout_at_exit_handler = MCT_USER_ATEXIT_RESEND_BUFFER_EXIT_TIMEOUT;

    env_local_print = getenv(MCT_USER_ENV_LOCAL_PRINT_MODE);

    if (env_local_print) {
        if (strcmp(env_local_print, "AUTOMATIC") == 0) {
            mct_user.local_print_mode = MCT_PM_AUTOMATIC;
        } else if (strcmp(env_local_print, "FORCE_ON") == 0) {
            mct_user.local_print_mode = MCT_PM_FORCE_ON;
        } else if (strcmp(env_local_print, "FORCE_OFF") == 0) {
            mct_user.local_print_mode = MCT_PM_FORCE_OFF;
        }
    }

    env_initial_log_level = getenv("MCT_INITIAL_LOG_LEVEL");

    if (env_initial_log_level != NULL) {
        if (mct_env_extract_ll_set(&env_initial_log_level, &mct_user.initial_ll_set) != 0) {
            mct_vlog(LOG_WARNING,
                     "Unable to parse initial set of log-levels from environment! Env:\n%s\n",
                     getenv("MCT_INITIAL_LOG_LEVEL"));
        }
    }

    /* Check for force block mode environment variable */
    env_force_block = getenv(MCT_USER_ENV_FORCE_BLOCK_MODE);

    /* Initialize LogLevel/TraceStatus field */
    MCT_SEM_LOCK();
    mct_user.mct_ll_ts = NULL;
    mct_user.mct_ll_ts_max_num_entries = 0;
    mct_user.mct_ll_ts_num_entries = 0;

    /* set block mode */
    if (env_force_block != NULL) {
        mct_user.block_mode = MCT_MODE_BLOCKING;
        mct_user.force_blocking = MCT_MODE_BLOCKING;
    }

    env_buffer_min = getenv(MCT_USER_ENV_BUFFER_MIN_SIZE);
    env_buffer_max = getenv(MCT_USER_ENV_BUFFER_MAX_SIZE);
    env_buffer_step = getenv(MCT_USER_ENV_BUFFER_STEP_SIZE);

    if (env_buffer_min != NULL) {
        buffer_min = (uint32_t)strtol(env_buffer_min, NULL, 10);

        if ((errno == EINVAL) || (errno == ERANGE)) {
            mct_vlog(LOG_ERR,
                     "Wrong value specified for %s. Using default: %d\n",
                     MCT_USER_ENV_BUFFER_MIN_SIZE,
                     MCT_USER_RINGBUFFER_MIN_SIZE);
            buffer_min = MCT_USER_RINGBUFFER_MIN_SIZE;
        }
    }

    if (env_buffer_max != NULL) {
        buffer_max = (uint32_t)strtol(env_buffer_max, NULL, 10);

        if ((errno == EINVAL) || (errno == ERANGE)) {
            mct_vlog(LOG_ERR,
                     "Wrong value specified for %s. Using default: %d\n",
                     MCT_USER_ENV_BUFFER_MAX_SIZE,
                     MCT_USER_RINGBUFFER_MAX_SIZE);
            buffer_max = MCT_USER_RINGBUFFER_MAX_SIZE;
        }
    }

    if (env_buffer_step != NULL) {
        buffer_step = (uint32_t)strtol(env_buffer_step, NULL, 10);

        if ((errno == EINVAL) || (errno == ERANGE)) {
            mct_vlog(LOG_ERR,
                     "Wrong value specified for %s. Using default: %d\n",
                     MCT_USER_ENV_BUFFER_STEP_SIZE,
                     MCT_USER_RINGBUFFER_STEP_SIZE);
            buffer_step = MCT_USER_RINGBUFFER_STEP_SIZE;
        }
    }

    /* init log buffer size */
    mct_user.log_buf_len = MCT_USER_BUF_MAX_SIZE;
    env_log_buffer_len = getenv(MCT_USER_ENV_LOG_MSG_BUF_LEN);

    if (env_log_buffer_len != NULL) {
        buffer_max_configured = (uint32_t)strtol(env_log_buffer_len, NULL, 10);

        if (buffer_max_configured > MCT_LOG_MSG_BUF_MAX_SIZE) {
            mct_user.log_buf_len = MCT_LOG_MSG_BUF_MAX_SIZE;
            mct_vlog(
                LOG_WARNING,
                "Configured size exceeds maximum allowed size,restricting to max [65535 bytes]\n");
        } else {
            mct_user.log_buf_len = buffer_max_configured;
            mct_vlog(LOG_INFO,
                     "Configured buffer size to [%u bytes]\n",
                     buffer_max_configured);
        }
    }

    if (mct_user.resend_buffer == NULL) {
        mct_user.resend_buffer = calloc(sizeof(unsigned char),
                                        (mct_user.log_buf_len + header_size));

        if (mct_user.resend_buffer == NULL) {
            mct_user_initialised = false;
            MCT_SEM_FREE();
            mct_vlog(LOG_ERR, "cannot allocate memory for resend buffer\n");
            return MCT_RETURN_ERROR;
        }
    }

    mct_user.disable_injection_msg = 0;

    if (getenv(MCT_USER_ENV_DISABLE_INJECTION_MSG)) {
        mct_log(LOG_WARNING, "Injection message is disabled\n");
        mct_user.disable_injection_msg = 1;
    }

    if (mct_buffer_init_dynamic(&(mct_user.startup_buffer),
                                buffer_min,
                                buffer_max,
                                buffer_step) == MCT_RETURN_ERROR) {
        mct_user_initialised = false;
        MCT_SEM_FREE();
        return MCT_RETURN_ERROR;
    }

    MCT_SEM_FREE();

    signal(SIGPIPE, SIG_IGN);                  /* ignore pipe signals */

    if (atexit_registered == 0) {
        atexit_registered = 1;
        atexit(mct_user_atexit_handler);
    }

#ifdef MCT_TEST_ENABLE
    mct_user.corrupt_user_header = 0;
    mct_user.corrupt_message_size = 0;
    mct_user.corrupt_message_size_size = 0;
#endif

    return MCT_RETURN_OK;
}

void mct_user_atexit_handler(void)
{
    /* parent will do clean-up */
    if (g_mct_is_child) {
        return;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        /* close file */
        mct_log_free();
        return;
    }

    /* Try to resend potential log messages in the user buffer */
    int count = mct_user_atexit_blow_out_user_buffer();

    if (count != 0) {
        mct_vnlog(LOG_WARNING, 128, "Lost log messages in user buffer when exiting: %i\n", count);
    }

    /* Unregister app (this also unregisters all contexts in daemon) */
    /* Ignore return value */
    mct_unregister_app_util(false);

    /* Cleanup */
    /* Ignore return value */
    mct_free();
}

int mct_user_atexit_blow_out_user_buffer(void)
{

    int count, ret;
    struct timespec ts;

    uint32_t exitTime = mct_uptime() + mct_user.timeout_at_exit_handler;

    /* Send content of ringbuffer */
    MCT_SEM_LOCK();
    count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
    MCT_SEM_FREE();

    if ((count > 0) && (mct_user.timeout_at_exit_handler > 0)) {
        while (mct_uptime() < exitTime) {
            if (mct_user.mct_log_handle == -1) {
                /* Reattach to daemon if neccesary */
                mct_user_log_reattach_to_daemon();

                if ((mct_user.mct_log_handle != -1) && (mct_user.overflow_counter)) {
                    if (mct_user_log_send_overflow() == 0) {
                        mct_vnlog(LOG_WARNING,
                                  MCT_USER_BUFFER_LENGTH,
                                  "%u messages discarded!\n",
                                  mct_user.overflow_counter);
                        mct_user.overflow_counter = 0;
                    }
                }
            }

            if (mct_user.mct_log_handle != -1) {
                ret = mct_user_log_resend_buffer();

                if (ret == 0) {
                    MCT_SEM_LOCK();
                    count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
                    MCT_SEM_FREE();

                    return count;
                }
            }

            ts.tv_sec = 0;
            ts.tv_nsec = MCT_USER_ATEXIT_RESEND_BUFFER_SLEEP;
            nanosleep(&ts, NULL);
        }

        MCT_SEM_LOCK();
        count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
        MCT_SEM_FREE();
    }

    return count;
}

static void mct_user_free_buffer(unsigned char **buffer)
{
    if (*buffer) {
        free(*buffer);
        *buffer = NULL;
    }
}

MctReturnValue mct_free(void)
{
    uint32_t i;
    int ret = 0;
#ifdef MCT_LIB_USE_FIFO_IPC
    char filename[MCT_PATH_MAX];
#endif

    if (mct_user_freeing != 0) {
        /* resources are already being freed. Do nothing and return. */
        return MCT_RETURN_ERROR;
    }

    /* library is freeing its resources. Avoid to allocate it in mct_init() */
    mct_user_freeing = 1;

    if (!mct_user_initialised) {
        mct_user_freeing = 0;
        return MCT_RETURN_ERROR;
    }

    mct_user_initialised = false;

    mct_stop_threads();

#ifdef MCT_LIB_USE_FIFO_IPC

    if (mct_user.mct_user_handle != MCT_FD_INIT) {
        close(mct_user.mct_user_handle);
        mct_user.mct_user_handle = MCT_FD_INIT;
        snprintf(filename, MCT_PATH_MAX, "%s/mct%d", mct_user_dir, getpid());
        unlink(filename);
    }

#endif

    if (mct_user.mct_log_handle != -1) {
        /* close log file/output fifo to daemon */
#if defined MCT_LIB_USE_UNIX_SOCKET_IPC
        ret = shutdown(mct_user.mct_log_handle, SHUT_WR);

        if (ret < 0) {
            mct_vlog(LOG_WARNING, "%s: shutdown failed: %s\n", __func__, strerror(errno));
        } else {
            ssize_t bytes_read = 0;
            int prev_errno = 0;
            struct pollfd nfd[1];
            nfd[0].events = POLLIN;
            nfd[0].fd = mct_user.mct_log_handle;

            while (1) {
                ret = poll(nfd, 1, MCT_USER_RECEIVE_MDELAY);

                /* In case failure of polling or reaching timeout,
                 * continue to close socket anyway.
                 * */
                if (ret < 0) {
                    mct_vlog(LOG_WARNING, "[%s] Failed to poll with error [%s]\n",
                             __func__, strerror(errno));
                    break;
                } else if (ret == 0) {
                    mct_vlog(LOG_DEBUG, "[%s] Polling timeout\n", __func__);
                    break;
                } else {
                    /* It could take some time to get the socket is shutdown
                     * So it means there could be some data available to read.
                     * Try to consume the data and poll the socket again.
                     * If read fails, time to close the socket then.
                     */
                    mct_vlog(LOG_DEBUG, "[%s] polling returns [%d] with revent [0x%x]."
                             "There are something to read\n", __func__, ret, (unsigned int)nfd[0].revents);

                    bytes_read = read(mct_user.mct_log_handle,
                                      mct_user.resend_buffer,
                                      mct_user.log_buf_len);
                    prev_errno = errno;

                    if (bytes_read < 0) {
                        mct_vlog(LOG_WARNING, "[%s] Failed to read with error [%s]\n",
                                 __func__, strerror(prev_errno));

                        if ((prev_errno == EAGAIN) || (EWOULDBLOCK != EAGAIN && prev_errno == EWOULDBLOCK)) {
                            continue;
                        } else {
                            break;
                        }
                    }

                    if (bytes_read >= 0) {
                        if (!bytes_read) {
                            break;
                        }

                        mct_vlog(LOG_NOTICE, "[%s] data is still readable... [%ld] bytes read\n",
                                 __func__, bytes_read);
                    }
                }
            }
        }

#endif
        ret = close(mct_user.mct_log_handle);

        if (ret < 0) {
            mct_vlog(LOG_WARNING, "%s: close failed: %s\n", __func__, strerror(errno));
        }

        mct_user.mct_log_handle = -1;
    }

    /* Ignore return value */
    MCT_SEM_LOCK();
    mct_receiver_free(&(mct_user.receiver));
    MCT_SEM_FREE();

    /* Ignore return value */
    MCT_SEM_LOCK();

    mct_user_free_buffer(&(mct_user.resend_buffer));
    mct_buffer_free_dynamic(&(mct_user.startup_buffer));

    /* Clear and free local stored application information */
    if (mct_user.application_description != NULL) {
        free(mct_user.application_description);
    }

    mct_user.application_description = NULL;

    if (mct_user.mct_ll_ts) {
        for (i = 0; i < mct_user.mct_ll_ts_max_num_entries; i++) {
            if (mct_user.mct_ll_ts[i].context_description != NULL) {
                free (mct_user.mct_ll_ts[i].context_description);
                mct_user.mct_ll_ts[i].context_description = NULL;
            }

            if (mct_user.mct_ll_ts[i].log_level_ptr != NULL) {
                free(mct_user.mct_ll_ts[i].log_level_ptr);
                mct_user.mct_ll_ts[i].log_level_ptr = NULL;
            }

            if (mct_user.mct_ll_ts[i].trace_status_ptr != NULL) {
                free(mct_user.mct_ll_ts[i].trace_status_ptr);
                mct_user.mct_ll_ts[i].trace_status_ptr = NULL;
            }

            if (mct_user.mct_ll_ts[i].injection_table != NULL) {
                free(mct_user.mct_ll_ts[i].injection_table);
                mct_user.mct_ll_ts[i].injection_table = NULL;
            }

            mct_user.mct_ll_ts[i].nrcallbacks = 0;
            mct_user.mct_ll_ts[i].log_level_changed_callback = 0;
#ifdef MCT_HP_LOG_ENABLE

            if (mct_user.mct_ll_ts[i].MctExtBuff_ptr != 0) {
                free(mct_user.mct_ll_ts[i].MctExtBuff_ptr);
                mct_user.mct_ll_ts[i].MctExtBuff_ptr = 0;
            }

#endif
        }

        free(mct_user.mct_ll_ts);
        mct_user.mct_ll_ts = NULL;
        mct_user.mct_ll_ts_max_num_entries = 0;
        mct_user.mct_ll_ts_num_entries = 0;
    }

    mct_env_free_ll_set(&mct_user.initial_ll_set);
    MCT_SEM_FREE();

    sem_destroy(&mct_mutex);

    /* allow the user app to do mct_init() again. */
    /* The flag is unset only to keep almost the same behaviour as before, on EntryNav */
    /* This should be removed for other projects (see documentation of mct_free() */
    mct_user_freeing = 0;

    return MCT_RETURN_OK;
}

MctReturnValue mct_check_library_version(const char *user_major_version,
                                         const char *user_minor_version)
{
    return mct_user_check_library_version(user_major_version, user_minor_version);
}

MctReturnValue mct_register_app(const char *apid, const char *description)
{
    MctReturnValue ret = MCT_RETURN_OK;

    /* pointer points to AppID environment variable */
    const char *env_app_id = NULL;
    /* pointer points to the AppID */
    const char *p_app_id = NULL;

    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* the AppID  may be specified by an environment variable */
    env_app_id = getenv(MCT_USER_ENV_APP_ID);

    if (env_app_id != NULL) {
        /* always use AppID if it is specified by an environment variable */
        p_app_id = env_app_id;
    } else {
        /* Environment variable is not specified, use value which is passed by function parameter */
        p_app_id = apid;
    }

    if ((p_app_id == NULL) || (p_app_id[0] == '\0')) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* check if application already registered */
    /* if yes do not register again */
    if (p_app_id[1] == 0) {
        if (p_app_id[0] == mct_user.appID[0]) {
            return MCT_RETURN_OK;
        }
    } else if (p_app_id[2] == 0) {
        if ((p_app_id[0] == mct_user.appID[0]) &&
            (p_app_id[1] == mct_user.appID[1])) {
            return MCT_RETURN_OK;
        }
    } else if (p_app_id[3] == 0) {
        if ((p_app_id[0] == mct_user.appID[0]) &&
            (p_app_id[1] == mct_user.appID[1]) &&
            (p_app_id[2] == mct_user.appID[2])) {
            return MCT_RETURN_OK;
        }
    } else {
        if ((p_app_id[0] == mct_user.appID[0]) &&
            (p_app_id[1] == mct_user.appID[1]) &&
            (p_app_id[2] == mct_user.appID[2]) &&
            (p_app_id[3] == mct_user.appID[3])) {
            return MCT_RETURN_OK;
        }
    }

    MCT_SEM_LOCK();

    /* Store locally application id and application description */
    mct_set_id(mct_user.appID, p_app_id);

    if (mct_user.application_description != NULL) {
        free(mct_user.application_description);
    }

    mct_user.application_description = NULL;

    if (description != NULL) {
        size_t desc_len = strlen(description);
        mct_user.application_description = malloc(desc_len + 1);

        if (mct_user.application_description) {
            strncpy(mct_user.application_description, description, desc_len + 1);
        } else {
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }
    }

    MCT_SEM_FREE();

    ret = mct_user_log_send_register_application();

    if ((ret == MCT_RETURN_OK) && (mct_user.mct_log_handle != -1)) {
        ret = mct_user_log_resend_buffer();
    }

    return ret;
}

MctReturnValue mct_register_context(MctContext *handle,
                                    const char *contextid,
                                    const char *description)
{
    /* check nullpointer */
    if (handle == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    if ((contextid == NULL) || (contextid[0] == '\0')) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    return mct_register_context_ll_ts(handle,
                                      contextid,
                                      description,
                                      MCT_USER_LOG_LEVEL_NOT_SET,
                                      MCT_USER_TRACE_STATUS_NOT_SET);
}

MctReturnValue mct_register_context_ll_ts_llccb(MctContext *handle,
                                                const char *contextid,
                                                const char *description,
                                                int loglevel,
                                                int tracestatus,
                                                void (*mct_log_level_changed_callback)(
                                                    char context_id[MCT_ID_SIZE],
                                                    uint8_t
                                                    log_level,
                                                    uint8_t
                                                    trace_status))
{
    MctContextData log;
    uint32_t i;
    int envLogLevel = MCT_USER_LOG_LEVEL_NOT_SET;

    /*check nullpointer */
    if ((handle == NULL) || (contextid == NULL) || (contextid[0] == '\0')) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if ((loglevel < MCT_USER_LOG_LEVEL_NOT_SET) || (loglevel >= MCT_LOG_MAX)) {
        mct_vlog(LOG_ERR, "Loglevel %d is outside valid range", loglevel);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if ((tracestatus < MCT_USER_TRACE_STATUS_NOT_SET) || (tracestatus >= MCT_TRACE_STATUS_MAX)) {
        mct_vlog(LOG_ERR, "Tracestatus %d is outside valid range", tracestatus);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (mct_user_log_init(handle, &log) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* Reset message counter */
    handle->mcnt = 0;

    /* Store context id in log level/trace status field */

    /* Check if already registered, else register context */
    MCT_SEM_LOCK();

    /* Check of double context registration removed */
    /* Double registration is already checked by daemon */

    /* Allocate or expand context array */
    if (mct_user.mct_ll_ts == NULL) {
        mct_user.mct_ll_ts = (mct_ll_ts_type *)malloc(
                sizeof(mct_ll_ts_type) * MCT_USER_CONTEXT_ALLOC_SIZE);

        if (mct_user.mct_ll_ts == NULL) {
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }

        mct_user.mct_ll_ts_max_num_entries = MCT_USER_CONTEXT_ALLOC_SIZE;

        /* Initialize new entries */
        for (i = 0; i < mct_user.mct_ll_ts_max_num_entries; i++) {
            mct_set_id(mct_user.mct_ll_ts[i].contextID, "");

            /* At startup, logging and tracing is locally enabled */
            /* the correct log level/status is set after received from daemon */
            mct_user.mct_ll_ts[i].log_level = MCT_USER_INITIAL_LOG_LEVEL;
            mct_user.mct_ll_ts[i].trace_status = MCT_USER_INITIAL_TRACE_STATUS;

            mct_user.mct_ll_ts[i].log_level_ptr = 0;
            mct_user.mct_ll_ts[i].trace_status_ptr = 0;

            mct_user.mct_ll_ts[i].context_description = 0;

            mct_user.mct_ll_ts[i].injection_table = 0;
            mct_user.mct_ll_ts[i].nrcallbacks = 0;
            mct_user.mct_ll_ts[i].log_level_changed_callback = 0;
#ifdef MCT_HP_LOG_ENABLE
            mct_user.mct_ll_ts[i].MctExtBuff_ptr = 0;
#endif
        }
    } else if ((mct_user.mct_ll_ts_num_entries % MCT_USER_CONTEXT_ALLOC_SIZE) == 0) {
        /* allocate memory in steps of MCT_USER_CONTEXT_ALLOC_SIZE, e.g. 500 */
        mct_ll_ts_type *old_ll_ts;
        uint32_t old_max_entries;

        old_ll_ts = mct_user.mct_ll_ts;
        old_max_entries = mct_user.mct_ll_ts_max_num_entries;

        mct_user.mct_ll_ts_max_num_entries = ((mct_user.mct_ll_ts_num_entries
                                               / MCT_USER_CONTEXT_ALLOC_SIZE) + 1)
            * MCT_USER_CONTEXT_ALLOC_SIZE;
        mct_user.mct_ll_ts = (mct_ll_ts_type *)malloc(sizeof(mct_ll_ts_type) *
                                                      mct_user.mct_ll_ts_max_num_entries);

        if (mct_user.mct_ll_ts == NULL) {
            mct_user.mct_ll_ts = old_ll_ts;
            mct_user.mct_ll_ts_max_num_entries = old_max_entries;
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }

        memcpy(mct_user.mct_ll_ts,
               old_ll_ts,
               sizeof(mct_ll_ts_type) * mct_user.mct_ll_ts_num_entries);
        free(old_ll_ts);

        /* Initialize new entries */
        for (i = mct_user.mct_ll_ts_num_entries; i < mct_user.mct_ll_ts_max_num_entries; i++) {
            mct_set_id(mct_user.mct_ll_ts[i].contextID, "");

            /* At startup, logging and tracing is locally enabled */
            /* the correct log level/status is set after received from daemon */
            mct_user.mct_ll_ts[i].log_level = MCT_USER_INITIAL_LOG_LEVEL;
            mct_user.mct_ll_ts[i].trace_status = MCT_USER_INITIAL_TRACE_STATUS;

            mct_user.mct_ll_ts[i].log_level_ptr = 0;
            mct_user.mct_ll_ts[i].trace_status_ptr = 0;

            mct_user.mct_ll_ts[i].context_description = 0;

            mct_user.mct_ll_ts[i].injection_table = 0;
            mct_user.mct_ll_ts[i].nrcallbacks = 0;
            mct_user.mct_ll_ts[i].log_level_changed_callback = 0;
#ifdef MCT_HP_LOG_ENABLE
            mct_user.mct_ll_ts[i].MctExtBuff_ptr = 0;
#endif
        }
    }

    /* New context entry to be initialized */
    mct_ll_ts_type *ctx_entry;
    ctx_entry = &mct_user.mct_ll_ts[mct_user.mct_ll_ts_num_entries];

    /* Store locally context id and context description */
    mct_set_id(ctx_entry->contextID, contextid);

    if (ctx_entry->context_description != 0) {
        free(ctx_entry->context_description);
    }

    ctx_entry->context_description = 0;

    if (description != 0) {
        size_t desc_len = strlen(description);
        ctx_entry->context_description = malloc(desc_len + 1);

        if (ctx_entry->context_description == 0) {
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }

        strncpy(ctx_entry->context_description, description, desc_len + 1);
    }

    if (ctx_entry->log_level_ptr == 0) {
        ctx_entry->log_level_ptr = malloc(sizeof(int8_t));

        if (ctx_entry->log_level_ptr == 0) {
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }
    }

    if (ctx_entry->trace_status_ptr == 0) {
        ctx_entry->trace_status_ptr = malloc(sizeof(int8_t));

        if (ctx_entry->trace_status_ptr == 0) {
            MCT_SEM_FREE();
            return MCT_RETURN_ERROR;
        }
    }

    /* check if the log level is set in the environement */
    envLogLevel = mct_env_adjust_ll_from_env(&mct_user.initial_ll_set,
                                             mct_user.appID,
                                             contextid,
                                             MCT_USER_LOG_LEVEL_NOT_SET);

    if (envLogLevel != MCT_USER_LOG_LEVEL_NOT_SET) {
        ctx_entry->log_level = envLogLevel;
        loglevel = envLogLevel;
    } else if (loglevel != MCT_USER_LOG_LEVEL_NOT_SET) {
        ctx_entry->log_level = loglevel;
    }

    if (tracestatus != MCT_USER_TRACE_STATUS_NOT_SET) {
        ctx_entry->trace_status = tracestatus;
    }

    /* Prepare transfer struct */
    mct_set_id(handle->contextID, contextid);
    handle->log_level_pos = mct_user.mct_ll_ts_num_entries;

    handle->log_level_ptr = ctx_entry->log_level_ptr;
    handle->trace_status_ptr = ctx_entry->trace_status_ptr;

    log.context_description = ctx_entry->context_description;

    *(ctx_entry->log_level_ptr) = ctx_entry->log_level;
    *(ctx_entry->trace_status_ptr) = ctx_entry->trace_status = tracestatus;
    ctx_entry->log_level_changed_callback = mct_log_level_changed_callback;

    log.log_level = loglevel;
    log.trace_status = tracestatus;

    mct_user.mct_ll_ts_num_entries++;

    MCT_SEM_FREE();

    return mct_user_log_send_register_context(&log);
}

MctReturnValue mct_register_context_ll_ts(MctContext *handle,
                                          const char *contextid,
                                          const char *description,
                                          int loglevel,
                                          int tracestatus)
{
    return mct_register_context_ll_ts_llccb(handle,
                                            contextid,
                                            description,
                                            loglevel,
                                            tracestatus,
                                            NULL);
}

MctReturnValue mct_register_context_llccb(MctContext *handle,
                                          const char *contextid,
                                          const char *description,
                                          void (*mct_log_level_changed_callback)(
                                              char context_id[MCT_ID_SIZE],
                                              uint8_t log_level,
                                              uint8_t
                                              trace_status))
{
    if ((handle == NULL) || (contextid == NULL) || (contextid[0] == '\0')) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    return mct_register_context_ll_ts_llccb(handle,
                                            contextid,
                                            description,
                                            MCT_USER_LOG_LEVEL_NOT_SET,
                                            MCT_USER_TRACE_STATUS_NOT_SET,
                                            mct_log_level_changed_callback);
}

/* If force_sending_messages is set to true, do not clean appIDs when there are
 * still data in startup_buffer. atexit_handler will free the appIDs */
MctReturnValue mct_unregister_app_util(bool force_sending_messages)
{
    MctReturnValue ret = MCT_RETURN_OK;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    /* Do not allow unregister_context() to be called after this */
    mct_user_app_unreg = true;

    /* Inform daemon to unregister application and all of its contexts */
    ret = mct_user_log_send_unregister_application();

    MCT_SEM_LOCK();

    int count = mct_buffer_get_message_count(&(mct_user.startup_buffer));

    if (!force_sending_messages ||
        (force_sending_messages && (count == 0))) {
        /* Clear and free local stored application information */
        mct_set_id(mct_user.appID, "");

        if (mct_user.application_description != NULL) {
            free(mct_user.application_description);
        }

        mct_user.application_description = NULL;
    }

    MCT_SEM_FREE();

    return ret;
}

MctReturnValue mct_unregister_app(void)
{
    return mct_unregister_app_util(false);
}

MctReturnValue mct_unregister_app_flush_buffered_logs(void)
{
    MctReturnValue ret = MCT_RETURN_OK;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_ERR, "%s mct_user_initialised false\n", __func__);
        return MCT_RETURN_ERROR;
    }

    if (mct_user.mct_log_handle != -1) {
        do {
            ret = mct_user_log_resend_buffer();
        } while ((ret != MCT_RETURN_OK) && (mct_user.mct_log_handle != -1));
    }

    return mct_unregister_app_util(true);
}

MctReturnValue mct_unregister_context(MctContext *handle)
{
    MctContextData log;
    MctReturnValue ret = MCT_RETURN_OK;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (mct_user_app_unreg) {
        mct_vlog(LOG_WARNING,
                 "%s: Contexts and application are already unregistered\n",
                 __func__);
        return MCT_RETURN_ERROR;
    }

    log.handle = NULL;
    log.context_description = NULL;

    if (mct_user_log_init(handle, &log) <= MCT_RETURN_ERROR) {
        return MCT_RETURN_ERROR;
    }

    MCT_SEM_LOCK();

    handle->log_level_ptr = NULL;
    handle->trace_status_ptr = NULL;

    if (mct_user.mct_ll_ts != NULL) {
        /* Clear and free local stored context information */
        mct_set_id(mct_user.mct_ll_ts[handle->log_level_pos].contextID, "");

        mct_user.mct_ll_ts[handle->log_level_pos].log_level = MCT_USER_INITIAL_LOG_LEVEL;
        mct_user.mct_ll_ts[handle->log_level_pos].trace_status = MCT_USER_INITIAL_TRACE_STATUS;

        if (mct_user.mct_ll_ts[handle->log_level_pos].context_description != NULL) {
            free(mct_user.mct_ll_ts[handle->log_level_pos].context_description);
        }

        if (mct_user.mct_ll_ts[handle->log_level_pos].log_level_ptr != NULL) {
            free(mct_user.mct_ll_ts[handle->log_level_pos].log_level_ptr);
            mct_user.mct_ll_ts[handle->log_level_pos].log_level_ptr = NULL;
        }

        if (mct_user.mct_ll_ts[handle->log_level_pos].trace_status_ptr != NULL) {
            free(mct_user.mct_ll_ts[handle->log_level_pos].trace_status_ptr);
            mct_user.mct_ll_ts[handle->log_level_pos].trace_status_ptr = NULL;
        }

        mct_user.mct_ll_ts[handle->log_level_pos].context_description = NULL;

        if (mct_user.mct_ll_ts[handle->log_level_pos].injection_table != NULL) {
            free(mct_user.mct_ll_ts[handle->log_level_pos].injection_table);
            mct_user.mct_ll_ts[handle->log_level_pos].injection_table = NULL;
        }

        mct_user.mct_ll_ts[handle->log_level_pos].nrcallbacks = 0;
        mct_user.mct_ll_ts[handle->log_level_pos].log_level_changed_callback = 0;
#ifdef MCT_HP_LOG_ENABLE

        if (mct_user.mct_ll_ts[handle->log_level_pos].MctExtBuff_ptr) {
            free(mct_user.mct_ll_ts[handle->log_level_pos].MctExtBuff_ptr);
            mct_user.mct_ll_ts[handle->log_level_pos].MctExtBuff_ptr = 0;
        }

#endif
    }

    MCT_SEM_FREE();

    /* Inform daemon to unregister context */
    ret = mct_user_log_send_unregister_context(&log);

    return ret;
}

MctReturnValue mct_set_application_ll_ts_limit(MctLogLevelType loglevel,
                                               MctTraceStatusType tracestatus)
{
    uint32_t i;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if ((loglevel < MCT_USER_LOG_LEVEL_NOT_SET) || (loglevel >= MCT_LOG_MAX)) {
        mct_vlog(LOG_ERR, "Loglevel %d is outside valid range", loglevel);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if ((tracestatus < MCT_USER_TRACE_STATUS_NOT_SET) || (tracestatus >= MCT_TRACE_STATUS_MAX)) {
        mct_vlog(LOG_ERR, "Tracestatus %d is outside valid range", tracestatus);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    MCT_SEM_LOCK();

    if (mct_user.mct_ll_ts == NULL) {
        MCT_SEM_FREE();
        return MCT_RETURN_ERROR;
    }

    /* Update local structures */
    for (i = 0; i < mct_user.mct_ll_ts_num_entries; i++) {
        mct_user.mct_ll_ts[i].log_level = loglevel;
        mct_user.mct_ll_ts[i].trace_status = tracestatus;

        if (mct_user.mct_ll_ts[i].log_level_ptr) {
            *(mct_user.mct_ll_ts[i].log_level_ptr) = loglevel;
        }

        if (mct_user.mct_ll_ts[i].trace_status_ptr) {
            *(mct_user.mct_ll_ts[i].trace_status_ptr) = tracestatus;
        }
    }

    MCT_SEM_FREE();

    /* Inform MCT server about update */
    return mct_send_app_ll_ts_limit(mct_user.appID, loglevel, tracestatus);
}

int mct_get_log_state()
{
    return mct_user.log_state;
}

/* @deprecated */
MctReturnValue mct_set_log_mode(MctUserLogMode mode)
{
    MCT_UNUSED(mode);

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    return 0;
}

int mct_set_resend_timeout_atexit(uint32_t timeout_in_milliseconds)
{
    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    if (mct_user_initialised == 0) {
        if (mct_init() < 0) {
            return -1;
        }
    }

    mct_user.timeout_at_exit_handler = timeout_in_milliseconds * 10;
    return 0;
}

/* ********************************************************************************************* */

MctReturnValue mct_user_log_write_start_init(MctContext *handle,
                                                    MctContextData *log,
                                                    MctLogLevelType loglevel,
                                                    bool is_verbose)
{
    MCT_LOG_FATAL_RESET_TRAP(loglevel);

    /* initialize values */
    if ((mct_user_log_init(handle, log) < MCT_RETURN_OK) || (mct_user.mct_ll_ts == NULL))
        return MCT_RETURN_ERROR;

    log->args_num = 0;
    log->log_level = loglevel;
    log->size = 0;
    log->use_timestamp = MCT_AUTO_TIMESTAMP;
    log->verbose_mode = is_verbose;

    return MCT_RETURN_TRUE;
}

static MctReturnValue mct_user_log_write_start_internal(MctContext *handle,
                                                        MctContextData *log,
                                                        MctLogLevelType loglevel,
                                                        uint32_t messageid,
                                                        bool is_verbose);

inline MctReturnValue mct_user_log_write_start(MctContext *handle,
                                               MctContextData *log,
                                               MctLogLevelType loglevel)
{
    return mct_user_log_write_start_internal(handle, log, loglevel, MCT_USER_DEFAULT_MSGID, true);
}

MctReturnValue mct_user_log_write_start_id(MctContext *handle,
                                           MctContextData *log,
                                           MctLogLevelType loglevel,
                                           uint32_t messageid)
{
    return mct_user_log_write_start_internal(handle, log, loglevel, messageid, false);
}

MctReturnValue mct_user_log_write_start_internal(MctContext *handle,
                                           MctContextData *log,
                                           MctLogLevelType loglevel,
                                           uint32_t messageid,
                                           bool is_verbose)
{
    int ret = MCT_RETURN_TRUE;

    /* check nullpointer */
    if ((handle == NULL) || (log == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return MCT_RETURN_ERROR;
    }

    /* check log levels */
    ret = mct_user_is_logLevel_enabled(handle, loglevel);

    if (ret == MCT_RETURN_WRONG_PARAMETER) {
        return MCT_RETURN_WRONG_PARAMETER;
    } else if (ret == MCT_RETURN_LOGGING_DISABLED) {
        log->handle = NULL;
        return MCT_RETURN_OK;
    }

    ret = mct_user_log_write_start_init(handle, log, loglevel, is_verbose);
    if (ret == MCT_RETURN_TRUE) {
        /* initialize values */
        if (log->buffer == NULL) {
            log->buffer = calloc(sizeof(unsigned char), mct_user.log_buf_len);

            if (log->buffer == NULL) {
                mct_vlog(LOG_ERR, "Cannot allocate buffer for MCT Log message\n");
                return MCT_RETURN_ERROR;
            }
        }

        /* In non-verbose mode, insert message id */
        if (!is_verbose_mode(mct_user.verbose_mode, log)) {
            if ((sizeof(uint32_t)) > mct_user.log_buf_len) {
                return MCT_RETURN_USER_BUFFER_FULL;
            }

            /* Write message id */
            memcpy(log->buffer, &(messageid), sizeof(uint32_t));
            log->size = sizeof(uint32_t);

            /* as the message id is part of each message in non-verbose mode,
             * it doesn't increment the argument counter in extended header (if used) */
        }
    }
    return ret;
}

MctReturnValue mct_user_log_write_start_w_given_buffer(MctContext *handle,
                                                       MctContextData *log,
                                                       MctLogLevelType loglevel,
                                                       char *buffer,
                                                       size_t size,
                                                       int32_t args_num)
{
    int ret = MCT_RETURN_TRUE;

    /* check nullpointer */
    if ((handle == NULL) || (log == NULL) || (buffer == NULL))
        return MCT_RETURN_WRONG_PARAMETER;

    /* discard unexpected parameters */
    if ((size <= 0) || (size > mct_user.log_buf_len) || (args_num <= 0))
        return MCT_RETURN_WRONG_PARAMETER;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child)
        return MCT_RETURN_ERROR;

    /* discard non-verbose mode */
    if (mct_user.verbose_mode == 0)
        return MCT_RETURN_ERROR;

    ret = mct_user_log_write_start_init(handle, log, loglevel, true);
    if (ret == MCT_RETURN_TRUE) {
        log->buffer = (unsigned char *)buffer;
        log->size = size;
        log->args_num = args_num;
    }

    return ret;
}

MctReturnValue mct_user_log_write_finish(MctContextData *log)
{
    int ret = MCT_RETURN_ERROR;

    if (log == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    ret = mct_user_log_send_log(log, MCT_TYPE_LOG);

    mct_user_free_buffer(&(log->buffer));

    return ret;
}

MctReturnValue mct_user_log_write_finish_w_given_buffer(MctContextData *log)
{
    int ret = MCT_RETURN_ERROR;

    if (log == NULL)
        return MCT_RETURN_WRONG_PARAMETER;

    ret = mct_user_log_send_log(log, MCT_TYPE_LOG);

    return ret;
}

static MctReturnValue mct_user_log_write_raw_internal(MctContextData *log, const void *data, uint16_t length, MctFormatType type, const char *name, bool with_var_info)
{
    /* check nullpointer */
    if ((log == NULL) || ((data == NULL) && (length != 0))) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* Have to cast type to signed type because some compilers assume that MctFormatType is unsigned and issue a warning */
    if (((int16_t)type < MCT_FORMAT_DEFAULT) || (type >= MCT_FORMAT_MAX)) {
        mct_vlog(LOG_ERR, "Format type %u is outside valid range", type);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    const uint16_t name_size = (name != NULL) ? strlen(name)+1 : 0;

    size_t needed_size = length + sizeof(uint16_t);
    if ((log->size + needed_size) > mct_user.log_buf_len)
        return MCT_RETURN_USER_BUFFER_FULL;

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        uint32_t type_info = MCT_TYPE_INFO_RAWD;

        needed_size += sizeof(uint32_t);  // Type Info field
        if (with_var_info) {
            needed_size += sizeof(uint16_t);  // length of name
            needed_size += name_size;  // the name itself

            type_info |= MCT_TYPE_INFO_VARI;
        }
        if ((log->size + needed_size) > mct_user.log_buf_len)
            return MCT_RETURN_USER_BUFFER_FULL;


        if ((type >= MCT_FORMAT_HEX8) && (type <= MCT_FORMAT_HEX64)) {
            type_info |= MCT_SCOD_HEX;
            type_info += type;
        } else if ((type >= MCT_FORMAT_BIN8) && (type <= MCT_FORMAT_BIN16)) {
            type_info |= MCT_SCOD_BIN;
            type_info += type - MCT_FORMAT_BIN8 + 1;
        }

        memcpy(log->buffer + log->size, &type_info, sizeof(uint32_t));
        log->size += sizeof(uint32_t);
    }

    memcpy(log->buffer + log->size, &length, sizeof(uint16_t));
    log->size += sizeof(uint16_t);

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        if (with_var_info) {
            // Write length of "name" attribute.
            // We assume that the protocol allows zero-sized strings here (which this code will create
            // when the input pointer is NULL).
            memcpy(log->buffer + log->size, &name_size, sizeof(uint16_t));
            log->size += sizeof(uint16_t);

            // Write name string itself.
            // Must not use NULL as source pointer for memcpy. This check assures that.
            if (name_size != 0) {
                memcpy(log->buffer + log->size, name, name_size);
                log->size += name_size;
            }
        }
    }

    memcpy(log->buffer + log->size, data, length);
    log->size += length;

    log->args_num++;

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_raw(MctContextData *log, void *data, uint16_t length)
{
    return mct_user_log_write_raw_internal(log, data, length, MCT_FORMAT_DEFAULT, NULL, false);
}

MctReturnValue mct_user_log_write_raw_formatted(MctContextData *log, void *data, uint16_t length, MctFormatType type)
{
    return mct_user_log_write_raw_internal(log, data, length, type, NULL, false);
}

MctReturnValue mct_user_log_write_raw_attr(MctContextData *log, const void *data, uint16_t length, const char *name)
{
    return mct_user_log_write_raw_internal(log, data, length, MCT_FORMAT_DEFAULT, name, true);
}

MctReturnValue mct_user_log_write_raw_formatted_attr(MctContextData *log, const void *data, uint16_t length, MctFormatType type, const char *name)
{
    return mct_user_log_write_raw_internal(log, data, length, type, name, true);
}

// Generic implementation for all "simple" types, possibly with attributes
static MctReturnValue mct_user_log_write_generic_attr(MctContextData *log, const void *datap, size_t datalen, uint32_t type_info, const VarInfo *varinfo)
{
    if (log == NULL)
        return MCT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    size_t needed_size = datalen;
    if ((log->size + needed_size) > mct_user.log_buf_len)
        return MCT_RETURN_USER_BUFFER_FULL;

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        bool with_var_info = (varinfo != NULL);

        uint16_t name_size;
        uint16_t unit_size;

        needed_size += sizeof(uint32_t);  // Type Info field
        if (with_var_info) {
            name_size = (varinfo->name != NULL) ? strlen(varinfo->name)+1 : 0;
            unit_size = (varinfo->unit != NULL) ? strlen(varinfo->unit)+1 : 0;

            needed_size += sizeof(uint16_t);      // length of name
            needed_size += name_size;             // the name itself
            if (varinfo->with_unit) {
                needed_size += sizeof(uint16_t);  // length of unit
                needed_size += unit_size;         // the unit itself
            }

            type_info |= MCT_TYPE_INFO_VARI;
        }
        if ((log->size + needed_size) > mct_user.log_buf_len)
            return MCT_RETURN_USER_BUFFER_FULL;

        memcpy(log->buffer + log->size, &type_info, sizeof(uint32_t));
        log->size += sizeof(uint32_t);

        if (with_var_info) {
            // Write lengths of name/unit strings
            // We assume here that the protocol allows zero-sized strings here (which occur
            // when the input pointers are NULL).
            memcpy(log->buffer + log->size, &name_size, sizeof(uint16_t));
            log->size += sizeof(uint16_t);
            if (varinfo->with_unit) {
                memcpy(log->buffer + log->size, &unit_size, sizeof(uint16_t));
                log->size += sizeof(uint16_t);
            }

            // Write name/unit strings themselves
            // Must not use NULL as source pointer for memcpy.
            if (name_size != 0) {
                memcpy(log->buffer + log->size, varinfo->name, name_size);
                log->size += name_size;
            }
            if (unit_size != 0) {
                memcpy(log->buffer + log->size, varinfo->unit, unit_size);
                log->size += unit_size;
            }
        }
    }

    memcpy(log->buffer + log->size, datap, datalen);
    log->size += datalen;

    log->args_num++;

    return MCT_RETURN_OK;
}

// Generic implementation for all "simple" types
static MctReturnValue mct_user_log_write_generic_formatted(MctContextData *log, const void *datap, size_t datalen, uint32_t type_info, MctFormatType type)
{
    if (log == NULL)
        return MCT_RETURN_WRONG_PARAMETER;

    /* Have to cast type to signed type because some compilers assume that MctFormatType is unsigned and issue a warning */
    if (((int16_t)type < MCT_FORMAT_DEFAULT) || (type >= MCT_FORMAT_MAX)) {
        mct_vlog(LOG_ERR, "Format type %d is outside valid range", type);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    size_t needed_size = datalen;
    if ((log->size + needed_size) > mct_user.log_buf_len)
        return MCT_RETURN_USER_BUFFER_FULL;

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        needed_size += sizeof(uint32_t);  // Type Info field
        if ((log->size + needed_size) > mct_user.log_buf_len)
            return MCT_RETURN_USER_BUFFER_FULL;

        if ((type >= MCT_FORMAT_HEX8) && (type <= MCT_FORMAT_HEX64))
            type_info |= MCT_SCOD_HEX;

        else if ((type >= MCT_FORMAT_BIN8) && (type <= MCT_FORMAT_BIN16))
            type_info |= MCT_SCOD_BIN;

        memcpy(log->buffer + log->size, &type_info, sizeof(uint32_t));
        log->size += sizeof(uint32_t);
    }

    memcpy(log->buffer + log->size, datap, datalen);
    log->size += datalen;

    log->args_num++;

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_float32(MctContextData *log, float32_t data)
{
    if (sizeof(float32_t) != 4)
        return MCT_RETURN_ERROR;

    uint32_t type_info = MCT_TYPE_INFO_FLOA | MCT_TYLE_32BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(float32_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_float64(MctContextData *log, float64_t data)
{
    if (sizeof(float64_t) != 8)
        return MCT_RETURN_ERROR;

    uint32_t type_info = MCT_TYPE_INFO_FLOA | MCT_TYLE_64BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(float64_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_float32_attr(MctContextData *log, float32_t data, const char *name, const char *unit)
{
    if (sizeof(float32_t) != 4)
        return MCT_RETURN_ERROR;

    uint32_t type_info = MCT_TYPE_INFO_FLOA | MCT_TYLE_32BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(float32_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_float64_attr(MctContextData *log, float64_t data, const char *name, const char *unit)
{
    if (sizeof(float64_t) != 8)
        return MCT_RETURN_ERROR;

    uint32_t type_info = MCT_TYPE_INFO_FLOA | MCT_TYLE_64BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(float64_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_uint(MctContextData *log, unsigned int data)
{
    if (log == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    switch (sizeof(unsigned int)) {
    case 1:
    {
        return mct_user_log_write_uint8(log, (uint8_t)data);
        break;
    }
    case 2:
    {
        return mct_user_log_write_uint16(log, (uint16_t)data);
        break;
    }
    case 4:
    {
        return mct_user_log_write_uint32(log, (uint32_t)data);
        break;
    }
    case 8:
    {
        return mct_user_log_write_uint64(log, (uint64_t)data);
        break;
    }
    default:
    {
        return MCT_RETURN_ERROR;
        break;
    }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_uint8(MctContextData *log, uint8_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_8BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_uint16(MctContextData *log, uint16_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_16BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint16_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_uint32(MctContextData *log, uint32_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_32BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint32_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_uint64(MctContextData *log, uint64_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_64BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint64_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_uint_attr(MctContextData *log, unsigned int data, const char *name, const char *unit)
{
    if (log == NULL)
        return MCT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    switch (sizeof(unsigned int)) {
    case 1:
    {
        return mct_user_log_write_uint8_attr(log, (uint8_t)data, name, unit);
        break;
    }
    case 2:
    {
        return mct_user_log_write_uint16_attr(log, (uint16_t)data, name, unit);
        break;
    }
    case 4:
    {
        return mct_user_log_write_uint32_attr(log, (uint32_t)data, name, unit);
        break;
    }
    case 8:
    {
        return mct_user_log_write_uint64_attr(log, (uint64_t)data, name, unit);
        break;
    }
    default:
    {
        return MCT_RETURN_ERROR;
        break;
    }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_uint8_attr(MctContextData *log, uint8_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_8BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_uint16_attr(MctContextData *log, uint16_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_16BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint16_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_uint32_attr(MctContextData *log, uint32_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_32BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint32_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_uint64_attr(MctContextData *log, uint64_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_64BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint64_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_uint8_formatted(MctContextData *log,
                                                  uint8_t data,
                                                  MctFormatType type)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_8BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint8_t), type_info, type);
}

MctReturnValue mct_user_log_write_uint16_formatted(MctContextData *log,
                                                   uint16_t data,
                                                   MctFormatType type)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_16BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint16_t), type_info, type);
}

MctReturnValue mct_user_log_write_uint32_formatted(MctContextData *log,
                                                   uint32_t data,
                                                   MctFormatType type)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_32BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint32_t), type_info, type);
}

MctReturnValue mct_user_log_write_uint64_formatted(MctContextData *log,
                                                   uint64_t data,
                                                   MctFormatType type)
{
    uint32_t type_info = MCT_TYPE_INFO_UINT | MCT_TYLE_64BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint64_t), type_info, type);
}

MctReturnValue mct_user_log_write_ptr(MctContextData *log, void *data)
{
    if (log == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    switch (sizeof(void *)) {
        case 4:
            return mct_user_log_write_uint32_formatted(log,
                                                       (uintptr_t)data,
                                                       MCT_FORMAT_HEX32);
            break;
        case 8:
            return mct_user_log_write_uint64_formatted(log,
                                                       (uintptr_t)data,
                                                       MCT_FORMAT_HEX64);
            break;
        default:
            ; /* skip */
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_int(MctContextData *log, int data)
{
    if (log == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    switch (sizeof(int)) {
    case 1:
    {
        return mct_user_log_write_int8(log, (int8_t)data);
        break;
    }
    case 2:
    {
        return mct_user_log_write_int16(log, (int16_t)data);
        break;
    }
    case 4:
    {
        return mct_user_log_write_int32(log, (int32_t)data);
        break;
    }
    case 8:
    {
        return mct_user_log_write_int64(log, (int64_t)data);
        break;
    }
    default:
    {
        return MCT_RETURN_ERROR;
        break;
    }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_int8(MctContextData *log, int8_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_8BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int8_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_int16(MctContextData *log, int16_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_16BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int16_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_int32(MctContextData *log, int32_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_32BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int32_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_int64(MctContextData *log, int64_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_64BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int64_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_int_attr(MctContextData *log, int data, const char *name, const char *unit)
{
    if (log == NULL)
        return MCT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    switch (sizeof(int)) {
    case 1:
    {
        return mct_user_log_write_int8_attr(log, (int8_t)data, name, unit);
        break;
    }
    case 2:
    {
        return mct_user_log_write_int16_attr(log, (int16_t)data, name, unit);
        break;
    }
    case 4:
    {
        return mct_user_log_write_int32_attr(log, (int32_t)data, name, unit);
        break;
    }
    case 8:
    {
        return mct_user_log_write_int64_attr(log, (int64_t)data, name, unit);
        break;
    }
    default:
    {
        return MCT_RETURN_ERROR;
        break;
    }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_int8_attr(MctContextData *log, int8_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_8BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int8_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_int16_attr(MctContextData *log, int16_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_16BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int16_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_int32_attr(MctContextData *log, int32_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_32BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int32_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_int64_attr(MctContextData *log, int64_t data, const char *name, const char *unit)
{
    uint32_t type_info = MCT_TYPE_INFO_SINT | MCT_TYLE_64BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int64_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_bool(MctContextData *log, uint8_t data)
{
    uint32_t type_info = MCT_TYPE_INFO_BOOL | MCT_TYLE_8BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, NULL);
}

MctReturnValue mct_user_log_write_bool_attr(MctContextData *log, uint8_t data, const char *name)
{
    uint32_t type_info = MCT_TYPE_INFO_BOOL | MCT_TYLE_8BIT;
    const VarInfo var_info = { name, NULL, false };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, &var_info);
}

MctReturnValue mct_user_log_write_string(MctContextData *log, const char *text)
{
    return mct_user_log_write_string_utils_attr(log, text, ASCII_STRING, NULL, false);
}

MctReturnValue mct_user_log_write_string_attr(MctContextData *log, const char *text, const char *name)
{
    return mct_user_log_write_string_utils_attr(log, text, ASCII_STRING, name, true);
}

MctReturnValue mct_user_log_write_sized_string(MctContextData *log,
                                               const char *text,
                                               uint16_t length)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, ASCII_STRING, NULL, false);
}

MctReturnValue mct_user_log_write_sized_string_attr(MctContextData *log, const char *text, uint16_t length, const char *name)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, ASCII_STRING, name, true);
}

MctReturnValue mct_user_log_write_constant_string(MctContextData *log, const char *text)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_string(log, text) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_constant_string_attr(MctContextData *log, const char *text, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_string_attr(log, text, name) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_sized_constant_string(MctContextData *log, const char *text, uint16_t length)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_string(log, text, length) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_sized_constant_string_attr(MctContextData *log, const char *text, uint16_t length, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_string_attr(log, text, length, name) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_utf8_string(MctContextData *log, const char *text)
{
    return mct_user_log_write_string_utils_attr(log, text, UTF8_STRING, NULL, false);
}

MctReturnValue mct_user_log_write_utf8_string_attr(MctContextData *log, const char *text, const char *name)
{
    return mct_user_log_write_string_utils_attr(log, text, UTF8_STRING, name, true);
}

MctReturnValue mct_user_log_write_sized_utf8_string(MctContextData *log, const char *text, uint16_t length)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, UTF8_STRING, NULL, false);
}

MctReturnValue mct_user_log_write_sized_utf8_string_attr(MctContextData *log, const char *text, uint16_t length, const char *name)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, UTF8_STRING, name, true);
}

MctReturnValue mct_user_log_write_constant_utf8_string(MctContextData *log, const char *text)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_utf8_string(log, text) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_constant_utf8_string_attr(MctContextData *log, const char *text, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_utf8_string_attr(log, text, name) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_sized_constant_utf8_string(MctContextData *log, const char *text, uint16_t length)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_utf8_string(log, text, length) : MCT_RETURN_OK;
}

MctReturnValue mct_user_log_write_sized_constant_utf8_string_attr(MctContextData *log, const char *text, uint16_t length, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_utf8_string_attr(log, text, length, name) : MCT_RETURN_OK;
}

static MctReturnValue mct_user_log_write_sized_string_utils_attr(MctContextData *log, const char *text, uint16_t length, const enum StringType type, const char *name, bool with_var_info)
{
    if ((log == NULL) || (text == NULL))
        return MCT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    const uint16_t name_size = (name != NULL) ? strlen(name)+1 : 0;

    uint16_t arg_size = (uint16_t) (length + 1);

    size_t new_log_size = log->size + arg_size + sizeof(uint16_t);

    uint32_t type_info = 0;

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        new_log_size += sizeof(uint32_t);
        if (with_var_info) {
            new_log_size += sizeof(uint16_t);  // length of "name" attribute
            new_log_size += name_size;  // the "name" attribute itself

            type_info |= MCT_TYPE_INFO_VARI;
        }
    }

    size_t str_truncate_message_length = strlen(STR_TRUNCATED_MESSAGE) + 1;
    size_t max_payload_str_msg;
    MctReturnValue ret = MCT_RETURN_OK;

    /* Check log size condition */
    if (new_log_size > mct_user.log_buf_len) {
        ret = MCT_RETURN_USER_BUFFER_FULL;

        /* Re-calculate arg_size */
        arg_size = mct_user.log_buf_len - log->size - sizeof(uint16_t);

        size_t min_payload_str_truncate_msg = log->size + str_truncate_message_length +
            sizeof(uint16_t);

        if (is_verbose_mode(mct_user.verbose_mode, log)) {
            min_payload_str_truncate_msg += sizeof(uint32_t);
            arg_size -= (uint16_t) sizeof(uint32_t);
            if (with_var_info) {
                min_payload_str_truncate_msg += sizeof(uint16_t) + name_size;
                arg_size -= sizeof(uint16_t) + name_size;
            }
        }

        /* Return when mct_user.log_buf_len does not have enough space for min_payload_str_truncate_msg */
        if (min_payload_str_truncate_msg > mct_user.log_buf_len) {
            mct_vlog(LOG_WARNING, "%s not enough minimum space to store data\n", __FUNCTION__);
            return ret;
        }

        /* Calculate the maximum size of string will be copied after truncate */
        max_payload_str_msg = mct_user.log_buf_len - min_payload_str_truncate_msg;

        if (type == UTF8_STRING) {
            /**
             * Adjust the lengh to truncate one utf8 character corectly
             * refer: https://en.wikipedia.org/wiki/UTF-8
             * one utf8 character will have maximum 4 bytes then maximum bytes will be truncate additional is 3
             */
            const char *tmp = (text + max_payload_str_msg - 3);
            uint16_t reduce_size = 0;

            if (tmp[2] & 0x80) {
                /* Is the last byte of truncated text is the first byte in multi-byte sequence (utf8 2 bytes) */
                if (tmp[2] & 0x40) {
                    reduce_size = 1;
                }
                /* Is the next to last byte of truncated text is the first byte in multi-byte sequence (utf8 3 bytes) */
                else if ((tmp[1] & 0xe0) == 0xe0) {
                    reduce_size = 2;
                }
                /* utf8 4 bytes */
                else if ((tmp[0] & 0xf0) == 0xf0) {
                    reduce_size = 3;
                }
            }

            max_payload_str_msg -= reduce_size;
            arg_size -= reduce_size;
        }
    }

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        switch (type) {
        case ASCII_STRING:
            type_info |= MCT_TYPE_INFO_STRG | MCT_SCOD_ASCII;
            break;
        case UTF8_STRING:
            type_info |= MCT_TYPE_INFO_STRG | MCT_SCOD_UTF8;
            break;
        default:
            /* Do nothing */
            break;
        }

        memcpy(log->buffer + log->size, &type_info, sizeof(uint32_t));
        log->size += sizeof(uint32_t);
    }

    memcpy(log->buffer + log->size, &arg_size, sizeof(uint16_t));
    log->size += sizeof(uint16_t);

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        if (with_var_info) {
            // Write length of "name" attribute.
            // We assume that the protocol allows zero-sized strings here (which this code will create
            // when the input pointer is NULL).
            memcpy(log->buffer + log->size, &name_size, sizeof(uint16_t));
            log->size += sizeof(uint16_t);

            // Write name string itself.
            // Must not use NULL as source pointer for memcpy. This check assures that.
            if (name_size != 0) {
                memcpy(log->buffer + log->size, name, name_size);
                log->size += name_size;
            }
        }
    }

    switch (ret) {
    case MCT_RETURN_OK:
    {
        /* Whole string will be copied */
        memcpy(log->buffer + log->size, text, length);
        /* The input string might not be null-terminated, so we're doing that by ourselves */
        log->buffer[log->size + length] = '\0';
        log->size += arg_size;
        break;
    }
    case MCT_RETURN_USER_BUFFER_FULL:
    {
        /* Only copy partial string */
        memcpy(log->buffer + log->size, text, max_payload_str_msg);
        log->size += max_payload_str_msg;

        /* Append string truncate the input string */
        memcpy(log->buffer + log->size, STR_TRUNCATED_MESSAGE, str_truncate_message_length);
        log->size += str_truncate_message_length;
        break;
    }
    default:
    {
        /* Do nothing */
        break;
    }
    }

    log->args_num++;

    return ret;
}

static MctReturnValue mct_user_log_write_string_utils_attr(MctContextData *log, const char *text, const enum StringType type, const char *name, bool with_var_info)
{
    if ((log == NULL) || (text == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    uint16_t length = (uint16_t) strlen(text);
    return mct_user_log_write_sized_string_utils_attr(log, text, length, type, name, with_var_info);
}

MctReturnValue mct_register_injection_callback_with_id(MctContext *handle,
                                                       uint32_t service_id,
                                                       mct_injection_callback_id mct_injection_cbk,
                                                       void *priv)
{
    MctContextData log;
    uint32_t i, j, k;
    int found = 0;

    MctUserInjectionCallback *old;

    if (mct_user_log_init(handle, &log) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    if (service_id < MCT_USER_INJECTION_MIN) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* This function doesn't make sense storing to local file is choosen;
     * so terminate this function */
    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    MCT_SEM_LOCK();

    if (mct_user.mct_ll_ts == NULL) {
        MCT_SEM_FREE();
        return MCT_RETURN_OK;
    }

    /* Insert callback in corresponding table */
    i = handle->log_level_pos;

    /* Insert each service_id only once */
    for (k = 0; k < mct_user.mct_ll_ts[i].nrcallbacks; k++) {
        if ((mct_user.mct_ll_ts[i].injection_table) &&
            (mct_user.mct_ll_ts[i].injection_table[k].service_id == service_id)) {
            found = 1;
            break;
        }
    }

    if (found) {
        j = k;
    } else {
        j = mct_user.mct_ll_ts[i].nrcallbacks;

        /* Allocate or expand injection table */
        if (mct_user.mct_ll_ts[i].injection_table == NULL) {
            mct_user.mct_ll_ts[i].injection_table =
                (MctUserInjectionCallback *)malloc(sizeof(MctUserInjectionCallback));

            if (mct_user.mct_ll_ts[i].injection_table == NULL) {
                MCT_SEM_FREE();
                return MCT_RETURN_ERROR;
            }
        } else {
            old = mct_user.mct_ll_ts[i].injection_table;
            mct_user.mct_ll_ts[i].injection_table = (MctUserInjectionCallback *)malloc(
                    sizeof(MctUserInjectionCallback) * (j + 1));

            if (mct_user.mct_ll_ts[i].injection_table == NULL) {
                mct_user.mct_ll_ts[i].injection_table = old;
                MCT_SEM_FREE();
                return MCT_RETURN_ERROR;
            }

            memcpy(mct_user.mct_ll_ts[i].injection_table, old, sizeof(MctUserInjectionCallback) * j);
            free(old);
        }

        mct_user.mct_ll_ts[i].nrcallbacks++;
    }

    /* Store service_id and corresponding function pointer for callback function */
    mct_user.mct_ll_ts[i].injection_table[j].service_id = service_id;

    if (priv == NULL) {
        mct_user.mct_ll_ts[i].injection_table[j].injection_callback =
            (mct_injection_callback)(void *)mct_injection_cbk;
        mct_user.mct_ll_ts[i].injection_table[j].injection_callback_with_id = NULL;
        mct_user.mct_ll_ts[i].injection_table[j].data = NULL;
    } else {
        mct_user.mct_ll_ts[i].injection_table[j].injection_callback = NULL;
        mct_user.mct_ll_ts[i].injection_table[j].injection_callback_with_id = mct_injection_cbk;
        mct_user.mct_ll_ts[i].injection_table[j].data = priv;
    }

    MCT_SEM_FREE();

    return MCT_RETURN_OK;
}

MctReturnValue mct_register_injection_callback(MctContext *handle, uint32_t service_id,
                                               int (*mct_injection_callback)(uint32_t service_id,
                                                                             void *data,
                                                                             uint32_t length))
{
    return mct_register_injection_callback_with_id(
               handle,
               service_id,
               (mct_injection_callback_id)(void *)
               mct_injection_callback,
               NULL);
}

MctReturnValue mct_register_log_level_changed_callback(MctContext *handle,
                                                       void (*mct_log_level_changed_callback)(
                                                           char context_id[MCT_ID_SIZE],
                                                           uint8_t log_level,
                                                           uint8_t trace_status))
{
    MctContextData log;
    uint32_t i;

    if (mct_user_log_init(handle, &log) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* This function doesn't make sense storing to local file is choosen;
     * so terminate this function */
    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    MCT_SEM_LOCK();

    if (mct_user.mct_ll_ts == NULL) {
        MCT_SEM_FREE();
        return MCT_RETURN_OK;
    }

    /* Insert callback in corresponding table */
    i = handle->log_level_pos;

    /* Store new callback function */
    mct_user.mct_ll_ts[i].log_level_changed_callback = mct_log_level_changed_callback;

    MCT_SEM_FREE();

    return MCT_RETURN_OK;
}


MctReturnValue mct_log_string(MctContext *handle, MctLogLevelType loglevel, const char *text)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return MCT_RETURN_ERROR;

    if ((handle == NULL) || (text == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MctReturnValue ret = MCT_RETURN_OK;
    MctContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == MCT_RETURN_TRUE) {
        ret = mct_user_log_write_string(&log, text);

        if (mct_user_log_write_finish(&log) < MCT_RETURN_OK) {
            ret = MCT_RETURN_ERROR;
        }
    }

    return ret;
}

MctReturnValue mct_log_string_int(MctContext *handle,
                                  MctLogLevelType loglevel,
                                  const char *text,
                                  int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return MCT_RETURN_ERROR;

    if ((handle == NULL) || (text == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MctReturnValue ret = MCT_RETURN_OK;
    MctContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == MCT_RETURN_TRUE) {
        ret = mct_user_log_write_string(&log, text);
        mct_user_log_write_int(&log, data);

        if (mct_user_log_write_finish(&log) < MCT_RETURN_OK) {
            ret = MCT_RETURN_ERROR;
        }
    }

    return ret;
}

MctReturnValue mct_log_string_uint(MctContext *handle,
                                   MctLogLevelType loglevel,
                                   const char *text,
                                   unsigned int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return MCT_RETURN_ERROR;

    if ((handle == NULL) || (text == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MctReturnValue ret = MCT_RETURN_OK;
    MctContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == MCT_RETURN_TRUE) {
        ret = mct_user_log_write_string(&log, text);
        mct_user_log_write_uint(&log, data);

        if (mct_user_log_write_finish(&log) < MCT_RETURN_OK) {
            ret = MCT_RETURN_ERROR;
        }
    }

    return ret;
}

MctReturnValue mct_log_int(MctContext *handle, MctLogLevelType loglevel, int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return MCT_RETURN_ERROR;

    if (handle == NULL) {
        return MCT_RETURN_ERROR;
    }

    MctContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == MCT_RETURN_TRUE) {
        mct_user_log_write_int(&log, data);

        if (mct_user_log_write_finish(&log) < MCT_RETURN_OK) {
            return MCT_RETURN_ERROR;
        }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_log_uint(MctContext *handle, MctLogLevelType loglevel, unsigned int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return MCT_RETURN_ERROR;

    if (handle == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MctContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == MCT_RETURN_TRUE) {
        mct_user_log_write_uint(&log, data);

        if (mct_user_log_write_finish(&log) < MCT_RETURN_OK) {
            return MCT_RETURN_ERROR;
        }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_log_raw(MctContext *handle,
                           MctLogLevelType loglevel,
                           void *data,
                           uint16_t length)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return MCT_RETURN_ERROR;

    if (handle == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MctContextData log;
    MctReturnValue ret = MCT_RETURN_OK;

    if (mct_user_log_write_start(handle, &log, loglevel) > 0) {
        if ((ret = mct_user_log_write_raw(&log, data, length)) < MCT_RETURN_OK) {
            mct_user_free_buffer(&(log.buffer));
            return ret;
        }

        if (mct_user_log_write_finish(&log) < MCT_RETURN_OK) {
            return MCT_RETURN_ERROR;
        }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_log_marker()
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    return mct_user_log_send_marker();
}

MctReturnValue mct_verbose_mode(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* Switch to verbose mode */
    mct_user.verbose_mode = 1;

    return MCT_RETURN_OK;
}

MctReturnValue mct_nonverbose_mode(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* Switch to non-verbose mode */
    mct_user.verbose_mode = 0;

    return MCT_RETURN_OK;
}

MctReturnValue mct_use_extended_header_for_non_verbose(int8_t use_extended_header_for_non_verbose)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* Set use_extended_header_for_non_verbose */
    mct_user.use_extended_header_for_non_verbose = use_extended_header_for_non_verbose;

    return MCT_RETURN_OK;
}

MctReturnValue mct_with_session_id(int8_t with_session_id)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* Set use_extended_header_for_non_verbose */
    mct_user.with_session_id = with_session_id;

    return MCT_RETURN_OK;
}

MctReturnValue mct_with_timestamp(int8_t with_timestamp)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* Set with_timestamp */
    mct_user.with_timestamp = with_timestamp;

    return MCT_RETURN_OK;
}

MctReturnValue mct_with_ecu_id(int8_t with_ecu_id)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    /* Set with_timestamp */
    mct_user.with_ecu_id = with_ecu_id;

    return MCT_RETURN_OK;
}

MctReturnValue mct_enable_local_print(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    mct_user.enable_local_print = 1;

    return MCT_RETURN_OK;
}

MctReturnValue mct_disable_local_print(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return MCT_RETURN_ERROR;
        }
    }

    mct_user.enable_local_print = 0;

    return MCT_RETURN_OK;
}

/* Cleanup on thread cancellation, thread may hold lock release it here */
static void mct_user_cleanup_handler(void *arg)
{
    MCT_UNUSED(arg); /* Satisfy compiler */
    /* unlock the MCT buffer */
    mct_unlock_mutex(&flush_mutex);

    /* unlock MCT (mct_mutex) */
    MCT_SEM_FREE();
}

void mct_user_housekeeperthread_function(__attribute__((unused)) void *ptr)
{
    struct timespec ts;
    bool in_loop = true;


#ifdef MCT_USE_PTHREAD_SETNAME_NP
    if (pthread_setname_np(mct_housekeeperthread_handle, "mct_housekeeper"))
        mct_log(LOG_WARNING, "Failed to rename housekeeper thread!\n");
#elif linux
    if (prctl(PR_SET_NAME, "mct_housekeeper", 0, 0, 0) < 0)
        mct_log(LOG_WARNING, "Failed to rename housekeeper thread!\n");
#endif

    pthread_cleanup_push(mct_user_cleanup_handler, NULL);

    while (in_loop) {
        /* Check for new messages from MCT daemon */
        if (!mct_user.disable_injection_msg) {
            if (mct_user_log_check_user_message() < MCT_RETURN_OK) {
                /* Critical error */
                mct_log(LOG_CRIT, "Housekeeper thread encountered error condition\n");
            }
        }

        /* flush buffer to MCT daemon if possible */
        pthread_mutex_lock(&flush_mutex);

        /* mct_buffer_empty is set by main thread to 0 in case data is written to buffer */
        if (g_mct_buffer_empty == 0) {
            /* Reattach to daemon if neccesary */
            mct_user_log_reattach_to_daemon();

            if (mct_user.mct_log_handle > 0) {
                if (mct_user_log_resend_buffer() == MCT_RETURN_OK) {
                    /* writing buffer to FIFO was successful */
                    g_mct_buffer_empty = 1; /* buffer is empty after flushing */
                    g_mct_buffer_full = 0;
                    pthread_cond_signal(&cond_free); /* signal buffer free */
                }
            }
        }

        pthread_mutex_unlock(&flush_mutex);
        /* delay */
        ts.tv_sec = 0;
        ts.tv_nsec = MCT_USER_RECEIVE_NDELAY;
        nanosleep(&ts, NULL);
    }

    pthread_cleanup_pop(1);
}

/* Private functions of user library */

MctReturnValue mct_user_log_init(MctContext *handle, MctContextData *log)
{
    int ret = MCT_RETURN_OK;

    if ((handle == NULL) || (log == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        ret = mct_init();

        if (ret < MCT_RETURN_OK) {
            if (ret != MCT_RETURN_LOGGING_DISABLED) {
                mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            }

            return ret;
        }
    }

    log->handle = handle;
    log->buffer = NULL;
    return ret;
}

MctReturnValue mct_user_log_send_log(MctContextData *log, int mtype)
{
    MctMessage msg;
    MctUserHeader userheader;
    int32_t len;

    MctReturnValue ret = MCT_RETURN_OK;

    if (!mct_user_initialised) {
        mct_vlog(LOG_ERR, "%s mct_user_initialised false\n", __FUNCTION__);
        return MCT_RETURN_ERROR;
    }

    if ((log == NULL) ||
        (log->handle == NULL) ||
        (log->handle->contextID[0] == '\0') ||
        (mtype < MCT_TYPE_LOG) || (mtype > MCT_TYPE_CONTROL)
        ) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* also for Trace messages */
    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_LOG) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    if (mct_message_init(&msg, 0) == MCT_RETURN_ERROR) {
        return MCT_RETURN_ERROR;
    }

    msg.storageheader = (MctStorageHeader *)msg.headerbuffer;

    if (mct_set_storageheader(msg.storageheader, mct_user.ecuID) == MCT_RETURN_ERROR) {
        return MCT_RETURN_ERROR;
    }

    msg.standardheader = (MctStandardHeader *)(msg.headerbuffer + sizeof(MctStorageHeader));
    msg.standardheader->htyp = MCT_HTYP_PROTOCOL_VERSION1;

    /* send ecu id */
    if (mct_user.with_ecu_id) {
        msg.standardheader->htyp |= MCT_HTYP_WEID;
    }

    /* send timestamp */
    if (mct_user.with_timestamp) {
        msg.standardheader->htyp |= MCT_HTYP_WTMS;
    }

    /* send session id */
    if (mct_user.with_session_id) {
        msg.standardheader->htyp |= MCT_HTYP_WSID;
        msg.headerextra.seid = getpid();
    }

    if (is_verbose_mode(mct_user.verbose_mode, log)){
        /* In verbose mode, send extended header */
        msg.standardheader->htyp = (msg.standardheader->htyp | MCT_HTYP_UEH);
    } else
    /* In non-verbose, send extended header if desired */
    if (mct_user.use_extended_header_for_non_verbose) {
        msg.standardheader->htyp = (msg.standardheader->htyp | MCT_HTYP_UEH);
    }

#if (BYTE_ORDER == BIG_ENDIAN)
    msg.standardheader->htyp = (msg.standardheader->htyp | MCT_HTYP_MSBF);
#endif

    msg.standardheader->mcnt = log->handle->mcnt++;

    /* Set header extra parameters */
    mct_set_id(msg.headerextra.ecu, mct_user.ecuID);

    /*msg.headerextra.seid = 0; */
    if (log->use_timestamp == MCT_AUTO_TIMESTAMP) {
        msg.headerextra.tmsp = mct_uptime();
    } else {
        msg.headerextra.tmsp = log->user_timestamp;
    }

    if (mct_message_set_extraparameters(&msg, 0) == MCT_RETURN_ERROR) {
        return MCT_RETURN_ERROR;
    }

    /* Fill out extended header, if extended header should be provided */
    if (MCT_IS_HTYP_UEH(msg.standardheader->htyp)) {
        /* with extended header */
        msg.extendedheader =
            (MctExtendedHeader *)(msg.headerbuffer + sizeof(MctStorageHeader) +
                                  sizeof(MctStandardHeader) +
                                  MCT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp));

        switch (mtype) {
            case MCT_TYPE_LOG:
            {
                msg.extendedheader->msin = (MCT_TYPE_LOG << MCT_MSIN_MSTP_SHIFT) |
                    ((log->log_level << MCT_MSIN_MTIN_SHIFT) & MCT_MSIN_MTIN);
                break;
            }
            case MCT_TYPE_NW_TRACE:
            {
                msg.extendedheader->msin = (MCT_TYPE_NW_TRACE << MCT_MSIN_MSTP_SHIFT) |
                    ((log->trace_status << MCT_MSIN_MTIN_SHIFT) & MCT_MSIN_MTIN);
                break;
            }
            default:
            {
                /* This case should not occur */
                return MCT_RETURN_ERROR;
                break;
            }
        }

        /* If in verbose mode, set flag in header for verbose mode */
        if (is_verbose_mode(mct_user.verbose_mode, log))
            msg.extendedheader->msin |= MCT_MSIN_VERB;

        msg.extendedheader->noar = log->args_num;                     /* number of arguments */
        mct_set_id(msg.extendedheader->apid, mct_user.appID);         /* application id */
        mct_set_id(msg.extendedheader->ctid, log->handle->contextID); /* context id */

        msg.headersize = sizeof(MctStorageHeader) + sizeof(MctStandardHeader) +
            sizeof(MctExtendedHeader) +
            MCT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp);
    } else {
        /* without extended header */
        msg.headersize = sizeof(MctStorageHeader) + sizeof(MctStandardHeader) +
            MCT_STANDARD_HEADER_EXTRA_SIZE(
                msg.standardheader->htyp);
    }

    len = msg.headersize - sizeof(MctStorageHeader) + log->size;

    if (len > UINT16_MAX) {
        mct_log(LOG_WARNING, "Huge message discarded!\n");
        return MCT_RETURN_ERROR;
    }

    msg.standardheader->len = MCT_HTOBE_16(len);

    /* print to std out, if enabled */
    if ((mct_user.local_print_mode != MCT_PM_FORCE_OFF) &&
        (mct_user.local_print_mode != MCT_PM_AUTOMATIC)) {
        if ((mct_user.enable_local_print) || (mct_user.local_print_mode == MCT_PM_FORCE_ON)) {
            if (mct_user_print_msg(&msg, log) == MCT_RETURN_ERROR) {
                return MCT_RETURN_ERROR;
            }
        }
    }

    if (mct_user.mct_is_file) {
        if (mct_user_file_reach_max) {
            return MCT_RETURN_FILESZERR;
        }
        else {
            /* Get file size */
            struct stat st;
            fstat(mct_user.mct_log_handle, &st);
            mct_vlog(LOG_DEBUG, "%s: Current file size=[%ld]\n", __func__,
                     st.st_size);

            /* Check filesize */
            /* Return error if the file size has reached to maximum */
            unsigned int msg_size = st.st_size + (unsigned int) msg.headersize +
                                    (unsigned int) log->size;
            if (msg_size > mct_user.filesize_max) {
                mct_user_file_reach_max = true;
                mct_vlog(LOG_ERR,
                         "%s: File size (%ld bytes) reached to defined maximum size (%d bytes)\n",
                         __func__, st.st_size, mct_user.filesize_max);
                return MCT_RETURN_FILESZERR;
            }
            else {
                /* log to file */
                ret = mct_user_log_out2(mct_user.mct_log_handle,
                                        msg.headerbuffer, msg.headersize,
                                        log->buffer, log->size);
                return ret;
            }
        }
    } else {

        if (mct_user.overflow_counter) {
            if (mct_user_log_send_overflow() == MCT_RETURN_OK) {
                mct_vnlog(LOG_WARNING,
                          MCT_USER_BUFFER_LENGTH,
                          "%u messages discarded!\n",
                          mct_user.overflow_counter);
                mct_user.overflow_counter = 0;
            }
        }

        /* try to resent old data first */
        ret = MCT_RETURN_OK;

        if ((mct_user.mct_log_handle != -1) && (mct_user.appID[0] != '\0')) {
            /* buffer not empty */
            if (g_mct_buffer_empty == 0) {
                ret = mct_user_log_resend_buffer();
            }
        }

        if ((ret == MCT_RETURN_OK) && (mct_user.appID[0] != '\0')) {
            pthread_mutex_lock(&flush_mutex);
            /* resend ok or nothing to resent */
            g_mct_buffer_empty = 1;
            pthread_mutex_unlock(&flush_mutex);
            ret = mct_user_log_out3(mct_user.mct_log_handle,
                                    &(userheader), sizeof(MctUserHeader),
                                    msg.headerbuffer + sizeof(MctStorageHeader),
                                    msg.headersize - sizeof(MctStorageHeader),
                                    log->buffer, log->size);

        }

        MctReturnValue process_error_ret = MCT_RETURN_OK;
        /* store message in ringbuffer, if an error has occured */
        if ((ret != MCT_RETURN_OK) || (mct_user.appID[0] == '\0')) {
            process_error_ret = mct_user_log_out_error_handling(&(userheader),
                                                  sizeof(MctUserHeader),
                                                  msg.headerbuffer + sizeof(MctStorageHeader),
                                                  msg.headersize - sizeof(MctStorageHeader),
                                                  log->buffer,
                                                  log->size);
        }

        if (process_error_ret == MCT_RETURN_OK)
            return MCT_RETURN_OK;
        if (process_error_ret == MCT_RETURN_BUFFER_FULL) {
            /* Buffer full */
            mct_user.overflow_counter += 1;
            return MCT_RETURN_BUFFER_FULL;
        }

        /* handle return value of function mct_user_log_out3() when process_error_ret < 0*/
        switch (ret) {
            case MCT_RETURN_PIPE_FULL:
            {
                /* data could not be written */
                return MCT_RETURN_PIPE_FULL;
            }
            case MCT_RETURN_PIPE_ERROR:
            {
                /* handle not open or pipe error */
                close(mct_user.mct_log_handle);
                mct_user.mct_log_handle = -1;
#if defined MCT_LIB_USE_UNIX_SOCKET_IPC
                mct_user.connection_state = MCT_USER_RETRY_CONNECT;
#endif

                if (mct_user.local_print_mode == MCT_PM_AUTOMATIC) {
                    mct_user_print_msg(&msg, log);
                }

                return MCT_RETURN_PIPE_ERROR;
            }
            case MCT_RETURN_ERROR:
            {
                /* other error condition */
                return MCT_RETURN_ERROR;
            }
            case MCT_RETURN_OK:
            {
                return MCT_RETURN_OK;
            }
            default:
            {
                /* This case should never occur. */
                return MCT_RETURN_ERROR;
            }
        }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_send_register_application(void)
{
    MctUserHeader userheader;
    MctUserControlMsgRegisterApplication usercontext;

    MctReturnValue ret;

    if (mct_user.appID[0] == '\0') {
        return MCT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader,
                                MCT_USER_MESSAGE_REGISTER_APPLICATION) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, mct_user.appID);       /* application id */
    usercontext.pid = getpid();

    if (mct_user.application_description != NULL) {
        usercontext.description_length = strlen(mct_user.application_description);
    } else {
        usercontext.description_length = 0;
    }

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    ret = mct_user_log_out3(mct_user.mct_log_handle,
                            &(userheader), sizeof(MctUserHeader),
                            &(usercontext), sizeof(MctUserControlMsgRegisterApplication),
                            mct_user.application_description, usercontext.description_length);

    /* store message in ringbuffer, if an error has occured */
    if (ret < MCT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(MctUserHeader),
                                               &(usercontext),
                                               sizeof(MctUserControlMsgRegisterApplication),
                                               mct_user.application_description,
                                               usercontext.description_length);
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_send_unregister_application(void)
{
    MctUserHeader userheader;
    MctUserControlMsgUnregisterApplication usercontext;
    MctReturnValue ret = MCT_RETURN_OK;

    if (mct_user.appID[0] == '\0') {
        return MCT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader,
                                MCT_USER_MESSAGE_UNREGISTER_APPLICATION) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, mct_user.appID);       /* application id */
    usercontext.pid = getpid();

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader), sizeof(MctUserHeader),
                            &(usercontext), sizeof(MctUserControlMsgUnregisterApplication));

    /* store message in ringbuffer, if an error has occured */
    if (ret < MCT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(MctUserHeader),
                                               &(usercontext),
                                               sizeof(MctUserControlMsgUnregisterApplication),
                                               NULL,
                                               0);
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_send_register_context(MctContextData *log)
{
    MctUserHeader userheader;
    MctUserControlMsgRegisterContext usercontext;
    MctReturnValue ret = MCT_RETURN_ERROR;

    if (log == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (log->handle == NULL) {
        return MCT_RETURN_ERROR;
    }

    if (log->handle->contextID[0] == '\0') {
        return MCT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_REGISTER_CONTEXT) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, mct_user.appID);         /* application id */
    mct_set_id(usercontext.ctid, log->handle->contextID); /* context id */
    usercontext.log_level_pos = log->handle->log_level_pos;
    usercontext.pid = getpid();

    usercontext.log_level = (int8_t)log->log_level;
    usercontext.trace_status = (int8_t)log->trace_status;

    if (log->context_description != NULL) {
        usercontext.description_length = strlen(log->context_description);
    } else {
        usercontext.description_length = 0;
    }

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    if (mct_user.appID[0] != '\0') {
        ret =
            mct_user_log_out3(mct_user.mct_log_handle,
                              &(userheader),
                              sizeof(MctUserHeader),
                              &(usercontext),
                              sizeof(MctUserControlMsgRegisterContext),
                              log->context_description,
                              usercontext.description_length);
    }

    /* store message in ringbuffer, if an error has occured */
    if ((ret != MCT_RETURN_OK) || (mct_user.appID[0] == '\0')) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(MctUserHeader),
                                               &(usercontext),
                                               sizeof(MctUserControlMsgRegisterContext),
                                               log->context_description,
                                               usercontext.description_length);
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_send_unregister_context(MctContextData *log)
{
    MctUserHeader userheader;
    MctUserControlMsgUnregisterContext usercontext;
    MctReturnValue ret;

    if (log == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (log->handle == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (log->handle->contextID[0] == '\0') {
        return MCT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader,
                                MCT_USER_MESSAGE_UNREGISTER_CONTEXT) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, mct_user.appID);         /* application id */
    mct_set_id(usercontext.ctid, log->handle->contextID); /* context id */
    usercontext.pid = getpid();

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader),
                            sizeof(MctUserHeader),
                            &(usercontext),
                            sizeof(MctUserControlMsgUnregisterContext));

    /* store message in ringbuffer, if an error has occured */
    if (ret < MCT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(MctUserHeader),
                                               &(usercontext),
                                               sizeof(MctUserControlMsgUnregisterContext),
                                               NULL,
                                               0);
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_send_app_ll_ts_limit(const char *apid,
                                        MctLogLevelType loglevel,
                                        MctTraceStatusType tracestatus)
{
    MctUserHeader userheader;
    MctUserControlMsgAppLogLevelTraceStatus usercontext;
    MctReturnValue ret;

    if ((loglevel < MCT_USER_LOG_LEVEL_NOT_SET) || (loglevel >= MCT_LOG_MAX)) {
        mct_vlog(LOG_ERR, "Loglevel %d is outside valid range", loglevel);
        return MCT_RETURN_ERROR;
    }

    if ((tracestatus < MCT_USER_TRACE_STATUS_NOT_SET) || (tracestatus >= MCT_TRACE_STATUS_MAX)) {
        mct_vlog(LOG_ERR, "Tracestatus %d is outside valid range", tracestatus);
        return MCT_RETURN_ERROR;
    }

    if ((apid == NULL) || (apid[0] == '\0')) {
        return MCT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_APP_LL_TS) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, apid);       /* application id */
    usercontext.log_level = loglevel;
    usercontext.trace_status = tracestatus;

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader), sizeof(MctUserHeader),
                            &(usercontext), sizeof(MctUserControlMsgAppLogLevelTraceStatus));

    /* store message in ringbuffer, if an error has occured */
    if (ret < MCT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(MctUserHeader),
                                               &(usercontext),
                                               sizeof(MctUserControlMsgAppLogLevelTraceStatus),
                                               NULL,
                                               0);
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_send_marker()
{
    MctUserHeader userheader;
    MctReturnValue ret;

    /* set userheader */
    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_MARKER) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    /* log to FIFO */
    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader), sizeof(MctUserHeader), 0, 0);

    /* store message in ringbuffer, if an error has occured */
    if (ret < MCT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(MctUserHeader),
                                               NULL,
                                               0,
                                               NULL,
                                               0);
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_print_msg(MctMessage *msg, MctContextData *log)
{
    uint8_t *databuffer_tmp;
    int32_t datasize_tmp;
    int32_t databuffersize_tmp;
    static char text[MCT_USER_TEXT_LENGTH];

    if ((msg == NULL) || (log == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* Save variables before print */
    databuffer_tmp = msg->databuffer;
    datasize_tmp = msg->datasize;
    databuffersize_tmp = msg->databuffersize;

    /* Act like a receiver, convert header back to host format */
    msg->standardheader->len = MCT_BETOH_16(msg->standardheader->len);
    mct_message_get_extraparameters(msg, 0);

    msg->databuffer = log->buffer;
    msg->datasize = log->size;
    msg->databuffersize = log->size;

    /* Print message as ASCII */
    if (mct_message_print_ascii(msg, text, MCT_USER_TEXT_LENGTH, 0) == MCT_RETURN_ERROR) {
        return MCT_RETURN_ERROR;
    }

    /* Restore variables and set len to BE*/
    msg->databuffer = databuffer_tmp;
    msg->databuffersize = databuffersize_tmp;
    msg->datasize = datasize_tmp;

    msg->standardheader->len = MCT_HTOBE_16(msg->standardheader->len);

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_check_user_message(void)
{
    int offset = 0;
    int leave_while = 0;
    int ret = 0;
    int poll_timeout = MCT_USER_RECEIVE_MDELAY;

    uint32_t i;
    int fd;
    struct pollfd nfd[1];

    MctUserHeader *userheader;
    MctReceiver *receiver = &(mct_user.receiver);

    MctUserControlMsgLogLevel *usercontextll;
    MctUserControlMsgInjection *usercontextinj;
    MctUserControlMsgLogState *userlogstate;
    MctUserControlMsgBlockMode *blockmode;
    unsigned char *userbuffer;

    /* For delayed calling of injection callback, to avoid deadlock */
    MctUserInjectionCallback delayed_injection_callback;
    MctUserLogLevelChangedCallback delayed_log_level_changed_callback;
    unsigned char *delayed_inject_buffer = 0;
    uint32_t delayed_inject_data_length = 0;

    /* Ensure that callback is null before searching for it */
    delayed_injection_callback.injection_callback = 0;
    delayed_injection_callback.injection_callback_with_id = 0;
    delayed_injection_callback.service_id = 0;
    delayed_log_level_changed_callback.log_level_changed_callback = 0;
    delayed_injection_callback.data = 0;

#if defined MCT_LIB_USE_UNIX_SOCKET_IPC
    fd = mct_user.mct_log_handle;
#else /* MCT_LIB_USE_FIFO_IPC */
    fd = mct_user.mct_user_handle;
#endif

    nfd[0].events = POLLIN;
    nfd[0].fd = fd;

#if defined MCT_LIB_USE_UNIX_SOCKET_IPC

    if (fd != MCT_FD_INIT) {
#else /* MCT_LIB_USE_FIFO_IPC */

    if ((fd != MCT_FD_INIT) && (mct_user.mct_log_handle > 0)) {
#endif
        ret = poll(nfd, 1, poll_timeout);

        if (ret) {
            if (nfd[0].revents & (POLLHUP | POLLNVAL | POLLERR)) {
                mct_user.mct_log_handle = MCT_FD_INIT;
                return MCT_RETURN_ERROR;
            }

            if (mct_receiver_receive(receiver) <= 0) {
                /* No new message available */
                return MCT_RETURN_OK;
            }

            /* look through buffer as long as data is in there */
            while (1) {
                if (receiver->bytesRcvd < (int32_t)sizeof(MctUserHeader)) {
                    break;
                }

                /* resync if necessary */
                offset = 0;

                do {
                    userheader = (MctUserHeader *)(receiver->buf + offset);

                    /* Check for user header pattern */
                    if (mct_user_check_userheader(userheader)) {
                        break;
                    }

                    offset++;
                } while ((int32_t)(sizeof(MctUserHeader) + offset) <= receiver->bytesRcvd);

                /* Check for user header pattern */
                if ((mct_user_check_userheader(userheader) < 0) ||
                    (mct_user_check_userheader(userheader) == 0)) {
                    break;
                }

                /* Set new start offset */
                if (offset > 0) {
                    receiver->buf += offset;
                    receiver->bytesRcvd -= offset;
                }

                switch (userheader->message) {
                    case MCT_USER_MESSAGE_LOG_LEVEL:
                    {
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(MctUserHeader) + sizeof(MctUserControlMsgLogLevel))) {
                            leave_while = 1;
                            break;
                        }

                        usercontextll =
                            (MctUserControlMsgLogLevel *)(receiver->buf + sizeof(MctUserHeader));

                        /* Update log level and trace status */
                        if (usercontextll != NULL) {
                            MCT_SEM_LOCK();

                            if ((usercontextll->log_level_pos >= 0) &&
                                (usercontextll->log_level_pos <
                                 (int32_t)mct_user.mct_ll_ts_num_entries)) {
                                if (mct_user.mct_ll_ts) {
                                    mct_user.mct_ll_ts[usercontextll->log_level_pos].log_level =
                                        usercontextll->log_level;
                                    mct_user.mct_ll_ts[usercontextll->log_level_pos].trace_status =
                                        usercontextll->trace_status;

                                    if (mct_user.mct_ll_ts[usercontextll->log_level_pos].
                                        log_level_ptr) {
                                        *(mct_user.mct_ll_ts[usercontextll->log_level_pos].
                                          log_level_ptr) =
                                            usercontextll->log_level;
                                    }

                                    if (mct_user.mct_ll_ts[usercontextll->log_level_pos].
                                        trace_status_ptr) {
                                        *(mct_user.mct_ll_ts[usercontextll->log_level_pos].
                                          trace_status_ptr) =
                                            usercontextll->trace_status;
                                    }

                                    delayed_log_level_changed_callback.log_level_changed_callback =
                                        mct_user.mct_ll_ts[usercontextll->log_level_pos].
                                        log_level_changed_callback;
                                    memcpy(
                                        delayed_log_level_changed_callback.contextID,
                                        mct_user.mct_ll_ts[usercontextll->log_level_pos].
                                        contextID,
                                        MCT_ID_SIZE);
                                    delayed_log_level_changed_callback.log_level =
                                        usercontextll->log_level;
                                    delayed_log_level_changed_callback.trace_status =
                                        usercontextll->trace_status;
                                }
                            }

                            MCT_SEM_FREE();
                        }

                        /* call callback outside of semaphore */
                        if (delayed_log_level_changed_callback.log_level_changed_callback != 0) {
                            delayed_log_level_changed_callback.log_level_changed_callback(
                                delayed_log_level_changed_callback.contextID,
                                delayed_log_level_changed_callback.log_level,
                                delayed_log_level_changed_callback.trace_status);
                        }

                        /* keep not read data in buffer */
                        if (mct_receiver_remove(receiver,
                                                sizeof(MctUserHeader) +
                                                sizeof(MctUserControlMsgLogLevel)) ==
                            MCT_RETURN_ERROR) {
                            return MCT_RETURN_ERROR;
                        }
                    }
                    break;
                    case MCT_USER_MESSAGE_INJECTION:
                    {
                        /* At least, user header, user context, and service id and data_length of injected message is available */
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(MctUserHeader) +
                                      sizeof(MctUserControlMsgInjection))) {
                            leave_while = 1;
                            break;
                        }

                        usercontextinj =
                            (MctUserControlMsgInjection *)(receiver->buf + sizeof(MctUserHeader));
                        userbuffer =
                            (unsigned char *)(receiver->buf + sizeof(MctUserHeader) +
                                              sizeof(MctUserControlMsgInjection));

                        if (userbuffer != NULL) {

                            if (receiver->bytesRcvd <
                                (int32_t)(sizeof(MctUserHeader) +
                                          sizeof(MctUserControlMsgInjection) +
                                          usercontextinj->data_length_inject)) {
                                leave_while = 1;
                                break;
                            }

                            MCT_SEM_LOCK();

                            if ((usercontextinj->data_length_inject > 0) && (mct_user.mct_ll_ts)) {
                                /* Check if injection callback is registered for this context */
                                for (i = 0;
                                     i <
                                     mct_user.mct_ll_ts[usercontextinj->log_level_pos].nrcallbacks;
                                     i++) {
                                    if ((mct_user.mct_ll_ts[usercontextinj->log_level_pos].
                                         injection_table) &&
                                        (mct_user.mct_ll_ts[usercontextinj->log_level_pos].
                                         injection_table[i].service_id ==
                                         usercontextinj->service_id)) {
                                        /* Prepare delayed injection callback call */
                                        if (mct_user.mct_ll_ts[usercontextinj->log_level_pos].
                                            injection_table[i].
                                            injection_callback != NULL) {
                                            delayed_injection_callback.injection_callback =
                                                mct_user.mct_ll_ts[usercontextinj->log_level_pos].
                                                injection_table[i].
                                                injection_callback;
                                        } else if (mct_user.mct_ll_ts[usercontextinj->log_level_pos
                                                   ].injection_table[i].
                                                   injection_callback_with_id != NULL) {
                                            delayed_injection_callback.injection_callback_with_id =
                                                mct_user.mct_ll_ts[usercontextinj->log_level_pos].
                                                injection_table[i].
                                                injection_callback_with_id;
                                            delayed_injection_callback.data =
                                                mct_user.mct_ll_ts[usercontextinj->log_level_pos].
                                                injection_table[i].data;
                                        }

                                        delayed_injection_callback.service_id =
                                            usercontextinj->service_id;
                                        delayed_inject_data_length =
                                            usercontextinj->data_length_inject;
                                        delayed_inject_buffer = malloc(delayed_inject_data_length);

                                        if (delayed_inject_buffer != NULL) {
                                            memcpy(delayed_inject_buffer,
                                                   userbuffer,
                                                   delayed_inject_data_length);
                                        } else {
                                            MCT_SEM_FREE();
                                            mct_log(LOG_WARNING, "malloc failed!\n");
                                            return MCT_RETURN_ERROR;
                                        }

                                        break;
                                    }
                                }
                            }

                            MCT_SEM_FREE();

                            /* Delayed injection callback call */
                            if ((delayed_inject_buffer != NULL) &&
                                (delayed_injection_callback.injection_callback != NULL)) {
                                delayed_injection_callback.injection_callback(
                                    delayed_injection_callback.service_id,
                                    delayed_inject_buffer,
                                    delayed_inject_data_length);
                                delayed_injection_callback.injection_callback = NULL;
                            } else if ((delayed_inject_buffer != NULL) &&
                                       (delayed_injection_callback.injection_callback_with_id !=
                                        NULL)) {
                                delayed_injection_callback.injection_callback_with_id(
                                    delayed_injection_callback.service_id,
                                    delayed_inject_buffer,
                                    delayed_inject_data_length,
                                    delayed_injection_callback
                                    .data);
                                delayed_injection_callback.injection_callback_with_id = NULL;
                            }

                            free(delayed_inject_buffer);
                            delayed_inject_buffer = NULL;

                            /* keep not read data in buffer */
                            if (mct_receiver_remove(receiver,
                                                    (sizeof(MctUserHeader) +
                                                     sizeof(MctUserControlMsgInjection) +
                                                     usercontextinj->data_length_inject)) !=
                                MCT_RETURN_OK) {
                                return MCT_RETURN_ERROR;
                            }
                        }
                    }
                    break;
                    case MCT_USER_MESSAGE_LOG_STATE:
                    {
                        /* At least, user header, user context, and service id and data_length of injected message is available */
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(MctUserHeader) + sizeof(MctUserControlMsgLogState))) {
                            leave_while = 1;
                            break;
                        }

                        userlogstate =
                            (MctUserControlMsgLogState *)(receiver->buf + sizeof(MctUserHeader));
                        mct_user.log_state = userlogstate->log_state;

                        /* keep not read data in buffer */
                        if (mct_receiver_remove(receiver,
                                                (sizeof(MctUserHeader) +
                                                 sizeof(MctUserControlMsgLogState))) ==
                            MCT_RETURN_ERROR) {
                            return MCT_RETURN_ERROR;
                        }
                    }
                    break;
                    case MCT_USER_MESSAGE_SET_BLOCK_MODE:
                    {
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(MctUserHeader) +
                                      sizeof(MctUserControlMsgBlockMode))) {
                            leave_while = 1;
                            break;
                        }

                        blockmode =
                            (MctUserControlMsgBlockMode *)(receiver->buf + sizeof(MctUserHeader));

                        /* only handle daemon requests if block mode not forced */
                        if (mct_user.force_blocking == 0) {
                            /* in case main thread currently blocks, it need to be
                             * waken up */
                            pthread_mutex_lock(&flush_mutex);
                            mct_user_set_blockmode(blockmode->block_mode);
                            pthread_cond_signal(&cond_free); /* signal buffer free */
                            pthread_mutex_unlock(&flush_mutex);
                        } else {
                            mct_log(LOG_INFO,
                                    "Forced Blockmode: Ignore Daemon request!\n");
                        }

                        /* keep not read data in buffer */
                        if (mct_receiver_remove(receiver,
                                                (sizeof(MctUserHeader) +
                                                 sizeof(MctUserControlMsgBlockMode))) == -1) {
                            return -1;
                        }
                    }
                    break;
                    default:
                    {
                        mct_log(LOG_WARNING, "Invalid user message type received!\n");
                        /* Ignore result */
                        mct_receiver_remove(receiver, sizeof(MctUserHeader));
                        /* In next invocation of while loop, a resync will be triggered if additional data was received */
                    }
                    break;
                } /* switch() */

                if (leave_while == 1) {
                    leave_while = 0;
                    break;
                }
            } /* while buffer*/

            if (mct_receiver_move_to_begin(receiver) == MCT_RETURN_ERROR) {
                return MCT_RETURN_ERROR;
            }
        } /* while receive */
    }     /* if */

    return MCT_RETURN_OK;
}

MctReturnValue mct_user_log_resend_buffer(void)
{
    int num, count;
    int size;
    MctReturnValue ret;

    MCT_SEM_LOCK();

    if (mct_user.appID[0] == '\0') {
        MCT_SEM_FREE();
        return 0;
    }

    /* Send content of ringbuffer */
    count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
    MCT_SEM_FREE();

    for (num = 0; num < count; num++) {

        MCT_SEM_LOCK();
        size = mct_buffer_copy(&(mct_user.startup_buffer),
                               mct_user.resend_buffer,
                               mct_user.log_buf_len);

        if (size > 0) {
            MctUserHeader *userheader = (MctUserHeader *)(mct_user.resend_buffer);

            /* Add application id to the messages of needed*/
            if (mct_user_check_userheader(userheader)) {
                switch (userheader->message) {
                    case MCT_USER_MESSAGE_REGISTER_CONTEXT:
                    {
                        MctUserControlMsgRegisterContext *usercontext =
                            (MctUserControlMsgRegisterContext *)(mct_user.resend_buffer +
                                                                 sizeof(MctUserHeader));

                        if ((usercontext != 0) && (usercontext->apid[0] == '\0')) {
                            mct_set_id(usercontext->apid, mct_user.appID);
                        }

                        break;
                    }
                    case MCT_USER_MESSAGE_LOG:
                    {
                        MctExtendedHeader *extendedHeader =
                            (MctExtendedHeader *)(mct_user.resend_buffer + sizeof(MctUserHeader) +
                                                  sizeof(MctStandardHeader) +
                                                  sizeof(MctStandardHeaderExtra));

                        if (((extendedHeader) != 0) && (extendedHeader->apid[0] == '\0')) { /* if application id is empty, add it */
                            mct_set_id(extendedHeader->apid, mct_user.appID);
                        }

                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            ret = mct_user_log_out3(mct_user.mct_log_handle,
                                    mct_user.resend_buffer,
                                    size,
                                    0,
                                    0,
                                    0,
                                    0);

            /* in case of error, keep message in ringbuffer */
            if (ret == MCT_RETURN_OK) {
                mct_buffer_remove(&(mct_user.startup_buffer));
            } else {
                if (ret == MCT_RETURN_PIPE_ERROR) {
                    /* handle not open or pipe error */
                    close(mct_user.mct_log_handle);
                    mct_user.mct_log_handle = -1;
                }

                /* keep message in ringbuffer */
                MCT_SEM_FREE();
                return ret;
            }
        }

        MCT_SEM_FREE();
    }

    return MCT_RETURN_OK;
}

void mct_user_log_reattach_to_daemon(void)
{
    uint32_t num;
    MctContext handle;
    MctContextData log_new;

    if (mct_user.mct_log_handle < 0) {
        mct_user.mct_log_handle = MCT_FD_INIT;

#ifdef MCT_LIB_USE_UNIX_SOCKET_IPC
        /* try to open connection to mct daemon */
        mct_initialize_socket_connection();

        if (mct_user.connection_state != MCT_USER_CONNECTED) {
            /* return if not connected */
            return;
        }
#else   /* MCT_LIB_USE_FIFO_IPC */
      /* try to open pipe to mct daemon */
        int fd = open(mct_daemon_fifo, O_WRONLY | O_NONBLOCK);

        if (fd < 0) {
            return;
        }

        mct_user.mct_log_handle = fd;
#endif

        if (mct_user_log_init(&handle, &log_new) < MCT_RETURN_OK) {
            return;
        }

        mct_log(LOG_NOTICE, "Logging (re-)enabled!\n");

        /* Re-register application */
        if (mct_user_log_send_register_application() < MCT_RETURN_ERROR) {
            return;
        }

        MCT_SEM_LOCK();

        /* Re-register all stored contexts */
        for (num = 0; num < mct_user.mct_ll_ts_num_entries; num++) {
            /* Re-register stored context */
            if ((mct_user.appID[0] != '\0') && (mct_user.mct_ll_ts) &&
                (mct_user.mct_ll_ts[num].contextID[0] != '\0')) {
                /*mct_set_id(log_new.appID, mct_user.appID); */
                mct_set_id(handle.contextID, mct_user.mct_ll_ts[num].contextID);
                handle.log_level_pos = num;
                log_new.context_description = mct_user.mct_ll_ts[num].context_description;

                /* Release the mutex for sending context registration: */
                /* function  mct_user_log_send_register_context() can take the mutex to write to the MCT buffer. => dead lock */
                MCT_SEM_FREE();

                log_new.log_level = MCT_USER_LOG_LEVEL_NOT_SET;
                log_new.trace_status = MCT_USER_TRACE_STATUS_NOT_SET;

                if (mct_user_log_send_register_context(&log_new) < MCT_RETURN_ERROR) {
                    return;
                }

                /* Lock again the mutex */
                /* it is necessary in the for(;;) test, in order to have coherent mct_user data all over the critical section. */
                MCT_SEM_LOCK();
            }
        }

        MCT_SEM_FREE();
    }
}

MctReturnValue mct_user_log_send_overflow(void)
{
    MctUserHeader userheader;
    MctUserControlMsgBufferOverflow userpayload;

    /* set userheader */
    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_OVERFLOW) < MCT_RETURN_OK) {
        return MCT_RETURN_ERROR;
    }

    if (mct_user.mct_is_file) {
        return MCT_RETURN_OK;
    }

    /* set user message parameters */
    userpayload.overflow_counter = mct_user.overflow_counter;
    mct_set_id(userpayload.apid, mct_user.appID);

    return mct_user_log_out2(mct_user.mct_log_handle,
                             &(userheader), sizeof(MctUserHeader),
                             &(userpayload), sizeof(MctUserControlMsgBufferOverflow));
}

MctReturnValue mct_user_check_buffer(int *total_size, int *used_size)
{
    if ((total_size == NULL) || (used_size == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MCT_SEM_LOCK();

    *total_size = mct_buffer_get_total_size(&(mct_user.startup_buffer));
    *used_size = mct_buffer_get_used_size(&(mct_user.startup_buffer));

    MCT_SEM_FREE();
    return MCT_RETURN_OK; /* ok */
}


int mct_start_threads(int id)
{
    if ((mct_housekeeperthread_handle == 0) && (id & MCT_USER_HOUSEKEEPER_THREAD)) {
        /* Start housekeeper thread */
        if (pthread_create(&mct_housekeeperthread_handle, 0,
                           (void *)&mct_user_housekeeperthread_function, 0) != 0) {
            mct_log(LOG_CRIT, "Failed to create housekeeper thread!\n");
            return -1;
        }
    }
    return 0;
}

void mct_stop_threads()
{
    int mct_housekeeperthread_result = 0;
    int joined = 0;

    if (mct_housekeeperthread_handle) {
        /* do not ignore return value */
        mct_housekeeperthread_result = pthread_cancel(mct_housekeeperthread_handle);
        if (mct_housekeeperthread_result != 0) {
            mct_vlog(LOG_ERR,
                     "ERROR %s(mct_housekeeperthread_handle): %s\n",
                     "pthread_cancel",
                     strerror(mct_housekeeperthread_result));
        }
    }

    /* make sure that the threads really finished working */
    if ((mct_housekeeperthread_result == 0) && mct_housekeeperthread_handle) {
        joined = pthread_join(mct_housekeeperthread_handle, NULL);

        if (joined != 0) {
            mct_vlog(LOG_ERR,
                     "ERROR pthread_join(mct_housekeeperthread_handle, NULL): %s\n",
                     strerror(joined));
        }

        mct_housekeeperthread_handle = 0; /* set to invalid */
    }
}

static void mct_fork_child_fork_handler()
{
    g_mct_is_child = 1;
    mct_user_initialised = false;
    mct_user.mct_log_handle = -1;
}

MctReturnValue mct_user_log_out_error_handling(void *ptr1, size_t len1,
                                               void *ptr2, size_t len2,
                                               void *ptr3, size_t len3)
{
    MctReturnValue ret = MCT_RETURN_ERROR;
    int msg_size = len1 + len2 + len3;

    MCT_SEM_LOCK();
    ret = mct_buffer_check_size(&(mct_user.startup_buffer), msg_size);
    MCT_SEM_FREE();

    if ((ret != MCT_RETURN_OK) && (mct_user_get_blockmode() == MCT_MODE_BLOCKING)) {
        pthread_mutex_lock(&flush_mutex);
        g_mct_buffer_empty = 0;

        /* block until buffer free */
        g_mct_buffer_full = 1;

        while (g_mct_buffer_full == 1) {
            pthread_cond_wait(&cond_free, &flush_mutex);
        }

        MCT_SEM_LOCK();

        if (mct_buffer_push3(&(mct_user.startup_buffer),
                             ptr1, len1,
                             ptr2, len2,
                             ptr3, len3) != MCT_RETURN_OK) {
            if (mct_user.overflow_counter == 0) {
                mct_log(LOG_CRIT,
                        "Buffer full! Messages will be discarded in BLOCKING.\n");
            }

            ret = MCT_RETURN_BUFFER_FULL;
        }

        MCT_SEM_FREE();

        pthread_mutex_unlock(&flush_mutex);
    } else {
        MCT_SEM_LOCK();

        if (mct_buffer_push3(&(mct_user.startup_buffer),
                             ptr1, len1,
                             ptr2, len2,
                             ptr3, len3) != MCT_RETURN_OK) {
            if (mct_user.overflow_counter == 0) {
                mct_log(LOG_WARNING,
                        "Buffer full! Messages will be discarded.\n");
            }

            ret = MCT_RETURN_BUFFER_FULL;
        }

        MCT_SEM_FREE();

        pthread_mutex_lock(&flush_mutex);
        g_mct_buffer_empty = 0;
        pthread_mutex_unlock(&flush_mutex);
    }

    return ret;
}

static MctReturnValue mct_user_set_blockmode(int8_t mode)
{
    if ((mode < MCT_MODE_NON_BLOCKING) || (mode > MCT_MODE_BLOCKING)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MCT_SEM_LOCK();
    mct_user.block_mode = mode;
    MCT_SEM_FREE();

    return MCT_RETURN_OK;
}

static int mct_user_get_blockmode()
{
    int bm;
    MCT_SEM_LOCK();
    bm = mct_user.block_mode;
    MCT_SEM_FREE();
    return bm;
}
