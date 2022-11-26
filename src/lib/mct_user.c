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

#if defined DLT_LIB_USE_UNIX_SOCKET_IPC
#include <sys/socket.h>
#endif
#ifdef DLT_LIB_USE_UNIX_SOCKET_IPC
#include <sys/un.h>
#endif


#include "mct_user.h"
#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"
#include "mct_user_cfg.h"

#ifdef DLT_FATAL_LOG_RESET_ENABLE
#define DLT_LOG_FATAL_RESET_TRAP(LOGLEVEL) \
    do {                                   \
        if (LOGLEVEL == DLT_LOG_FATAL) {   \
            int *p = NULL;                 \
            *p = 0;                        \
        }                                  \
    } while (0)
#else /* DLT_FATAL_LOG_RESET_ENABLE */
#define DLT_LOG_FATAL_RESET_TRAP(LOGLEVEL)
#endif /* DLT_FATAL_LOG_RESET_ENABLE */

#ifndef DLT_HP_LOG_ENABLE
static DltUser mct_user;
#else
DltUser mct_user;
#endif
static atomic_bool mct_user_initialised = false;
static int mct_user_freeing = 0;
static bool mct_user_file_reach_max = false;
static bool mct_user_app_unreg = true;

#ifdef DLT_LIB_USE_FIFO_IPC
static char mct_user_dir[DLT_PATH_MAX];
static char mct_daemon_fifo[DLT_PATH_MAX];
#endif

#ifndef DLT_HP_LOG_ENABLE
static sem_t mct_mutex;
#else
sem_t mct_mutex;
#endif
static pthread_t mct_housekeeperthread_handle;

/* calling mct_user_atexit_handler() second time fails with error message */
static int atexit_registered = 0;

/* used to disallow DLT usage in fork() child */
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

#define DLT_UNUSED(x) (void)(x)

/* Thread definitions */
#define DLT_USER_NO_THREAD            0
#define DLT_USER_HOUSEKEEPER_THREAD  (1 << 1)

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
    DltContext *handle;
    uint32_t id;
    DltNetworkTraceType nw_trace_type;
    uint32_t header_len;
    void *header;
    uint32_t payload_len;
    void *payload;
} s_segmented_data;

/* Function prototypes for internally used functions */
static void mct_user_housekeeperthread_function(void *ptr);
static void mct_user_atexit_handler(void);
static DltReturnValue mct_user_log_init(DltContext *handle, DltContextData *log);
static DltReturnValue mct_user_log_send_log(DltContextData *log, int mtype);
static DltReturnValue mct_user_log_send_register_application(void);
static DltReturnValue mct_user_log_send_unregister_application(void);
static DltReturnValue mct_user_log_send_register_context(DltContextData *log);
static DltReturnValue mct_user_log_send_unregister_context(DltContextData *log);
static DltReturnValue mct_send_app_ll_ts_limit(const char *apid,
                                               DltLogLevelType loglevel,
                                               DltTraceStatusType tracestatus);
static DltReturnValue mct_user_log_send_marker();
static DltReturnValue mct_user_print_msg(DltMessage *msg, DltContextData *log);
static DltReturnValue mct_user_log_check_user_message(void);
static void mct_user_log_reattach_to_daemon(void);
static DltReturnValue mct_user_log_send_overflow(void);
static DltReturnValue mct_user_set_blockmode(int8_t mode);
static int mct_user_get_blockmode();
static DltReturnValue mct_user_log_out_error_handling(void *ptr1,
                                                      size_t len1,
                                                      void *ptr2,
                                                      size_t len2,
                                                      void *ptr3,
                                                      size_t len3);
static void mct_user_cleanup_handler(void *arg);
static int mct_start_threads(int id);
static void mct_stop_threads();
static void mct_fork_child_fork_handler();

static DltReturnValue mct_user_log_write_string_utils_attr(DltContextData *log, const char *text, const enum StringType type, const char *name, bool with_var_info);
static DltReturnValue mct_user_log_write_sized_string_utils_attr(DltContextData *log, const char *text, uint16_t length, const enum StringType type, const char *name, bool with_var_info);


static DltReturnValue mct_unregister_app_util(bool force_sending_messages);

DltReturnValue mct_user_check_library_version(const char *user_major_version,
                                              const char *user_minor_version)
{
    char lib_major_version[DLT_USER_MAX_LIB_VERSION_LENGTH];
    char lib_minor_version[DLT_USER_MAX_LIB_VERSION_LENGTH];

    mct_get_major_version(lib_major_version, DLT_USER_MAX_LIB_VERSION_LENGTH);
    mct_get_minor_version(lib_minor_version, DLT_USER_MAX_LIB_VERSION_LENGTH);

    if ((strcmp(lib_major_version,
                user_major_version) != 0) ||
        (strcmp(lib_minor_version, user_minor_version) != 0)) {
        mct_vnlog(
            LOG_WARNING,
            DLT_USER_BUFFER_LENGTH,
            "DLT Library version check failed! Installed DLT library version is %s.%s - Application using DLT library version %s.%s\n",
            lib_major_version,
            lib_minor_version,
            user_major_version,
            user_minor_version);

        return DLT_RETURN_ERROR;
    }

    return DLT_RETURN_OK;
}

#if defined DLT_LIB_USE_UNIX_SOCKET_IPC
static DltReturnValue mct_socket_set_nonblock_and_linger(int sockfd)
{
    int status;
    struct linger l_opt;

    status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    if (status == -1) {
        mct_log(LOG_INFO, "Socket cannot be changed to NON BLOCK\n");
        return DLT_RETURN_ERROR;
    }

    l_opt.l_onoff = 1;
    l_opt.l_linger = 10;

    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &l_opt, sizeof l_opt) < 0) {
        mct_log(LOG_WARNING, "Failed to set socket linger option\n");
    }

    return DLT_RETURN_OK;
}
#endif

#ifdef DLT_LIB_USE_UNIX_SOCKET_IPC
static DltReturnValue mct_initialize_socket_connection(void)
{
    struct sockaddr_un remote;
    char mctSockBaseDir[DLT_IPC_PATH_MAX];

    DLT_SEM_LOCK();
    int sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (sockfd == DLT_FD_INIT) {
        mct_log(LOG_CRIT, "Failed to create socket\n");
        DLT_SEM_FREE();
        return DLT_RETURN_ERROR;
    }

    mct_user.mct_log_handle = sockfd;

    /* Change socket mode */
    if (mct_set_socket_mode(mct_user.socket_mode) == DLT_RETURN_ERROR) {
        DLT_SEM_FREE();
        return DLT_RETURN_ERROR;
    }

    if (mct_socket_set_nonblock_and_linger(sockfd) != DLT_RETURN_OK) {
        close(sockfd);
        DLT_SEM_FREE();
        return DLT_RETURN_ERROR;
    }

    remote.sun_family = AF_UNIX;
    snprintf(mctSockBaseDir, DLT_IPC_PATH_MAX, "%s/mct", DLT_USER_IPC_PATH);
    strncpy(remote.sun_path, mctSockBaseDir, sizeof(remote.sun_path));

    if (strlen(DLT_USER_IPC_PATH) > DLT_IPC_PATH_MAX) {
        mct_vlog(LOG_INFO,
                 "Provided path too long...trimming it to path[%s]\n",
                 mctSockBaseDir);
    }

    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
        if (mct_user.connection_state != DLT_USER_RETRY_CONNECT) {
            mct_vlog(LOG_INFO,
                     "Socket %s cannot be opened (errno=%d). Retrying later...\n",
                     mctSockBaseDir, errno);
            mct_user.connection_state = DLT_USER_RETRY_CONNECT;
        }

        close(sockfd);
        mct_user.mct_log_handle = -1;
    } else {
        mct_user.mct_log_handle = sockfd;
        mct_user.connection_state = DLT_USER_CONNECTED;

        if (mct_receiver_init(&(mct_user.receiver),
                              sockfd,
                              DLT_RECEIVE_SOCKET,
                              DLT_USER_RCVBUF_MAX_SIZE) == DLT_RETURN_ERROR) {
            mct_user_initialised = false;
            close(sockfd);
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }
    }

    DLT_SEM_FREE();

    return DLT_RETURN_OK;
}

#elif defined DLT_LIB_USE_VSOCK_IPC
static DltReturnValue mct_initialize_vsock_connection()
{
    struct sockaddr_vm remote;

    DLT_SEM_LOCK();
    int sockfd = socket(AF_VSOCK, SOCK_STREAM, 0);

    if (sockfd == DLT_FD_INIT) {
        mct_log(LOG_CRIT, "Failed to create VSOCK socket\n");
        DLT_SEM_FREE();
        return DLT_RETURN_ERROR;
    }

    memset(&remote, 0, sizeof(remote));
    remote.svm_family = AF_VSOCK;
    remote.svm_port = DLT_VSOCK_PORT;
    remote.svm_cid = VMADDR_CID_HOST;

    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
        if (mct_user.connection_state != DLT_USER_RETRY_CONNECT) {
            mct_vlog(LOG_INFO, "VSOCK socket cannot be opened. Retrying later...\n");
            mct_user.connection_state = DLT_USER_RETRY_CONNECT;
        }

        close(sockfd);
        mct_user.mct_log_handle = -1;
    } else {
        /* Set to non-blocking after connect() to avoid EINPROGRESS. DltUserConntextionState
         * needs "connecting" state if connect() should be non-blocking. */
        if (mct_socket_set_nonblock_and_linger(sockfd) != DLT_RETURN_OK) {
            close(sockfd);
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }

        mct_user.mct_log_handle = sockfd;
        mct_user.connection_state = DLT_USER_CONNECTED;

        if (mct_receiver_init(&(mct_user.receiver),
                              sockfd,
                              DLT_RECEIVE_SOCKET,
                              DLT_USER_RCVBUF_MAX_SIZE) == DLT_RETURN_ERROR) {
            mct_user_initialised = false;
            close(sockfd);
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }
    }

    DLT_SEM_FREE();

    return DLT_RETURN_OK;
}
#else /* DLT_LIB_USE_FIFO_IPC */
static DltReturnValue mct_initialize_fifo_connection(void)
{
    char filename[DLT_PATH_MAX];
    int ret;

    snprintf(mct_user_dir, DLT_PATH_MAX, "%s/mctpipes", mctFifoBaseDir);
    snprintf(mct_daemon_fifo, DLT_PATH_MAX, "%s/mct", mctFifoBaseDir);
    ret = mkdir(mct_user_dir,
                S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_ISVTX);

    if ((ret == -1) && (errno != EEXIST)) {
        mct_vnlog(LOG_ERR,
                  DLT_USER_BUFFER_LENGTH,
                  "FIFO user dir %s cannot be created!\n",
                  mct_user_dir);
        return DLT_RETURN_ERROR;
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
                      DLT_USER_BUFFER_LENGTH,
                      "FIFO user dir %s cannot be chmoded!\n",
                      mct_user_dir);
            return DLT_RETURN_ERROR;
        }
    }

    /* create and open DLT user FIFO */
    snprintf(filename, DLT_PATH_MAX, "%s/mct%d", mct_user_dir, getpid());

    /* Try to delete existing pipe, ignore result of unlink */
    unlink(filename);

    ret = mkfifo(filename, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

    if (ret == -1) {
        mct_vnlog(LOG_WARNING,
                  DLT_USER_BUFFER_LENGTH,
                  "Loging disabled, FIFO user %s cannot be created!\n",
                  filename);
    }

    /* S_IWGRP cannot be set by mkfifo (???), let's reassign right bits */
    ret = chmod(filename, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);

    if (ret == -1) {
        mct_vnlog(LOG_WARNING,
                  DLT_USER_BUFFER_LENGTH,
                  "FIFO user %s cannot be chmoded!\n",
                  mct_user_dir);
        return DLT_RETURN_ERROR;
    }

    mct_user.mct_user_handle = open(filename, O_RDWR | O_NONBLOCK | O_CLOEXEC);

    if (mct_user.mct_user_handle == DLT_FD_INIT) {
        mct_vnlog(LOG_WARNING,
                  DLT_USER_BUFFER_LENGTH,
                  "Logging disabled, FIFO user %s cannot be opened!\n",
                  filename);
        unlink(filename);
        return DLT_RETURN_OK;
    }

    /* open DLT output FIFO */
    mct_user.mct_log_handle = open(mct_daemon_fifo, O_WRONLY | O_NONBLOCK | O_CLOEXEC);

    if (mct_user.mct_log_handle == -1) {
        /* This is a normal usecase. It is OK that the daemon (and thus the FIFO /tmp/mct)
         * starts later and some DLT users have already been started before.
         * Thus it is OK if the FIFO can't be opened. */
        mct_vnlog(LOG_INFO, DLT_USER_BUFFER_LENGTH, "FIFO %s cannot be opened. Retrying later...\n",
                  mct_daemon_fifo);
    }

    return DLT_RETURN_OK;
}
#endif

DltReturnValue mct_set_socket_mode(DltUserSocketMode mode)
{
    int ret = DLT_RETURN_OK;

#ifdef DLT_USE_UNIX_SOCKET_IPC
    int flags = 0;
    int result = 0;
    char mctSockBaseDir[DLT_IPC_PATH_MAX];

    if (mct_user.mct_log_handle != -1) {
        flags = fcntl(mct_user.mct_log_handle, F_GETFL);

        if (mode == DLT_USER_SOCKET_BLOCKING) {
            /* Set socket to Blocking mode */
            mct_vlog(LOG_DEBUG, "%s: Set socket to Blocking mode\n", __func__);
            result = fcntl(mct_user.mct_log_handle, F_SETFL, flags & ~O_NONBLOCK);
            mct_user.socket_mode = DLT_USER_SOCKET_BLOCKING;
        } else {
            /* Set socket to Non-blocking mode */
            mct_vlog(LOG_DEBUG, "%s: Set socket to Non-blocking mode\n", __func__);
            result = fcntl(mct_user.mct_log_handle, F_SETFL, flags | O_NONBLOCK);
            mct_user.socket_mode = DLT_USER_SOCKET_NON_BLOCKING;
        }

        snprintf(mctSockBaseDir, DLT_IPC_PATH_MAX, "%s/mct", DLT_USER_IPC_PATH);

        if (result < 0) {
            mct_vlog(LOG_WARNING,
                     "%s: Failed to change mode of socket %s: %s\n",
                     __func__, mctSockBaseDir, strerror(errno));
            ret = DLT_RETURN_ERROR;
        }
    }

#else
    /* Satisfy Compiler for warnings */
    (void)mode;
#endif

    return ret;
}

DltReturnValue mct_init(void)
{
    /* Compare 'mct_user_initialised' to false. If equal, 'mct_user_initialised' will be set to true.
    Calls retruns true, if 'mct_user_initialised' was false.
    That way it's no problem, if two threads enter this function, because only the very first one will
    pass fully. The other one will immediately return, because when it executes the atomic function
    'mct_user_initialised' will be for sure already set to true.
    */
    bool expected = false;
    if (!(atomic_compare_exchange_strong(&mct_user_initialised, &expected, true)))
        return DLT_RETURN_OK;

    /* check environment variables */
    mct_check_envvar();

    /* Check logging mode and internal log file is opened or not*/
    if(logging_mode == DLT_LOG_TO_FILE && logging_handle == NULL)
    {
        mct_log_init(logging_mode);
    }

    /* process is exiting. Do not allocate new resources. */
    if (mct_user_freeing != 0) {
        mct_vlog(LOG_INFO, "%s logging disabled, process is exiting", __func__);
        /* return negative value, to stop the current log */
        return DLT_RETURN_LOGGING_DISABLED;
    }

    mct_user_app_unreg = false;

    /* Initialize common part of mct_init()/mct_init_file() */
    if (mct_init_common() == DLT_RETURN_ERROR) {
        mct_user_initialised = false;
        return DLT_RETURN_ERROR;
    }

    mct_user.mct_is_file = 0;
    mct_user.filesize_max = UINT_MAX;
    mct_user_file_reach_max = false;

    mct_user.overflow = 0;
    mct_user.overflow_counter = 0;

#ifdef DLT_LIB_USE_UNIX_SOCKET_IPC

    if (mct_initialize_socket_connection() != DLT_RETURN_OK) {
        /* We could connect to the pipe, but not to the socket, which is normally */
        /* open before by the DLT daemon => bad failure => return error code */
        /* in case application is started before daemon, it is expected behaviour */
        return DLT_RETURN_ERROR;
    }

#elif defined DLT_LIB_USE_VSOCK_IPC

    if (mct_initialize_vsock_connection() != DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

#else /* DLT_LIB_USE_FIFO_IPC */

    if (mct_initialize_fifo_connection() != DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    if (mct_receiver_init(&(mct_user.receiver),
                          mct_user.mct_user_handle,
                          DLT_RECEIVE_FD,
                          DLT_USER_RCVBUF_MAX_SIZE) == DLT_RETURN_ERROR) {
        mct_user_initialised = false;
        return DLT_RETURN_ERROR;
    }

#endif

    if (mct_start_threads(DLT_USER_HOUSEKEEPER_THREAD) < 0) {
        mct_user_initialised = false;
        return DLT_RETURN_ERROR;
    }

    /* prepare for fork() call */
    pthread_atfork(NULL, NULL, &mct_fork_child_fork_handler);

    return DLT_RETURN_OK;
}

DltReturnValue mct_get_appid(char *appid)
{
    if (appid != NULL) {
        strncpy(appid, mct_user.appID, 4);
        return DLT_RETURN_OK;
    } else {
        mct_log(LOG_ERR, "Invalid parameter.\n");
        return DLT_RETURN_WRONG_PARAMETER;
    }
}

DltReturnValue mct_init_file(const char *name)
{
    /* check null pointer */
    if (!name) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* Compare 'mct_user_initialised' to false. If equal, 'mct_user_initialised' will be set to true.
    Calls retruns true, if 'mct_user_initialised' was false.
    That way it's no problem, if two threads enter this function, because only the very first one will
    pass fully. The other one will immediately return, because when it executes the atomic function
    'mct_user_initialised' will be for sure already set to true.
    */
    bool expected = false;
    if (!(atomic_compare_exchange_strong(&mct_user_initialised, &expected, true)))
        return DLT_RETURN_OK;

    /* Initialize common part of mct_init()/mct_init_file() */
    if (mct_init_common() == DLT_RETURN_ERROR) {
        mct_user_initialised = false;
        return DLT_RETURN_ERROR;
    }

    mct_user.mct_is_file = 1;

    /* open DLT output file */
    mct_user.mct_log_handle = open(name, O_WRONLY | O_CREAT,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); /* mode: wb */

    if (mct_user.mct_log_handle == -1) {
        mct_vnlog(LOG_ERR, DLT_USER_BUFFER_LENGTH, "Log file %s cannot be opened!\n", name);
        mct_user.mct_is_file = 0;
        return DLT_RETURN_ERROR;
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_set_filesize_max(unsigned int filesize)
{
    if (mct_user.mct_is_file == 0)
    {
        mct_vlog(LOG_ERR, "%s: Library is not configured to log to file\n",
                 __func__);
        return DLT_RETURN_ERROR;
    }

    if (filesize == 0) {
        mct_user.filesize_max = UINT_MAX;
    }
    else {
        mct_user.filesize_max = filesize;
    }
    mct_vlog(LOG_DEBUG, "%s: Defined filesize_max is [%d]\n", __func__,
             mct_user.filesize_max);

    return DLT_RETURN_OK;
}

/* Return true if verbose mode is to be used for this DltContextData */
static inline bool is_verbose_mode(int8_t mctuser_verbose_mode, const DltContextData* log)
{
    return (mctuser_verbose_mode == 1) || (log != NULL && log->verbose_mode);
}

DltReturnValue mct_init_common(void)
{
    char *env_local_print;
    char *env_initial_log_level;
    char *env_buffer_min;
    uint32_t buffer_min = DLT_USER_RINGBUFFER_MIN_SIZE;
    char *env_buffer_max;
    uint32_t buffer_max = DLT_USER_RINGBUFFER_MAX_SIZE;
    char *env_buffer_step;
    uint32_t buffer_step = DLT_USER_RINGBUFFER_STEP_SIZE;
    char *env_force_block;
    char *env_disable_extended_header_for_nonverbose;
    char *env_log_buffer_len;
    uint32_t buffer_max_configured = 0;
    uint32_t header_size = 0;

    /* Binary semaphore for threads */
    if (sem_init(&mct_mutex, 0, 1) == -1) {
        mct_user_initialised = false;
        return DLT_RETURN_ERROR;
    }

    /* set to unknown state of connected client */
    mct_user.log_state = -1;

    mct_user.mct_log_handle = -1;
    mct_user.mct_user_handle = DLT_FD_INIT;

    mct_set_id(mct_user.ecuID, DLT_USER_DEFAULT_ECU_ID);
    mct_set_id(mct_user.appID, "");

    mct_user.application_description = NULL;

    /* Verbose mode is enabled by default */
    mct_user.verbose_mode = 1;

    /* header_size is used for resend buffer
     * so it won't include DltStorageHeader
     */
    header_size = sizeof(DltUserHeader) + sizeof(DltStandardHeader) +
        sizeof(DltStandardHeaderExtra);

    /* Use extended header for non verbose is enabled by default */
    mct_user.use_extended_header_for_non_verbose =
        DLT_USER_USE_EXTENDED_HEADER_FOR_NONVERBOSE;

    /* Use extended header for non verbose is modified as per environment variable */
    env_disable_extended_header_for_nonverbose =
        getenv(DLT_USER_ENV_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE);

    if (env_disable_extended_header_for_nonverbose) {
        if (strcmp(env_disable_extended_header_for_nonverbose, "1") == 0) {
            mct_user.use_extended_header_for_non_verbose =
                DLT_USER_NO_USE_EXTENDED_HEADER_FOR_NONVERBOSE;
        }
    }

    if (mct_user.use_extended_header_for_non_verbose ==
        DLT_USER_USE_EXTENDED_HEADER_FOR_NONVERBOSE) {
        header_size += sizeof(DltExtendedHeader);
    }

    /* With session id is enabled by default */
    mct_user.with_session_id = DLT_USER_WITH_SESSION_ID;

    /* With timestamp is enabled by default */
    mct_user.with_timestamp = DLT_USER_WITH_TIMESTAMP;

    /* With timestamp is enabled by default */
    mct_user.with_ecu_id = DLT_USER_WITH_ECU_ID;

    /* Local print is disabled by default */
    mct_user.enable_local_print = 0;

    mct_user.local_print_mode = DLT_PM_UNSET;

    mct_user.timeout_at_exit_handler = DLT_USER_ATEXIT_RESEND_BUFFER_EXIT_TIMEOUT;

    env_local_print = getenv(DLT_USER_ENV_LOCAL_PRINT_MODE);

    if (env_local_print) {
        if (strcmp(env_local_print, "AUTOMATIC") == 0) {
            mct_user.local_print_mode = DLT_PM_AUTOMATIC;
        } else if (strcmp(env_local_print, "FORCE_ON") == 0) {
            mct_user.local_print_mode = DLT_PM_FORCE_ON;
        } else if (strcmp(env_local_print, "FORCE_OFF") == 0) {
            mct_user.local_print_mode = DLT_PM_FORCE_OFF;
        }
    }

    env_initial_log_level = getenv("DLT_INITIAL_LOG_LEVEL");

    if (env_initial_log_level != NULL) {
        if (mct_env_extract_ll_set(&env_initial_log_level, &mct_user.initial_ll_set) != 0) {
            mct_vlog(LOG_WARNING,
                     "Unable to parse initial set of log-levels from environment! Env:\n%s\n",
                     getenv("DLT_INITIAL_LOG_LEVEL"));
        }
    }

    /* Check for force block mode environment variable */
    env_force_block = getenv(DLT_USER_ENV_FORCE_BLOCK_MODE);

    /* Initialize LogLevel/TraceStatus field */
    DLT_SEM_LOCK();
    mct_user.mct_ll_ts = NULL;
    mct_user.mct_ll_ts_max_num_entries = 0;
    mct_user.mct_ll_ts_num_entries = 0;

    /* set block mode */
    if (env_force_block != NULL) {
        mct_user.block_mode = DLT_MODE_BLOCKING;
        mct_user.force_blocking = DLT_MODE_BLOCKING;
    }

    env_buffer_min = getenv(DLT_USER_ENV_BUFFER_MIN_SIZE);
    env_buffer_max = getenv(DLT_USER_ENV_BUFFER_MAX_SIZE);
    env_buffer_step = getenv(DLT_USER_ENV_BUFFER_STEP_SIZE);

    if (env_buffer_min != NULL) {
        buffer_min = (uint32_t)strtol(env_buffer_min, NULL, 10);

        if ((errno == EINVAL) || (errno == ERANGE)) {
            mct_vlog(LOG_ERR,
                     "Wrong value specified for %s. Using default: %d\n",
                     DLT_USER_ENV_BUFFER_MIN_SIZE,
                     DLT_USER_RINGBUFFER_MIN_SIZE);
            buffer_min = DLT_USER_RINGBUFFER_MIN_SIZE;
        }
    }

    if (env_buffer_max != NULL) {
        buffer_max = (uint32_t)strtol(env_buffer_max, NULL, 10);

        if ((errno == EINVAL) || (errno == ERANGE)) {
            mct_vlog(LOG_ERR,
                     "Wrong value specified for %s. Using default: %d\n",
                     DLT_USER_ENV_BUFFER_MAX_SIZE,
                     DLT_USER_RINGBUFFER_MAX_SIZE);
            buffer_max = DLT_USER_RINGBUFFER_MAX_SIZE;
        }
    }

    if (env_buffer_step != NULL) {
        buffer_step = (uint32_t)strtol(env_buffer_step, NULL, 10);

        if ((errno == EINVAL) || (errno == ERANGE)) {
            mct_vlog(LOG_ERR,
                     "Wrong value specified for %s. Using default: %d\n",
                     DLT_USER_ENV_BUFFER_STEP_SIZE,
                     DLT_USER_RINGBUFFER_STEP_SIZE);
            buffer_step = DLT_USER_RINGBUFFER_STEP_SIZE;
        }
    }

    /* init log buffer size */
    mct_user.log_buf_len = DLT_USER_BUF_MAX_SIZE;
    env_log_buffer_len = getenv(DLT_USER_ENV_LOG_MSG_BUF_LEN);

    if (env_log_buffer_len != NULL) {
        buffer_max_configured = (uint32_t)strtol(env_log_buffer_len, NULL, 10);

        if (buffer_max_configured > DLT_LOG_MSG_BUF_MAX_SIZE) {
            mct_user.log_buf_len = DLT_LOG_MSG_BUF_MAX_SIZE;
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
            DLT_SEM_FREE();
            mct_vlog(LOG_ERR, "cannot allocate memory for resend buffer\n");
            return DLT_RETURN_ERROR;
        }
    }

    mct_user.disable_injection_msg = 0;

    if (getenv(DLT_USER_ENV_DISABLE_INJECTION_MSG)) {
        mct_log(LOG_WARNING, "Injection message is disabled\n");
        mct_user.disable_injection_msg = 1;
    }

    if (mct_buffer_init_dynamic(&(mct_user.startup_buffer),
                                buffer_min,
                                buffer_max,
                                buffer_step) == DLT_RETURN_ERROR) {
        mct_user_initialised = false;
        DLT_SEM_FREE();
        return DLT_RETURN_ERROR;
    }

    DLT_SEM_FREE();

    signal(SIGPIPE, SIG_IGN);                  /* ignore pipe signals */

    if (atexit_registered == 0) {
        atexit_registered = 1;
        atexit(mct_user_atexit_handler);
    }

#ifdef DLT_TEST_ENABLE
    mct_user.corrupt_user_header = 0;
    mct_user.corrupt_message_size = 0;
    mct_user.corrupt_message_size_size = 0;
#endif

    return DLT_RETURN_OK;
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
    DLT_SEM_LOCK();
    count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
    DLT_SEM_FREE();

    if ((count > 0) && (mct_user.timeout_at_exit_handler > 0)) {
        while (mct_uptime() < exitTime) {
            if (mct_user.mct_log_handle == -1) {
                /* Reattach to daemon if neccesary */
                mct_user_log_reattach_to_daemon();

                if ((mct_user.mct_log_handle != -1) && (mct_user.overflow_counter)) {
                    if (mct_user_log_send_overflow() == 0) {
                        mct_vnlog(LOG_WARNING,
                                  DLT_USER_BUFFER_LENGTH,
                                  "%u messages discarded!\n",
                                  mct_user.overflow_counter);
                        mct_user.overflow_counter = 0;
                    }
                }
            }

            if (mct_user.mct_log_handle != -1) {
                ret = mct_user_log_resend_buffer();

                if (ret == 0) {
                    DLT_SEM_LOCK();
                    count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
                    DLT_SEM_FREE();

                    return count;
                }
            }

            ts.tv_sec = 0;
            ts.tv_nsec = DLT_USER_ATEXIT_RESEND_BUFFER_SLEEP;
            nanosleep(&ts, NULL);
        }

        DLT_SEM_LOCK();
        count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
        DLT_SEM_FREE();
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

DltReturnValue mct_free(void)
{
    uint32_t i;
    int ret = 0;
#ifdef DLT_LIB_USE_FIFO_IPC
    char filename[DLT_PATH_MAX];
#endif

    if (mct_user_freeing != 0) {
        /* resources are already being freed. Do nothing and return. */
        return DLT_RETURN_ERROR;
    }

    /* library is freeing its resources. Avoid to allocate it in mct_init() */
    mct_user_freeing = 1;

    if (!mct_user_initialised) {
        mct_user_freeing = 0;
        return DLT_RETURN_ERROR;
    }

    mct_user_initialised = false;

    mct_stop_threads();

#ifdef DLT_LIB_USE_FIFO_IPC

    if (mct_user.mct_user_handle != DLT_FD_INIT) {
        close(mct_user.mct_user_handle);
        mct_user.mct_user_handle = DLT_FD_INIT;
        snprintf(filename, DLT_PATH_MAX, "%s/mct%d", mct_user_dir, getpid());
        unlink(filename);
    }

#endif

    if (mct_user.mct_log_handle != -1) {
        /* close log file/output fifo to daemon */
#if defined DLT_LIB_USE_UNIX_SOCKET_IPC
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
                ret = poll(nfd, 1, DLT_USER_RECEIVE_MDELAY);

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
    DLT_SEM_LOCK();
    mct_receiver_free(&(mct_user.receiver));
    DLT_SEM_FREE();

    /* Ignore return value */
    DLT_SEM_LOCK();

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
#ifdef DLT_HP_LOG_ENABLE

            if (mct_user.mct_ll_ts[i].DltExtBuff_ptr != 0) {
                free(mct_user.mct_ll_ts[i].DltExtBuff_ptr);
                mct_user.mct_ll_ts[i].DltExtBuff_ptr = 0;
            }

#endif
        }

        free(mct_user.mct_ll_ts);
        mct_user.mct_ll_ts = NULL;
        mct_user.mct_ll_ts_max_num_entries = 0;
        mct_user.mct_ll_ts_num_entries = 0;
    }

    mct_env_free_ll_set(&mct_user.initial_ll_set);
    DLT_SEM_FREE();

    sem_destroy(&mct_mutex);

    /* allow the user app to do mct_init() again. */
    /* The flag is unset only to keep almost the same behaviour as before, on EntryNav */
    /* This should be removed for other projects (see documentation of mct_free() */
    mct_user_freeing = 0;

    return DLT_RETURN_OK;
}

DltReturnValue mct_check_library_version(const char *user_major_version,
                                         const char *user_minor_version)
{
    return mct_user_check_library_version(user_major_version, user_minor_version);
}

DltReturnValue mct_register_app(const char *apid, const char *description)
{
    DltReturnValue ret = DLT_RETURN_OK;

    /* pointer points to AppID environment variable */
    const char *env_app_id = NULL;
    /* pointer points to the AppID */
    const char *p_app_id = NULL;

    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* the AppID  may be specified by an environment variable */
    env_app_id = getenv(DLT_USER_ENV_APP_ID);

    if (env_app_id != NULL) {
        /* always use AppID if it is specified by an environment variable */
        p_app_id = env_app_id;
    } else {
        /* Environment variable is not specified, use value which is passed by function parameter */
        p_app_id = apid;
    }

    if ((p_app_id == NULL) || (p_app_id[0] == '\0')) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* check if application already registered */
    /* if yes do not register again */
    if (p_app_id[1] == 0) {
        if (p_app_id[0] == mct_user.appID[0]) {
            return DLT_RETURN_OK;
        }
    } else if (p_app_id[2] == 0) {
        if ((p_app_id[0] == mct_user.appID[0]) &&
            (p_app_id[1] == mct_user.appID[1])) {
            return DLT_RETURN_OK;
        }
    } else if (p_app_id[3] == 0) {
        if ((p_app_id[0] == mct_user.appID[0]) &&
            (p_app_id[1] == mct_user.appID[1]) &&
            (p_app_id[2] == mct_user.appID[2])) {
            return DLT_RETURN_OK;
        }
    } else {
        if ((p_app_id[0] == mct_user.appID[0]) &&
            (p_app_id[1] == mct_user.appID[1]) &&
            (p_app_id[2] == mct_user.appID[2]) &&
            (p_app_id[3] == mct_user.appID[3])) {
            return DLT_RETURN_OK;
        }
    }

    DLT_SEM_LOCK();

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
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }
    }

    DLT_SEM_FREE();

    ret = mct_user_log_send_register_application();

    if ((ret == DLT_RETURN_OK) && (mct_user.mct_log_handle != -1)) {
        ret = mct_user_log_resend_buffer();
    }

    return ret;
}

DltReturnValue mct_register_context(DltContext *handle,
                                    const char *contextid,
                                    const char *description)
{
    /* check nullpointer */
    if (handle == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    if ((contextid == NULL) || (contextid[0] == '\0')) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    return mct_register_context_ll_ts(handle,
                                      contextid,
                                      description,
                                      DLT_USER_LOG_LEVEL_NOT_SET,
                                      DLT_USER_TRACE_STATUS_NOT_SET);
}

DltReturnValue mct_register_context_ll_ts_llccb(DltContext *handle,
                                                const char *contextid,
                                                const char *description,
                                                int loglevel,
                                                int tracestatus,
                                                void (*mct_log_level_changed_callback)(
                                                    char context_id[DLT_ID_SIZE],
                                                    uint8_t
                                                    log_level,
                                                    uint8_t
                                                    trace_status))
{
    DltContextData log;
    uint32_t i;
    int envLogLevel = DLT_USER_LOG_LEVEL_NOT_SET;

    /*check nullpointer */
    if ((handle == NULL) || (contextid == NULL) || (contextid[0] == '\0')) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if ((loglevel < DLT_USER_LOG_LEVEL_NOT_SET) || (loglevel >= DLT_LOG_MAX)) {
        mct_vlog(LOG_ERR, "Loglevel %d is outside valid range", loglevel);
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if ((tracestatus < DLT_USER_TRACE_STATUS_NOT_SET) || (tracestatus >= DLT_TRACE_STATUS_MAX)) {
        mct_vlog(LOG_ERR, "Tracestatus %d is outside valid range", tracestatus);
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (mct_user_log_init(handle, &log) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    /* Reset message counter */
    handle->mcnt = 0;

    /* Store context id in log level/trace status field */

    /* Check if already registered, else register context */
    DLT_SEM_LOCK();

    /* Check of double context registration removed */
    /* Double registration is already checked by daemon */

    /* Allocate or expand context array */
    if (mct_user.mct_ll_ts == NULL) {
        mct_user.mct_ll_ts = (mct_ll_ts_type *)malloc(
                sizeof(mct_ll_ts_type) * DLT_USER_CONTEXT_ALLOC_SIZE);

        if (mct_user.mct_ll_ts == NULL) {
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }

        mct_user.mct_ll_ts_max_num_entries = DLT_USER_CONTEXT_ALLOC_SIZE;

        /* Initialize new entries */
        for (i = 0; i < mct_user.mct_ll_ts_max_num_entries; i++) {
            mct_set_id(mct_user.mct_ll_ts[i].contextID, "");

            /* At startup, logging and tracing is locally enabled */
            /* the correct log level/status is set after received from daemon */
            mct_user.mct_ll_ts[i].log_level = DLT_USER_INITIAL_LOG_LEVEL;
            mct_user.mct_ll_ts[i].trace_status = DLT_USER_INITIAL_TRACE_STATUS;

            mct_user.mct_ll_ts[i].log_level_ptr = 0;
            mct_user.mct_ll_ts[i].trace_status_ptr = 0;

            mct_user.mct_ll_ts[i].context_description = 0;

            mct_user.mct_ll_ts[i].injection_table = 0;
            mct_user.mct_ll_ts[i].nrcallbacks = 0;
            mct_user.mct_ll_ts[i].log_level_changed_callback = 0;
#ifdef DLT_HP_LOG_ENABLE
            mct_user.mct_ll_ts[i].DltExtBuff_ptr = 0;
#endif
        }
    } else if ((mct_user.mct_ll_ts_num_entries % DLT_USER_CONTEXT_ALLOC_SIZE) == 0) {
        /* allocate memory in steps of DLT_USER_CONTEXT_ALLOC_SIZE, e.g. 500 */
        mct_ll_ts_type *old_ll_ts;
        uint32_t old_max_entries;

        old_ll_ts = mct_user.mct_ll_ts;
        old_max_entries = mct_user.mct_ll_ts_max_num_entries;

        mct_user.mct_ll_ts_max_num_entries = ((mct_user.mct_ll_ts_num_entries
                                               / DLT_USER_CONTEXT_ALLOC_SIZE) + 1)
            * DLT_USER_CONTEXT_ALLOC_SIZE;
        mct_user.mct_ll_ts = (mct_ll_ts_type *)malloc(sizeof(mct_ll_ts_type) *
                                                      mct_user.mct_ll_ts_max_num_entries);

        if (mct_user.mct_ll_ts == NULL) {
            mct_user.mct_ll_ts = old_ll_ts;
            mct_user.mct_ll_ts_max_num_entries = old_max_entries;
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
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
            mct_user.mct_ll_ts[i].log_level = DLT_USER_INITIAL_LOG_LEVEL;
            mct_user.mct_ll_ts[i].trace_status = DLT_USER_INITIAL_TRACE_STATUS;

            mct_user.mct_ll_ts[i].log_level_ptr = 0;
            mct_user.mct_ll_ts[i].trace_status_ptr = 0;

            mct_user.mct_ll_ts[i].context_description = 0;

            mct_user.mct_ll_ts[i].injection_table = 0;
            mct_user.mct_ll_ts[i].nrcallbacks = 0;
            mct_user.mct_ll_ts[i].log_level_changed_callback = 0;
#ifdef DLT_HP_LOG_ENABLE
            mct_user.mct_ll_ts[i].DltExtBuff_ptr = 0;
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
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }

        strncpy(ctx_entry->context_description, description, desc_len + 1);
    }

    if (ctx_entry->log_level_ptr == 0) {
        ctx_entry->log_level_ptr = malloc(sizeof(int8_t));

        if (ctx_entry->log_level_ptr == 0) {
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }
    }

    if (ctx_entry->trace_status_ptr == 0) {
        ctx_entry->trace_status_ptr = malloc(sizeof(int8_t));

        if (ctx_entry->trace_status_ptr == 0) {
            DLT_SEM_FREE();
            return DLT_RETURN_ERROR;
        }
    }

    /* check if the log level is set in the environement */
    envLogLevel = mct_env_adjust_ll_from_env(&mct_user.initial_ll_set,
                                             mct_user.appID,
                                             contextid,
                                             DLT_USER_LOG_LEVEL_NOT_SET);

    if (envLogLevel != DLT_USER_LOG_LEVEL_NOT_SET) {
        ctx_entry->log_level = envLogLevel;
        loglevel = envLogLevel;
    } else if (loglevel != DLT_USER_LOG_LEVEL_NOT_SET) {
        ctx_entry->log_level = loglevel;
    }

    if (tracestatus != DLT_USER_TRACE_STATUS_NOT_SET) {
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

    DLT_SEM_FREE();

    return mct_user_log_send_register_context(&log);
}

DltReturnValue mct_register_context_ll_ts(DltContext *handle,
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

DltReturnValue mct_register_context_llccb(DltContext *handle,
                                          const char *contextid,
                                          const char *description,
                                          void (*mct_log_level_changed_callback)(
                                              char context_id[DLT_ID_SIZE],
                                              uint8_t log_level,
                                              uint8_t
                                              trace_status))
{
    if ((handle == NULL) || (contextid == NULL) || (contextid[0] == '\0')) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    return mct_register_context_ll_ts_llccb(handle,
                                            contextid,
                                            description,
                                            DLT_USER_LOG_LEVEL_NOT_SET,
                                            DLT_USER_TRACE_STATUS_NOT_SET,
                                            mct_log_level_changed_callback);
}

/* If force_sending_messages is set to true, do not clean appIDs when there are
 * still data in startup_buffer. atexit_handler will free the appIDs */
DltReturnValue mct_unregister_app_util(bool force_sending_messages)
{
    DltReturnValue ret = DLT_RETURN_OK;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
    }

    /* Do not allow unregister_context() to be called after this */
    mct_user_app_unreg = true;

    /* Inform daemon to unregister application and all of its contexts */
    ret = mct_user_log_send_unregister_application();

    DLT_SEM_LOCK();

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

    DLT_SEM_FREE();

    return ret;
}

DltReturnValue mct_unregister_app(void)
{
    return mct_unregister_app_util(false);
}

DltReturnValue mct_unregister_app_flush_buffered_logs(void)
{
    DltReturnValue ret = DLT_RETURN_OK;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_ERR, "%s mct_user_initialised false\n", __func__);
        return DLT_RETURN_ERROR;
    }

    if (mct_user.mct_log_handle != -1) {
        do {
            ret = mct_user_log_resend_buffer();
        } while ((ret != DLT_RETURN_OK) && (mct_user.mct_log_handle != -1));
    }

    return mct_unregister_app_util(true);
}

DltReturnValue mct_unregister_context(DltContext *handle)
{
    DltContextData log;
    DltReturnValue ret = DLT_RETURN_OK;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if (mct_user_app_unreg) {
        mct_vlog(LOG_WARNING,
                 "%s: Contexts and application are already unregistered\n",
                 __func__);
        return DLT_RETURN_ERROR;
    }

    log.handle = NULL;
    log.context_description = NULL;

    if (mct_user_log_init(handle, &log) <= DLT_RETURN_ERROR) {
        return DLT_RETURN_ERROR;
    }

    DLT_SEM_LOCK();

    handle->log_level_ptr = NULL;
    handle->trace_status_ptr = NULL;

    if (mct_user.mct_ll_ts != NULL) {
        /* Clear and free local stored context information */
        mct_set_id(mct_user.mct_ll_ts[handle->log_level_pos].contextID, "");

        mct_user.mct_ll_ts[handle->log_level_pos].log_level = DLT_USER_INITIAL_LOG_LEVEL;
        mct_user.mct_ll_ts[handle->log_level_pos].trace_status = DLT_USER_INITIAL_TRACE_STATUS;

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
#ifdef DLT_HP_LOG_ENABLE

        if (mct_user.mct_ll_ts[handle->log_level_pos].DltExtBuff_ptr) {
            free(mct_user.mct_ll_ts[handle->log_level_pos].DltExtBuff_ptr);
            mct_user.mct_ll_ts[handle->log_level_pos].DltExtBuff_ptr = 0;
        }

#endif
    }

    DLT_SEM_FREE();

    /* Inform daemon to unregister context */
    ret = mct_user_log_send_unregister_context(&log);

    return ret;
}

DltReturnValue mct_set_application_ll_ts_limit(DltLogLevelType loglevel,
                                               DltTraceStatusType tracestatus)
{
    uint32_t i;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    if ((loglevel < DLT_USER_LOG_LEVEL_NOT_SET) || (loglevel >= DLT_LOG_MAX)) {
        mct_vlog(LOG_ERR, "Loglevel %d is outside valid range", loglevel);
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if ((tracestatus < DLT_USER_TRACE_STATUS_NOT_SET) || (tracestatus >= DLT_TRACE_STATUS_MAX)) {
        mct_vlog(LOG_ERR, "Tracestatus %d is outside valid range", tracestatus);
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        if (mct_init() < 0) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    DLT_SEM_LOCK();

    if (mct_user.mct_ll_ts == NULL) {
        DLT_SEM_FREE();
        return DLT_RETURN_ERROR;
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

    DLT_SEM_FREE();

    /* Inform DLT server about update */
    return mct_send_app_ll_ts_limit(mct_user.appID, loglevel, tracestatus);
}

int mct_get_log_state()
{
    return mct_user.log_state;
}

/* @deprecated */
DltReturnValue mct_set_log_mode(DltUserLogMode mode)
{
    DLT_UNUSED(mode);

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    return 0;
}

int mct_set_resend_timeout_atexit(uint32_t timeout_in_milliseconds)
{
    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
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

DltReturnValue mct_user_log_write_start_init(DltContext *handle,
                                                    DltContextData *log,
                                                    DltLogLevelType loglevel,
                                                    bool is_verbose)
{
    DLT_LOG_FATAL_RESET_TRAP(loglevel);

    /* initialize values */
    if ((mct_user_log_init(handle, log) < DLT_RETURN_OK) || (mct_user.mct_ll_ts == NULL))
        return DLT_RETURN_ERROR;

    log->args_num = 0;
    log->log_level = loglevel;
    log->size = 0;
    log->use_timestamp = DLT_AUTO_TIMESTAMP;
    log->verbose_mode = is_verbose;

    return DLT_RETURN_TRUE;
}

static DltReturnValue mct_user_log_write_start_internal(DltContext *handle,
                                                        DltContextData *log,
                                                        DltLogLevelType loglevel,
                                                        uint32_t messageid,
                                                        bool is_verbose);

inline DltReturnValue mct_user_log_write_start(DltContext *handle,
                                               DltContextData *log,
                                               DltLogLevelType loglevel)
{
    return mct_user_log_write_start_internal(handle, log, loglevel, DLT_USER_DEFAULT_MSGID, true);
}

DltReturnValue mct_user_log_write_start_id(DltContext *handle,
                                           DltContextData *log,
                                           DltLogLevelType loglevel,
                                           uint32_t messageid)
{
    return mct_user_log_write_start_internal(handle, log, loglevel, messageid, false);
}

DltReturnValue mct_user_log_write_start_internal(DltContext *handle,
                                           DltContextData *log,
                                           DltLogLevelType loglevel,
                                           uint32_t messageid,
                                           bool is_verbose)
{
    int ret = DLT_RETURN_TRUE;

    /* check nullpointer */
    if ((handle == NULL) || (log == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* forbid mct usage in child after fork */
    if (g_mct_is_child) {
        return DLT_RETURN_ERROR;
    }

    /* check log levels */
    ret = mct_user_is_logLevel_enabled(handle, loglevel);

    if (ret == DLT_RETURN_WRONG_PARAMETER) {
        return DLT_RETURN_WRONG_PARAMETER;
    } else if (ret == DLT_RETURN_LOGGING_DISABLED) {
        log->handle = NULL;
        return DLT_RETURN_OK;
    }

    ret = mct_user_log_write_start_init(handle, log, loglevel, is_verbose);
    if (ret == DLT_RETURN_TRUE) {
        /* initialize values */
        if (log->buffer == NULL) {
            log->buffer = calloc(sizeof(unsigned char), mct_user.log_buf_len);

            if (log->buffer == NULL) {
                mct_vlog(LOG_ERR, "Cannot allocate buffer for DLT Log message\n");
                return DLT_RETURN_ERROR;
            }
        }

        /* In non-verbose mode, insert message id */
        if (!is_verbose_mode(mct_user.verbose_mode, log)) {
            if ((sizeof(uint32_t)) > mct_user.log_buf_len) {
                return DLT_RETURN_USER_BUFFER_FULL;
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

DltReturnValue mct_user_log_write_start_w_given_buffer(DltContext *handle,
                                                       DltContextData *log,
                                                       DltLogLevelType loglevel,
                                                       char *buffer,
                                                       size_t size,
                                                       int32_t args_num)
{
    int ret = DLT_RETURN_TRUE;

    /* check nullpointer */
    if ((handle == NULL) || (log == NULL) || (buffer == NULL))
        return DLT_RETURN_WRONG_PARAMETER;

    /* discard unexpected parameters */
    if ((size <= 0) || (size > mct_user.log_buf_len) || (args_num <= 0))
        return DLT_RETURN_WRONG_PARAMETER;

    /* forbid mct usage in child after fork */
    if (g_mct_is_child)
        return DLT_RETURN_ERROR;

    /* discard non-verbose mode */
    if (mct_user.verbose_mode == 0)
        return DLT_RETURN_ERROR;

    ret = mct_user_log_write_start_init(handle, log, loglevel, true);
    if (ret == DLT_RETURN_TRUE) {
        log->buffer = (unsigned char *)buffer;
        log->size = size;
        log->args_num = args_num;
    }

    return ret;
}

DltReturnValue mct_user_log_write_finish(DltContextData *log)
{
    int ret = DLT_RETURN_ERROR;

    if (log == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    ret = mct_user_log_send_log(log, DLT_TYPE_LOG);

    mct_user_free_buffer(&(log->buffer));

    return ret;
}

DltReturnValue mct_user_log_write_finish_w_given_buffer(DltContextData *log)
{
    int ret = DLT_RETURN_ERROR;

    if (log == NULL)
        return DLT_RETURN_WRONG_PARAMETER;

    ret = mct_user_log_send_log(log, DLT_TYPE_LOG);

    return ret;
}

static DltReturnValue mct_user_log_write_raw_internal(DltContextData *log, const void *data, uint16_t length, DltFormatType type, const char *name, bool with_var_info)
{
    /* check nullpointer */
    if ((log == NULL) || ((data == NULL) && (length != 0))) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* Have to cast type to signed type because some compilers assume that DltFormatType is unsigned and issue a warning */
    if (((int16_t)type < DLT_FORMAT_DEFAULT) || (type >= DLT_FORMAT_MAX)) {
        mct_vlog(LOG_ERR, "Format type %u is outside valid range", type);
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
    }

    const uint16_t name_size = (name != NULL) ? strlen(name)+1 : 0;

    size_t needed_size = length + sizeof(uint16_t);
    if ((log->size + needed_size) > mct_user.log_buf_len)
        return DLT_RETURN_USER_BUFFER_FULL;

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        uint32_t type_info = DLT_TYPE_INFO_RAWD;

        needed_size += sizeof(uint32_t);  // Type Info field
        if (with_var_info) {
            needed_size += sizeof(uint16_t);  // length of name
            needed_size += name_size;  // the name itself

            type_info |= DLT_TYPE_INFO_VARI;
        }
        if ((log->size + needed_size) > mct_user.log_buf_len)
            return DLT_RETURN_USER_BUFFER_FULL;

        // Genivi extension: put formatting hints into the unused (for RAWD) TYLE + SCOD fields.
        // The SCOD field holds the base (hex or bin); the TYLE field holds the column width (8bit..64bit).
        if ((type >= DLT_FORMAT_HEX8) && (type <= DLT_FORMAT_HEX64)) {
            type_info |= DLT_SCOD_HEX;
            type_info += type;
        } else if ((type >= DLT_FORMAT_BIN8) && (type <= DLT_FORMAT_BIN16)) {
            type_info |= DLT_SCOD_BIN;
            type_info += type - DLT_FORMAT_BIN8 + 1;
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

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_raw(DltContextData *log, void *data, uint16_t length)
{
    return mct_user_log_write_raw_internal(log, data, length, DLT_FORMAT_DEFAULT, NULL, false);
}

DltReturnValue mct_user_log_write_raw_formatted(DltContextData *log, void *data, uint16_t length, DltFormatType type)
{
    return mct_user_log_write_raw_internal(log, data, length, type, NULL, false);
}

DltReturnValue mct_user_log_write_raw_attr(DltContextData *log, const void *data, uint16_t length, const char *name)
{
    return mct_user_log_write_raw_internal(log, data, length, DLT_FORMAT_DEFAULT, name, true);
}

DltReturnValue mct_user_log_write_raw_formatted_attr(DltContextData *log, const void *data, uint16_t length, DltFormatType type, const char *name)
{
    return mct_user_log_write_raw_internal(log, data, length, type, name, true);
}

// Generic implementation for all "simple" types, possibly with attributes
static DltReturnValue mct_user_log_write_generic_attr(DltContextData *log, const void *datap, size_t datalen, uint32_t type_info, const VarInfo *varinfo)
{
    if (log == NULL)
        return DLT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
    }

    size_t needed_size = datalen;
    if ((log->size + needed_size) > mct_user.log_buf_len)
        return DLT_RETURN_USER_BUFFER_FULL;

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

            type_info |= DLT_TYPE_INFO_VARI;
        }
        if ((log->size + needed_size) > mct_user.log_buf_len)
            return DLT_RETURN_USER_BUFFER_FULL;

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

    return DLT_RETURN_OK;
}

// Generic implementation for all "simple" types
static DltReturnValue mct_user_log_write_generic_formatted(DltContextData *log, const void *datap, size_t datalen, uint32_t type_info, DltFormatType type)
{
    if (log == NULL)
        return DLT_RETURN_WRONG_PARAMETER;

    /* Have to cast type to signed type because some compilers assume that DltFormatType is unsigned and issue a warning */
    if (((int16_t)type < DLT_FORMAT_DEFAULT) || (type >= DLT_FORMAT_MAX)) {
        mct_vlog(LOG_ERR, "Format type %d is outside valid range", type);
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
    }

    size_t needed_size = datalen;
    if ((log->size + needed_size) > mct_user.log_buf_len)
        return DLT_RETURN_USER_BUFFER_FULL;

    if (is_verbose_mode(mct_user.verbose_mode, log)) {
        needed_size += sizeof(uint32_t);  // Type Info field
        if ((log->size + needed_size) > mct_user.log_buf_len)
            return DLT_RETURN_USER_BUFFER_FULL;

        // Genivi extension: put formatting hints into the unused (for SINT/UINT/FLOA) SCOD field.
        if ((type >= DLT_FORMAT_HEX8) && (type <= DLT_FORMAT_HEX64))
            type_info |= DLT_SCOD_HEX;

        else if ((type >= DLT_FORMAT_BIN8) && (type <= DLT_FORMAT_BIN16))
            type_info |= DLT_SCOD_BIN;

        memcpy(log->buffer + log->size, &type_info, sizeof(uint32_t));
        log->size += sizeof(uint32_t);
    }

    memcpy(log->buffer + log->size, datap, datalen);
    log->size += datalen;

    log->args_num++;

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_float32(DltContextData *log, float32_t data)
{
    if (sizeof(float32_t) != 4)
        return DLT_RETURN_ERROR;

    uint32_t type_info = DLT_TYPE_INFO_FLOA | DLT_TYLE_32BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(float32_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_float64(DltContextData *log, float64_t data)
{
    if (sizeof(float64_t) != 8)
        return DLT_RETURN_ERROR;

    uint32_t type_info = DLT_TYPE_INFO_FLOA | DLT_TYLE_64BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(float64_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_float32_attr(DltContextData *log, float32_t data, const char *name, const char *unit)
{
    if (sizeof(float32_t) != 4)
        return DLT_RETURN_ERROR;

    uint32_t type_info = DLT_TYPE_INFO_FLOA | DLT_TYLE_32BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(float32_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_float64_attr(DltContextData *log, float64_t data, const char *name, const char *unit)
{
    if (sizeof(float64_t) != 8)
        return DLT_RETURN_ERROR;

    uint32_t type_info = DLT_TYPE_INFO_FLOA | DLT_TYLE_64BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(float64_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_uint(DltContextData *log, unsigned int data)
{
    if (log == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
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
        return DLT_RETURN_ERROR;
        break;
    }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_uint8(DltContextData *log, uint8_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_8BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_uint16(DltContextData *log, uint16_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_16BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint16_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_uint32(DltContextData *log, uint32_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_32BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint32_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_uint64(DltContextData *log, uint64_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_64BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint64_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_uint_attr(DltContextData *log, unsigned int data, const char *name, const char *unit)
{
    if (log == NULL)
        return DLT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
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
        return DLT_RETURN_ERROR;
        break;
    }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_uint8_attr(DltContextData *log, uint8_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_8BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_uint16_attr(DltContextData *log, uint16_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_16BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint16_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_uint32_attr(DltContextData *log, uint32_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_32BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint32_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_uint64_attr(DltContextData *log, uint64_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_64BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint64_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_uint8_formatted(DltContextData *log,
                                                  uint8_t data,
                                                  DltFormatType type)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_8BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint8_t), type_info, type);
}

DltReturnValue mct_user_log_write_uint16_formatted(DltContextData *log,
                                                   uint16_t data,
                                                   DltFormatType type)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_16BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint16_t), type_info, type);
}

DltReturnValue mct_user_log_write_uint32_formatted(DltContextData *log,
                                                   uint32_t data,
                                                   DltFormatType type)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_32BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint32_t), type_info, type);
}

DltReturnValue mct_user_log_write_uint64_formatted(DltContextData *log,
                                                   uint64_t data,
                                                   DltFormatType type)
{
    uint32_t type_info = DLT_TYPE_INFO_UINT | DLT_TYLE_64BIT;
    return mct_user_log_write_generic_formatted(log, &data, sizeof(uint64_t), type_info, type);
}

DltReturnValue mct_user_log_write_ptr(DltContextData *log, void *data)
{
    if (log == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
    }

    switch (sizeof(void *)) {
        case 4:
            return mct_user_log_write_uint32_formatted(log,
                                                       (uintptr_t)data,
                                                       DLT_FORMAT_HEX32);
            break;
        case 8:
            return mct_user_log_write_uint64_formatted(log,
                                                       (uintptr_t)data,
                                                       DLT_FORMAT_HEX64);
            break;
        default:
            ; /* skip */
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_int(DltContextData *log, int data)
{
    if (log == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
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
        return DLT_RETURN_ERROR;
        break;
    }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_int8(DltContextData *log, int8_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_8BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int8_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_int16(DltContextData *log, int16_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_16BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int16_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_int32(DltContextData *log, int32_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_32BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int32_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_int64(DltContextData *log, int64_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_64BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(int64_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_int_attr(DltContextData *log, int data, const char *name, const char *unit)
{
    if (log == NULL)
        return DLT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
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
        return DLT_RETURN_ERROR;
        break;
    }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_int8_attr(DltContextData *log, int8_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_8BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int8_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_int16_attr(DltContextData *log, int16_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_16BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int16_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_int32_attr(DltContextData *log, int32_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_32BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int32_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_int64_attr(DltContextData *log, int64_t data, const char *name, const char *unit)
{
    uint32_t type_info = DLT_TYPE_INFO_SINT | DLT_TYLE_64BIT;
    const VarInfo var_info = { name, unit, true };
    return mct_user_log_write_generic_attr(log, &data, sizeof(int64_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_bool(DltContextData *log, uint8_t data)
{
    uint32_t type_info = DLT_TYPE_INFO_BOOL | DLT_TYLE_8BIT;
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, NULL);
}

DltReturnValue mct_user_log_write_bool_attr(DltContextData *log, uint8_t data, const char *name)
{
    uint32_t type_info = DLT_TYPE_INFO_BOOL | DLT_TYLE_8BIT;
    const VarInfo var_info = { name, NULL, false };
    return mct_user_log_write_generic_attr(log, &data, sizeof(uint8_t), type_info, &var_info);
}

DltReturnValue mct_user_log_write_string(DltContextData *log, const char *text)
{
    return mct_user_log_write_string_utils_attr(log, text, ASCII_STRING, NULL, false);
}

DltReturnValue mct_user_log_write_string_attr(DltContextData *log, const char *text, const char *name)
{
    return mct_user_log_write_string_utils_attr(log, text, ASCII_STRING, name, true);
}

DltReturnValue mct_user_log_write_sized_string(DltContextData *log,
                                               const char *text,
                                               uint16_t length)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, ASCII_STRING, NULL, false);
}

DltReturnValue mct_user_log_write_sized_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, ASCII_STRING, name, true);
}

DltReturnValue mct_user_log_write_constant_string(DltContextData *log, const char *text)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_string(log, text) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_constant_string_attr(DltContextData *log, const char *text, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_string_attr(log, text, name) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_sized_constant_string(DltContextData *log, const char *text, uint16_t length)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_string(log, text, length) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_sized_constant_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_string_attr(log, text, length, name) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_utf8_string(DltContextData *log, const char *text)
{
    return mct_user_log_write_string_utils_attr(log, text, UTF8_STRING, NULL, false);
}

DltReturnValue mct_user_log_write_utf8_string_attr(DltContextData *log, const char *text, const char *name)
{
    return mct_user_log_write_string_utils_attr(log, text, UTF8_STRING, name, true);
}

DltReturnValue mct_user_log_write_sized_utf8_string(DltContextData *log, const char *text, uint16_t length)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, UTF8_STRING, NULL, false);
}

DltReturnValue mct_user_log_write_sized_utf8_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name)
{
    return mct_user_log_write_sized_string_utils_attr(log, text, length, UTF8_STRING, name, true);
}

DltReturnValue mct_user_log_write_constant_utf8_string(DltContextData *log, const char *text)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_utf8_string(log, text) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_constant_utf8_string_attr(DltContextData *log, const char *text, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_utf8_string_attr(log, text, name) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_sized_constant_utf8_string(DltContextData *log, const char *text, uint16_t length)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_utf8_string(log, text, length) : DLT_RETURN_OK;
}

DltReturnValue mct_user_log_write_sized_constant_utf8_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name)
{
    /* Send parameter only in verbose mode */
    return is_verbose_mode(mct_user.verbose_mode, log) ? mct_user_log_write_sized_utf8_string_attr(log, text, length, name) : DLT_RETURN_OK;
}

static DltReturnValue mct_user_log_write_sized_string_utils_attr(DltContextData *log, const char *text, uint16_t length, const enum StringType type, const char *name, bool with_var_info)
{
    if ((log == NULL) || (text == NULL))
        return DLT_RETURN_WRONG_PARAMETER;

    if (!mct_user_initialised) {
        mct_vlog(LOG_WARNING, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
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

            type_info |= DLT_TYPE_INFO_VARI;
        }
    }

    size_t str_truncate_message_length = strlen(STR_TRUNCATED_MESSAGE) + 1;
    size_t max_payload_str_msg;
    DltReturnValue ret = DLT_RETURN_OK;

    /* Check log size condition */
    if (new_log_size > mct_user.log_buf_len) {
        ret = DLT_RETURN_USER_BUFFER_FULL;

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
            type_info |= DLT_TYPE_INFO_STRG | DLT_SCOD_ASCII;
            break;
        case UTF8_STRING:
            type_info |= DLT_TYPE_INFO_STRG | DLT_SCOD_UTF8;
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
    case DLT_RETURN_OK:
    {
        /* Whole string will be copied */
        memcpy(log->buffer + log->size, text, length);
        /* The input string might not be null-terminated, so we're doing that by ourselves */
        log->buffer[log->size + length] = '\0';
        log->size += arg_size;
        break;
    }
    case DLT_RETURN_USER_BUFFER_FULL:
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

static DltReturnValue mct_user_log_write_string_utils_attr(DltContextData *log, const char *text, const enum StringType type, const char *name, bool with_var_info)
{
    if ((log == NULL) || (text == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    uint16_t length = (uint16_t) strlen(text);
    return mct_user_log_write_sized_string_utils_attr(log, text, length, type, name, with_var_info);
}

DltReturnValue mct_register_injection_callback_with_id(DltContext *handle,
                                                       uint32_t service_id,
                                                       mct_injection_callback_id mct_injection_cbk,
                                                       void *priv)
{
    DltContextData log;
    uint32_t i, j, k;
    int found = 0;

    DltUserInjectionCallback *old;

    if (mct_user_log_init(handle, &log) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    if (service_id < DLT_USER_INJECTION_MIN) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* This function doesn't make sense storing to local file is choosen;
     * so terminate this function */
    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    DLT_SEM_LOCK();

    if (mct_user.mct_ll_ts == NULL) {
        DLT_SEM_FREE();
        return DLT_RETURN_OK;
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
                (DltUserInjectionCallback *)malloc(sizeof(DltUserInjectionCallback));

            if (mct_user.mct_ll_ts[i].injection_table == NULL) {
                DLT_SEM_FREE();
                return DLT_RETURN_ERROR;
            }
        } else {
            old = mct_user.mct_ll_ts[i].injection_table;
            mct_user.mct_ll_ts[i].injection_table = (DltUserInjectionCallback *)malloc(
                    sizeof(DltUserInjectionCallback) * (j + 1));

            if (mct_user.mct_ll_ts[i].injection_table == NULL) {
                mct_user.mct_ll_ts[i].injection_table = old;
                DLT_SEM_FREE();
                return DLT_RETURN_ERROR;
            }

            memcpy(mct_user.mct_ll_ts[i].injection_table, old, sizeof(DltUserInjectionCallback) * j);
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

    DLT_SEM_FREE();

    return DLT_RETURN_OK;
}

DltReturnValue mct_register_injection_callback(DltContext *handle, uint32_t service_id,
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

DltReturnValue mct_register_log_level_changed_callback(DltContext *handle,
                                                       void (*mct_log_level_changed_callback)(
                                                           char context_id[DLT_ID_SIZE],
                                                           uint8_t log_level,
                                                           uint8_t trace_status))
{
    DltContextData log;
    uint32_t i;

    if (mct_user_log_init(handle, &log) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    /* This function doesn't make sense storing to local file is choosen;
     * so terminate this function */
    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    DLT_SEM_LOCK();

    if (mct_user.mct_ll_ts == NULL) {
        DLT_SEM_FREE();
        return DLT_RETURN_OK;
    }

    /* Insert callback in corresponding table */
    i = handle->log_level_pos;

    /* Store new callback function */
    mct_user.mct_ll_ts[i].log_level_changed_callback = mct_log_level_changed_callback;

    DLT_SEM_FREE();

    return DLT_RETURN_OK;
}


DltReturnValue mct_log_string(DltContext *handle, DltLogLevelType loglevel, const char *text)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return DLT_RETURN_ERROR;

    if ((handle == NULL) || (text == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DltReturnValue ret = DLT_RETURN_OK;
    DltContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == DLT_RETURN_TRUE) {
        ret = mct_user_log_write_string(&log, text);

        if (mct_user_log_write_finish(&log) < DLT_RETURN_OK) {
            ret = DLT_RETURN_ERROR;
        }
    }

    return ret;
}

DltReturnValue mct_log_string_int(DltContext *handle,
                                  DltLogLevelType loglevel,
                                  const char *text,
                                  int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return DLT_RETURN_ERROR;

    if ((handle == NULL) || (text == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DltReturnValue ret = DLT_RETURN_OK;
    DltContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == DLT_RETURN_TRUE) {
        ret = mct_user_log_write_string(&log, text);
        mct_user_log_write_int(&log, data);

        if (mct_user_log_write_finish(&log) < DLT_RETURN_OK) {
            ret = DLT_RETURN_ERROR;
        }
    }

    return ret;
}

DltReturnValue mct_log_string_uint(DltContext *handle,
                                   DltLogLevelType loglevel,
                                   const char *text,
                                   unsigned int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return DLT_RETURN_ERROR;

    if ((handle == NULL) || (text == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DltReturnValue ret = DLT_RETURN_OK;
    DltContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == DLT_RETURN_TRUE) {
        ret = mct_user_log_write_string(&log, text);
        mct_user_log_write_uint(&log, data);

        if (mct_user_log_write_finish(&log) < DLT_RETURN_OK) {
            ret = DLT_RETURN_ERROR;
        }
    }

    return ret;
}

DltReturnValue mct_log_int(DltContext *handle, DltLogLevelType loglevel, int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return DLT_RETURN_ERROR;

    if (handle == NULL) {
        return DLT_RETURN_ERROR;
    }

    DltContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == DLT_RETURN_TRUE) {
        mct_user_log_write_int(&log, data);

        if (mct_user_log_write_finish(&log) < DLT_RETURN_OK) {
            return DLT_RETURN_ERROR;
        }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_log_uint(DltContext *handle, DltLogLevelType loglevel, unsigned int data)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return DLT_RETURN_ERROR;

    if (handle == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DltContextData log;

    if (mct_user_log_write_start(handle, &log, loglevel) == DLT_RETURN_TRUE) {
        mct_user_log_write_uint(&log, data);

        if (mct_user_log_write_finish(&log) < DLT_RETURN_OK) {
            return DLT_RETURN_ERROR;
        }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_log_raw(DltContext *handle,
                           DltLogLevelType loglevel,
                           void *data,
                           uint16_t length)
{
    if (!is_verbose_mode(mct_user.verbose_mode, NULL))
        return DLT_RETURN_ERROR;

    if (handle == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DltContextData log;
    DltReturnValue ret = DLT_RETURN_OK;

    if (mct_user_log_write_start(handle, &log, loglevel) > 0) {
        if ((ret = mct_user_log_write_raw(&log, data, length)) < DLT_RETURN_OK) {
            mct_user_free_buffer(&(log.buffer));
            return ret;
        }

        if (mct_user_log_write_finish(&log) < DLT_RETURN_OK) {
            return DLT_RETURN_ERROR;
        }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_log_marker()
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    return mct_user_log_send_marker();
}

DltReturnValue mct_verbose_mode(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* Switch to verbose mode */
    mct_user.verbose_mode = 1;

    return DLT_RETURN_OK;
}

DltReturnValue mct_nonverbose_mode(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* Switch to non-verbose mode */
    mct_user.verbose_mode = 0;

    return DLT_RETURN_OK;
}

DltReturnValue mct_use_extended_header_for_non_verbose(int8_t use_extended_header_for_non_verbose)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* Set use_extended_header_for_non_verbose */
    mct_user.use_extended_header_for_non_verbose = use_extended_header_for_non_verbose;

    return DLT_RETURN_OK;
}

DltReturnValue mct_with_session_id(int8_t with_session_id)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* Set use_extended_header_for_non_verbose */
    mct_user.with_session_id = with_session_id;

    return DLT_RETURN_OK;
}

DltReturnValue mct_with_timestamp(int8_t with_timestamp)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* Set with_timestamp */
    mct_user.with_timestamp = with_timestamp;

    return DLT_RETURN_OK;
}

DltReturnValue mct_with_ecu_id(int8_t with_ecu_id)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    /* Set with_timestamp */
    mct_user.with_ecu_id = with_ecu_id;

    return DLT_RETURN_OK;
}

DltReturnValue mct_enable_local_print(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    mct_user.enable_local_print = 1;

    return DLT_RETURN_OK;
}

DltReturnValue mct_disable_local_print(void)
{
    if (!mct_user_initialised) {
        if (mct_init() < DLT_RETURN_OK) {
            mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            return DLT_RETURN_ERROR;
        }
    }

    mct_user.enable_local_print = 0;

    return DLT_RETURN_OK;
}

/* Cleanup on thread cancellation, thread may hold lock release it here */
static void mct_user_cleanup_handler(void *arg)
{
    DLT_UNUSED(arg); /* Satisfy compiler */
    /* unlock the DLT buffer */
    mct_unlock_mutex(&flush_mutex);

    /* unlock DLT (mct_mutex) */
    DLT_SEM_FREE();
}

void mct_user_housekeeperthread_function(__attribute__((unused)) void *ptr)
{
    struct timespec ts;
    bool in_loop = true;


#ifdef DLT_USE_PTHREAD_SETNAME_NP
    if (pthread_setname_np(mct_housekeeperthread_handle, "mct_housekeeper"))
        mct_log(LOG_WARNING, "Failed to rename housekeeper thread!\n");
#elif linux
    if (prctl(PR_SET_NAME, "mct_housekeeper", 0, 0, 0) < 0)
        mct_log(LOG_WARNING, "Failed to rename housekeeper thread!\n");
#endif

    pthread_cleanup_push(mct_user_cleanup_handler, NULL);

    while (in_loop) {
        /* Check for new messages from DLT daemon */
        if (!mct_user.disable_injection_msg) {
            if (mct_user_log_check_user_message() < DLT_RETURN_OK) {
                /* Critical error */
                mct_log(LOG_CRIT, "Housekeeper thread encountered error condition\n");
            }
        }

        /* flush buffer to DLT daemon if possible */
        pthread_mutex_lock(&flush_mutex);

        /* mct_buffer_empty is set by main thread to 0 in case data is written to buffer */
        if (g_mct_buffer_empty == 0) {
            /* Reattach to daemon if neccesary */
            mct_user_log_reattach_to_daemon();

            if (mct_user.mct_log_handle > 0) {
                if (mct_user_log_resend_buffer() == DLT_RETURN_OK) {
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
        ts.tv_nsec = DLT_USER_RECEIVE_NDELAY;
        nanosleep(&ts, NULL);
    }

    pthread_cleanup_pop(1);
}

/* Private functions of user library */

DltReturnValue mct_user_log_init(DltContext *handle, DltContextData *log)
{
    int ret = DLT_RETURN_OK;

    if ((handle == NULL) || (log == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (!mct_user_initialised) {
        ret = mct_init();

        if (ret < DLT_RETURN_OK) {
            if (ret != DLT_RETURN_LOGGING_DISABLED) {
                mct_vlog(LOG_ERR, "%s Failed to initialise mct", __FUNCTION__);
            }

            return ret;
        }
    }

    log->handle = handle;
    log->buffer = NULL;
    return ret;
}

DltReturnValue mct_user_log_send_log(DltContextData *log, int mtype)
{
    DltMessage msg;
    DltUserHeader userheader;
    int32_t len;

    DltReturnValue ret = DLT_RETURN_OK;

    if (!mct_user_initialised) {
        mct_vlog(LOG_ERR, "%s mct_user_initialised false\n", __FUNCTION__);
        return DLT_RETURN_ERROR;
    }

    if ((log == NULL) ||
        (log->handle == NULL) ||
        (log->handle->contextID[0] == '\0') ||
        (mtype < DLT_TYPE_LOG) || (mtype > DLT_TYPE_CONTROL)
        ) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* also for Trace messages */
    if (mct_user_set_userheader(&userheader, DLT_USER_MESSAGE_LOG) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    if (mct_message_init(&msg, 0) == DLT_RETURN_ERROR) {
        return DLT_RETURN_ERROR;
    }

    msg.storageheader = (DltStorageHeader *)msg.headerbuffer;

    if (mct_set_storageheader(msg.storageheader, mct_user.ecuID) == DLT_RETURN_ERROR) {
        return DLT_RETURN_ERROR;
    }

    msg.standardheader = (DltStandardHeader *)(msg.headerbuffer + sizeof(DltStorageHeader));
    msg.standardheader->htyp = DLT_HTYP_PROTOCOL_VERSION1;

    /* send ecu id */
    if (mct_user.with_ecu_id) {
        msg.standardheader->htyp |= DLT_HTYP_WEID;
    }

    /* send timestamp */
    if (mct_user.with_timestamp) {
        msg.standardheader->htyp |= DLT_HTYP_WTMS;
    }

    /* send session id */
    if (mct_user.with_session_id) {
        msg.standardheader->htyp |= DLT_HTYP_WSID;
        msg.headerextra.seid = getpid();
    }

    if (is_verbose_mode(mct_user.verbose_mode, log)){
        /* In verbose mode, send extended header */
        msg.standardheader->htyp = (msg.standardheader->htyp | DLT_HTYP_UEH);
    } else
    /* In non-verbose, send extended header if desired */
    if (mct_user.use_extended_header_for_non_verbose) {
        msg.standardheader->htyp = (msg.standardheader->htyp | DLT_HTYP_UEH);
    }

#if (BYTE_ORDER == BIG_ENDIAN)
    msg.standardheader->htyp = (msg.standardheader->htyp | DLT_HTYP_MSBF);
#endif

    msg.standardheader->mcnt = log->handle->mcnt++;

    /* Set header extra parameters */
    mct_set_id(msg.headerextra.ecu, mct_user.ecuID);

    /*msg.headerextra.seid = 0; */
    if (log->use_timestamp == DLT_AUTO_TIMESTAMP) {
        msg.headerextra.tmsp = mct_uptime();
    } else {
        msg.headerextra.tmsp = log->user_timestamp;
    }

    if (mct_message_set_extraparameters(&msg, 0) == DLT_RETURN_ERROR) {
        return DLT_RETURN_ERROR;
    }

    /* Fill out extended header, if extended header should be provided */
    if (DLT_IS_HTYP_UEH(msg.standardheader->htyp)) {
        /* with extended header */
        msg.extendedheader =
            (DltExtendedHeader *)(msg.headerbuffer + sizeof(DltStorageHeader) +
                                  sizeof(DltStandardHeader) +
                                  DLT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp));

        switch (mtype) {
            case DLT_TYPE_LOG:
            {
                msg.extendedheader->msin = (DLT_TYPE_LOG << DLT_MSIN_MSTP_SHIFT) |
                    ((log->log_level << DLT_MSIN_MTIN_SHIFT) & DLT_MSIN_MTIN);
                break;
            }
            case DLT_TYPE_NW_TRACE:
            {
                msg.extendedheader->msin = (DLT_TYPE_NW_TRACE << DLT_MSIN_MSTP_SHIFT) |
                    ((log->trace_status << DLT_MSIN_MTIN_SHIFT) & DLT_MSIN_MTIN);
                break;
            }
            default:
            {
                /* This case should not occur */
                return DLT_RETURN_ERROR;
                break;
            }
        }

        /* If in verbose mode, set flag in header for verbose mode */
        if (is_verbose_mode(mct_user.verbose_mode, log))
            msg.extendedheader->msin |= DLT_MSIN_VERB;

        msg.extendedheader->noar = log->args_num;                     /* number of arguments */
        mct_set_id(msg.extendedheader->apid, mct_user.appID);         /* application id */
        mct_set_id(msg.extendedheader->ctid, log->handle->contextID); /* context id */

        msg.headersize = sizeof(DltStorageHeader) + sizeof(DltStandardHeader) +
            sizeof(DltExtendedHeader) +
            DLT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp);
    } else {
        /* without extended header */
        msg.headersize = sizeof(DltStorageHeader) + sizeof(DltStandardHeader) +
            DLT_STANDARD_HEADER_EXTRA_SIZE(
                msg.standardheader->htyp);
    }

    len = msg.headersize - sizeof(DltStorageHeader) + log->size;

    if (len > UINT16_MAX) {
        mct_log(LOG_WARNING, "Huge message discarded!\n");
        return DLT_RETURN_ERROR;
    }

    msg.standardheader->len = DLT_HTOBE_16(len);

    /* print to std out, if enabled */
    if ((mct_user.local_print_mode != DLT_PM_FORCE_OFF) &&
        (mct_user.local_print_mode != DLT_PM_AUTOMATIC)) {
        if ((mct_user.enable_local_print) || (mct_user.local_print_mode == DLT_PM_FORCE_ON)) {
            if (mct_user_print_msg(&msg, log) == DLT_RETURN_ERROR) {
                return DLT_RETURN_ERROR;
            }
        }
    }

    if (mct_user.mct_is_file) {
        if (mct_user_file_reach_max) {
            return DLT_RETURN_FILESZERR;
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
                return DLT_RETURN_FILESZERR;
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
            if (mct_user_log_send_overflow() == DLT_RETURN_OK) {
                mct_vnlog(LOG_WARNING,
                          DLT_USER_BUFFER_LENGTH,
                          "%u messages discarded!\n",
                          mct_user.overflow_counter);
                mct_user.overflow_counter = 0;
            }
        }

        /* try to resent old data first */
        ret = DLT_RETURN_OK;

        if ((mct_user.mct_log_handle != -1) && (mct_user.appID[0] != '\0')) {
            /* buffer not empty */
            if (g_mct_buffer_empty == 0) {
                ret = mct_user_log_resend_buffer();
            }
        }

        if ((ret == DLT_RETURN_OK) && (mct_user.appID[0] != '\0')) {
            pthread_mutex_lock(&flush_mutex);
            /* resend ok or nothing to resent */
            g_mct_buffer_empty = 1;
            pthread_mutex_unlock(&flush_mutex);
            ret = mct_user_log_out3(mct_user.mct_log_handle,
                                    &(userheader), sizeof(DltUserHeader),
                                    msg.headerbuffer + sizeof(DltStorageHeader),
                                    msg.headersize - sizeof(DltStorageHeader),
                                    log->buffer, log->size);

        }

        DltReturnValue process_error_ret = DLT_RETURN_OK;
        /* store message in ringbuffer, if an error has occured */
        if ((ret != DLT_RETURN_OK) || (mct_user.appID[0] == '\0')) {
            process_error_ret = mct_user_log_out_error_handling(&(userheader),
                                                  sizeof(DltUserHeader),
                                                  msg.headerbuffer + sizeof(DltStorageHeader),
                                                  msg.headersize - sizeof(DltStorageHeader),
                                                  log->buffer,
                                                  log->size);
        }

        if (process_error_ret == DLT_RETURN_OK)
            return DLT_RETURN_OK;
        if (process_error_ret == DLT_RETURN_BUFFER_FULL) {
            /* Buffer full */
            mct_user.overflow_counter += 1;
            return DLT_RETURN_BUFFER_FULL;
        }

        /* handle return value of function mct_user_log_out3() when process_error_ret < 0*/
        switch (ret) {
            case DLT_RETURN_PIPE_FULL:
            {
                /* data could not be written */
                return DLT_RETURN_PIPE_FULL;
            }
            case DLT_RETURN_PIPE_ERROR:
            {
                /* handle not open or pipe error */
                close(mct_user.mct_log_handle);
                mct_user.mct_log_handle = -1;
#if defined DLT_LIB_USE_UNIX_SOCKET_IPC
                mct_user.connection_state = DLT_USER_RETRY_CONNECT;
#endif

                if (mct_user.local_print_mode == DLT_PM_AUTOMATIC) {
                    mct_user_print_msg(&msg, log);
                }

                return DLT_RETURN_PIPE_ERROR;
            }
            case DLT_RETURN_ERROR:
            {
                /* other error condition */
                return DLT_RETURN_ERROR;
            }
            case DLT_RETURN_OK:
            {
                return DLT_RETURN_OK;
            }
            default:
            {
                /* This case should never occur. */
                return DLT_RETURN_ERROR;
            }
        }
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_send_register_application(void)
{
    DltUserHeader userheader;
    DltUserControlMsgRegisterApplication usercontext;

    DltReturnValue ret;

    if (mct_user.appID[0] == '\0') {
        return DLT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader,
                                DLT_USER_MESSAGE_REGISTER_APPLICATION) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
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
        return DLT_RETURN_OK;
    }

    ret = mct_user_log_out3(mct_user.mct_log_handle,
                            &(userheader), sizeof(DltUserHeader),
                            &(usercontext), sizeof(DltUserControlMsgRegisterApplication),
                            mct_user.application_description, usercontext.description_length);

    /* store message in ringbuffer, if an error has occured */
    if (ret < DLT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(DltUserHeader),
                                               &(usercontext),
                                               sizeof(DltUserControlMsgRegisterApplication),
                                               mct_user.application_description,
                                               usercontext.description_length);
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_send_unregister_application(void)
{
    DltUserHeader userheader;
    DltUserControlMsgUnregisterApplication usercontext;
    DltReturnValue ret = DLT_RETURN_OK;

    if (mct_user.appID[0] == '\0') {
        return DLT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader,
                                DLT_USER_MESSAGE_UNREGISTER_APPLICATION) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, mct_user.appID);       /* application id */
    usercontext.pid = getpid();

    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader), sizeof(DltUserHeader),
                            &(usercontext), sizeof(DltUserControlMsgUnregisterApplication));

    /* store message in ringbuffer, if an error has occured */
    if (ret < DLT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(DltUserHeader),
                                               &(usercontext),
                                               sizeof(DltUserControlMsgUnregisterApplication),
                                               NULL,
                                               0);
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_send_register_context(DltContextData *log)
{
    DltUserHeader userheader;
    DltUserControlMsgRegisterContext usercontext;
    DltReturnValue ret = DLT_RETURN_ERROR;

    if (log == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (log->handle == NULL) {
        return DLT_RETURN_ERROR;
    }

    if (log->handle->contextID[0] == '\0') {
        return DLT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader, DLT_USER_MESSAGE_REGISTER_CONTEXT) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
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
        return DLT_RETURN_OK;
    }

    if (mct_user.appID[0] != '\0') {
        ret =
            mct_user_log_out3(mct_user.mct_log_handle,
                              &(userheader),
                              sizeof(DltUserHeader),
                              &(usercontext),
                              sizeof(DltUserControlMsgRegisterContext),
                              log->context_description,
                              usercontext.description_length);
    }

    /* store message in ringbuffer, if an error has occured */
    if ((ret != DLT_RETURN_OK) || (mct_user.appID[0] == '\0')) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(DltUserHeader),
                                               &(usercontext),
                                               sizeof(DltUserControlMsgRegisterContext),
                                               log->context_description,
                                               usercontext.description_length);
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_send_unregister_context(DltContextData *log)
{
    DltUserHeader userheader;
    DltUserControlMsgUnregisterContext usercontext;
    DltReturnValue ret;

    if (log == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (log->handle == NULL) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    if (log->handle->contextID[0] == '\0') {
        return DLT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader,
                                DLT_USER_MESSAGE_UNREGISTER_CONTEXT) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, mct_user.appID);         /* application id */
    mct_set_id(usercontext.ctid, log->handle->contextID); /* context id */
    usercontext.pid = getpid();

    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader),
                            sizeof(DltUserHeader),
                            &(usercontext),
                            sizeof(DltUserControlMsgUnregisterContext));

    /* store message in ringbuffer, if an error has occured */
    if (ret < DLT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(DltUserHeader),
                                               &(usercontext),
                                               sizeof(DltUserControlMsgUnregisterContext),
                                               NULL,
                                               0);
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_send_app_ll_ts_limit(const char *apid,
                                        DltLogLevelType loglevel,
                                        DltTraceStatusType tracestatus)
{
    DltUserHeader userheader;
    DltUserControlMsgAppLogLevelTraceStatus usercontext;
    DltReturnValue ret;

    if ((loglevel < DLT_USER_LOG_LEVEL_NOT_SET) || (loglevel >= DLT_LOG_MAX)) {
        mct_vlog(LOG_ERR, "Loglevel %d is outside valid range", loglevel);
        return DLT_RETURN_ERROR;
    }

    if ((tracestatus < DLT_USER_TRACE_STATUS_NOT_SET) || (tracestatus >= DLT_TRACE_STATUS_MAX)) {
        mct_vlog(LOG_ERR, "Tracestatus %d is outside valid range", tracestatus);
        return DLT_RETURN_ERROR;
    }

    if ((apid == NULL) || (apid[0] == '\0')) {
        return DLT_RETURN_ERROR;
    }

    /* set userheader */
    if (mct_user_set_userheader(&userheader, DLT_USER_MESSAGE_APP_LL_TS) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    /* set usercontext */
    mct_set_id(usercontext.apid, apid);       /* application id */
    usercontext.log_level = loglevel;
    usercontext.trace_status = tracestatus;

    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader), sizeof(DltUserHeader),
                            &(usercontext), sizeof(DltUserControlMsgAppLogLevelTraceStatus));

    /* store message in ringbuffer, if an error has occured */
    if (ret < DLT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(DltUserHeader),
                                               &(usercontext),
                                               sizeof(DltUserControlMsgAppLogLevelTraceStatus),
                                               NULL,
                                               0);
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_send_marker()
{
    DltUserHeader userheader;
    DltReturnValue ret;

    /* set userheader */
    if (mct_user_set_userheader(&userheader, DLT_USER_MESSAGE_MARKER) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    /* log to FIFO */
    ret = mct_user_log_out2(mct_user.mct_log_handle,
                            &(userheader), sizeof(DltUserHeader), 0, 0);

    /* store message in ringbuffer, if an error has occured */
    if (ret < DLT_RETURN_OK) {
        return mct_user_log_out_error_handling(&(userheader),
                                               sizeof(DltUserHeader),
                                               NULL,
                                               0,
                                               NULL,
                                               0);
    }

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_print_msg(DltMessage *msg, DltContextData *log)
{
    uint8_t *databuffer_tmp;
    int32_t datasize_tmp;
    int32_t databuffersize_tmp;
    static char text[DLT_USER_TEXT_LENGTH];

    if ((msg == NULL) || (log == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    /* Save variables before print */
    databuffer_tmp = msg->databuffer;
    datasize_tmp = msg->datasize;
    databuffersize_tmp = msg->databuffersize;

    /* Act like a receiver, convert header back to host format */
    msg->standardheader->len = DLT_BETOH_16(msg->standardheader->len);
    mct_message_get_extraparameters(msg, 0);

    msg->databuffer = log->buffer;
    msg->datasize = log->size;
    msg->databuffersize = log->size;

    /* Print message as ASCII */
    if (mct_message_print_ascii(msg, text, DLT_USER_TEXT_LENGTH, 0) == DLT_RETURN_ERROR) {
        return DLT_RETURN_ERROR;
    }

    /* Restore variables and set len to BE*/
    msg->databuffer = databuffer_tmp;
    msg->databuffersize = databuffersize_tmp;
    msg->datasize = datasize_tmp;

    msg->standardheader->len = DLT_HTOBE_16(msg->standardheader->len);

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_check_user_message(void)
{
    int offset = 0;
    int leave_while = 0;
    int ret = 0;
    int poll_timeout = DLT_USER_RECEIVE_MDELAY;

    uint32_t i;
    int fd;
    struct pollfd nfd[1];

    DltUserHeader *userheader;
    DltReceiver *receiver = &(mct_user.receiver);

    DltUserControlMsgLogLevel *usercontextll;
    DltUserControlMsgInjection *usercontextinj;
    DltUserControlMsgLogState *userlogstate;
    DltUserControlMsgBlockMode *blockmode;
    unsigned char *userbuffer;

    /* For delayed calling of injection callback, to avoid deadlock */
    DltUserInjectionCallback delayed_injection_callback;
    DltUserLogLevelChangedCallback delayed_log_level_changed_callback;
    unsigned char *delayed_inject_buffer = 0;
    uint32_t delayed_inject_data_length = 0;

    /* Ensure that callback is null before searching for it */
    delayed_injection_callback.injection_callback = 0;
    delayed_injection_callback.injection_callback_with_id = 0;
    delayed_injection_callback.service_id = 0;
    delayed_log_level_changed_callback.log_level_changed_callback = 0;
    delayed_injection_callback.data = 0;

#if defined DLT_LIB_USE_UNIX_SOCKET_IPC
    fd = mct_user.mct_log_handle;
#else /* DLT_LIB_USE_FIFO_IPC */
    fd = mct_user.mct_user_handle;
#endif

    nfd[0].events = POLLIN;
    nfd[0].fd = fd;

#if defined DLT_LIB_USE_UNIX_SOCKET_IPC

    if (fd != DLT_FD_INIT) {
#else /* DLT_LIB_USE_FIFO_IPC */

    if ((fd != DLT_FD_INIT) && (mct_user.mct_log_handle > 0)) {
#endif
        ret = poll(nfd, 1, poll_timeout);

        if (ret) {
            if (nfd[0].revents & (POLLHUP | POLLNVAL | POLLERR)) {
                mct_user.mct_log_handle = DLT_FD_INIT;
                return DLT_RETURN_ERROR;
            }

            if (mct_receiver_receive(receiver) <= 0) {
                /* No new message available */
                return DLT_RETURN_OK;
            }

            /* look through buffer as long as data is in there */
            while (1) {
                if (receiver->bytesRcvd < (int32_t)sizeof(DltUserHeader)) {
                    break;
                }

                /* resync if necessary */
                offset = 0;

                do {
                    userheader = (DltUserHeader *)(receiver->buf + offset);

                    /* Check for user header pattern */
                    if (mct_user_check_userheader(userheader)) {
                        break;
                    }

                    offset++;
                } while ((int32_t)(sizeof(DltUserHeader) + offset) <= receiver->bytesRcvd);

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
                    case DLT_USER_MESSAGE_LOG_LEVEL:
                    {
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(DltUserHeader) + sizeof(DltUserControlMsgLogLevel))) {
                            leave_while = 1;
                            break;
                        }

                        usercontextll =
                            (DltUserControlMsgLogLevel *)(receiver->buf + sizeof(DltUserHeader));

                        /* Update log level and trace status */
                        if (usercontextll != NULL) {
                            DLT_SEM_LOCK();

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
                                        DLT_ID_SIZE);
                                    delayed_log_level_changed_callback.log_level =
                                        usercontextll->log_level;
                                    delayed_log_level_changed_callback.trace_status =
                                        usercontextll->trace_status;
                                }
                            }

                            DLT_SEM_FREE();
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
                                                sizeof(DltUserHeader) +
                                                sizeof(DltUserControlMsgLogLevel)) ==
                            DLT_RETURN_ERROR) {
                            return DLT_RETURN_ERROR;
                        }
                    }
                    break;
                    case DLT_USER_MESSAGE_INJECTION:
                    {
                        /* At least, user header, user context, and service id and data_length of injected message is available */
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(DltUserHeader) +
                                      sizeof(DltUserControlMsgInjection))) {
                            leave_while = 1;
                            break;
                        }

                        usercontextinj =
                            (DltUserControlMsgInjection *)(receiver->buf + sizeof(DltUserHeader));
                        userbuffer =
                            (unsigned char *)(receiver->buf + sizeof(DltUserHeader) +
                                              sizeof(DltUserControlMsgInjection));

                        if (userbuffer != NULL) {

                            if (receiver->bytesRcvd <
                                (int32_t)(sizeof(DltUserHeader) +
                                          sizeof(DltUserControlMsgInjection) +
                                          usercontextinj->data_length_inject)) {
                                leave_while = 1;
                                break;
                            }

                            DLT_SEM_LOCK();

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
                                            DLT_SEM_FREE();
                                            mct_log(LOG_WARNING, "malloc failed!\n");
                                            return DLT_RETURN_ERROR;
                                        }

                                        break;
                                    }
                                }
                            }

                            DLT_SEM_FREE();

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
                                                    (sizeof(DltUserHeader) +
                                                     sizeof(DltUserControlMsgInjection) +
                                                     usercontextinj->data_length_inject)) !=
                                DLT_RETURN_OK) {
                                return DLT_RETURN_ERROR;
                            }
                        }
                    }
                    break;
                    case DLT_USER_MESSAGE_LOG_STATE:
                    {
                        /* At least, user header, user context, and service id and data_length of injected message is available */
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(DltUserHeader) + sizeof(DltUserControlMsgLogState))) {
                            leave_while = 1;
                            break;
                        }

                        userlogstate =
                            (DltUserControlMsgLogState *)(receiver->buf + sizeof(DltUserHeader));
                        mct_user.log_state = userlogstate->log_state;

                        /* keep not read data in buffer */
                        if (mct_receiver_remove(receiver,
                                                (sizeof(DltUserHeader) +
                                                 sizeof(DltUserControlMsgLogState))) ==
                            DLT_RETURN_ERROR) {
                            return DLT_RETURN_ERROR;
                        }
                    }
                    break;
                    case DLT_USER_MESSAGE_SET_BLOCK_MODE:
                    {
                        if (receiver->bytesRcvd <
                            (int32_t)(sizeof(DltUserHeader) +
                                      sizeof(DltUserControlMsgBlockMode))) {
                            leave_while = 1;
                            break;
                        }

                        blockmode =
                            (DltUserControlMsgBlockMode *)(receiver->buf + sizeof(DltUserHeader));

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
                                                (sizeof(DltUserHeader) +
                                                 sizeof(DltUserControlMsgBlockMode))) == -1) {
                            return -1;
                        }
                    }
                    break;
                    default:
                    {
                        mct_log(LOG_WARNING, "Invalid user message type received!\n");
                        /* Ignore result */
                        mct_receiver_remove(receiver, sizeof(DltUserHeader));
                        /* In next invocation of while loop, a resync will be triggered if additional data was received */
                    }
                    break;
                } /* switch() */

                if (leave_while == 1) {
                    leave_while = 0;
                    break;
                }
            } /* while buffer*/

            if (mct_receiver_move_to_begin(receiver) == DLT_RETURN_ERROR) {
                return DLT_RETURN_ERROR;
            }
        } /* while receive */
    }     /* if */

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_resend_buffer(void)
{
    int num, count;
    int size;
    DltReturnValue ret;

    DLT_SEM_LOCK();

    if (mct_user.appID[0] == '\0') {
        DLT_SEM_FREE();
        return 0;
    }

    /* Send content of ringbuffer */
    count = mct_buffer_get_message_count(&(mct_user.startup_buffer));
    DLT_SEM_FREE();

    for (num = 0; num < count; num++) {

        DLT_SEM_LOCK();
        size = mct_buffer_copy(&(mct_user.startup_buffer),
                               mct_user.resend_buffer,
                               mct_user.log_buf_len);

        if (size > 0) {
            DltUserHeader *userheader = (DltUserHeader *)(mct_user.resend_buffer);

            /* Add application id to the messages of needed*/
            if (mct_user_check_userheader(userheader)) {
                switch (userheader->message) {
                    case DLT_USER_MESSAGE_REGISTER_CONTEXT:
                    {
                        DltUserControlMsgRegisterContext *usercontext =
                            (DltUserControlMsgRegisterContext *)(mct_user.resend_buffer +
                                                                 sizeof(DltUserHeader));

                        if ((usercontext != 0) && (usercontext->apid[0] == '\0')) {
                            mct_set_id(usercontext->apid, mct_user.appID);
                        }

                        break;
                    }
                    case DLT_USER_MESSAGE_LOG:
                    {
                        DltExtendedHeader *extendedHeader =
                            (DltExtendedHeader *)(mct_user.resend_buffer + sizeof(DltUserHeader) +
                                                  sizeof(DltStandardHeader) +
                                                  sizeof(DltStandardHeaderExtra));

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
            if (ret == DLT_RETURN_OK) {
                mct_buffer_remove(&(mct_user.startup_buffer));
            } else {
                if (ret == DLT_RETURN_PIPE_ERROR) {
                    /* handle not open or pipe error */
                    close(mct_user.mct_log_handle);
                    mct_user.mct_log_handle = -1;
                }

                /* keep message in ringbuffer */
                DLT_SEM_FREE();
                return ret;
            }
        }

        DLT_SEM_FREE();
    }

    return DLT_RETURN_OK;
}

void mct_user_log_reattach_to_daemon(void)
{
    uint32_t num;
    DltContext handle;
    DltContextData log_new;

    if (mct_user.mct_log_handle < 0) {
        mct_user.mct_log_handle = DLT_FD_INIT;

#ifdef DLT_LIB_USE_UNIX_SOCKET_IPC
        /* try to open connection to mct daemon */
        mct_initialize_socket_connection();

        if (mct_user.connection_state != DLT_USER_CONNECTED) {
            /* return if not connected */
            return;
        }
#else   /* DLT_LIB_USE_FIFO_IPC */
      /* try to open pipe to mct daemon */
        int fd = open(mct_daemon_fifo, O_WRONLY | O_NONBLOCK);

        if (fd < 0) {
            return;
        }

        mct_user.mct_log_handle = fd;
#endif

        if (mct_user_log_init(&handle, &log_new) < DLT_RETURN_OK) {
            return;
        }

        mct_log(LOG_NOTICE, "Logging (re-)enabled!\n");

        /* Re-register application */
        if (mct_user_log_send_register_application() < DLT_RETURN_ERROR) {
            return;
        }

        DLT_SEM_LOCK();

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
                /* function  mct_user_log_send_register_context() can take the mutex to write to the DLT buffer. => dead lock */
                DLT_SEM_FREE();

                log_new.log_level = DLT_USER_LOG_LEVEL_NOT_SET;
                log_new.trace_status = DLT_USER_TRACE_STATUS_NOT_SET;

                if (mct_user_log_send_register_context(&log_new) < DLT_RETURN_ERROR) {
                    return;
                }

                /* Lock again the mutex */
                /* it is necessary in the for(;;) test, in order to have coherent mct_user data all over the critical section. */
                DLT_SEM_LOCK();
            }
        }

        DLT_SEM_FREE();
    }
}

DltReturnValue mct_user_log_send_overflow(void)
{
    DltUserHeader userheader;
    DltUserControlMsgBufferOverflow userpayload;

    /* set userheader */
    if (mct_user_set_userheader(&userheader, DLT_USER_MESSAGE_OVERFLOW) < DLT_RETURN_OK) {
        return DLT_RETURN_ERROR;
    }

    if (mct_user.mct_is_file) {
        return DLT_RETURN_OK;
    }

    /* set user message parameters */
    userpayload.overflow_counter = mct_user.overflow_counter;
    mct_set_id(userpayload.apid, mct_user.appID);

    return mct_user_log_out2(mct_user.mct_log_handle,
                             &(userheader), sizeof(DltUserHeader),
                             &(userpayload), sizeof(DltUserControlMsgBufferOverflow));
}

DltReturnValue mct_user_check_buffer(int *total_size, int *used_size)
{
    if ((total_size == NULL) || (used_size == NULL)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DLT_SEM_LOCK();

    *total_size = mct_buffer_get_total_size(&(mct_user.startup_buffer));
    *used_size = mct_buffer_get_used_size(&(mct_user.startup_buffer));

    DLT_SEM_FREE();
    return DLT_RETURN_OK; /* ok */
}


int mct_start_threads(int id)
{
    if ((mct_housekeeperthread_handle == 0) && (id & DLT_USER_HOUSEKEEPER_THREAD)) {
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

DltReturnValue mct_user_log_out_error_handling(void *ptr1, size_t len1,
                                               void *ptr2, size_t len2,
                                               void *ptr3, size_t len3)
{
    DltReturnValue ret = DLT_RETURN_ERROR;
    int msg_size = len1 + len2 + len3;

    DLT_SEM_LOCK();
    ret = mct_buffer_check_size(&(mct_user.startup_buffer), msg_size);
    DLT_SEM_FREE();

    if ((ret != DLT_RETURN_OK) && (mct_user_get_blockmode() == DLT_MODE_BLOCKING)) {
        pthread_mutex_lock(&flush_mutex);
        g_mct_buffer_empty = 0;

        /* block until buffer free */
        g_mct_buffer_full = 1;

        while (g_mct_buffer_full == 1) {
            pthread_cond_wait(&cond_free, &flush_mutex);
        }

        DLT_SEM_LOCK();

        if (mct_buffer_push3(&(mct_user.startup_buffer),
                             ptr1, len1,
                             ptr2, len2,
                             ptr3, len3) != DLT_RETURN_OK) {
            if (mct_user.overflow_counter == 0) {
                mct_log(LOG_CRIT,
                        "Buffer full! Messages will be discarded in BLOCKING.\n");
            }

            ret = DLT_RETURN_BUFFER_FULL;
        }

        DLT_SEM_FREE();

        pthread_mutex_unlock(&flush_mutex);
    } else {
        DLT_SEM_LOCK();

        if (mct_buffer_push3(&(mct_user.startup_buffer),
                             ptr1, len1,
                             ptr2, len2,
                             ptr3, len3) != DLT_RETURN_OK) {
            if (mct_user.overflow_counter == 0) {
                mct_log(LOG_WARNING,
                        "Buffer full! Messages will be discarded.\n");
            }

            ret = DLT_RETURN_BUFFER_FULL;
        }

        DLT_SEM_FREE();

        pthread_mutex_lock(&flush_mutex);
        g_mct_buffer_empty = 0;
        pthread_mutex_unlock(&flush_mutex);
    }

    return ret;
}

static DltReturnValue mct_user_set_blockmode(int8_t mode)
{
    if ((mode < DLT_MODE_NON_BLOCKING) || (mode > DLT_MODE_BLOCKING)) {
        return DLT_RETURN_WRONG_PARAMETER;
    }

    DLT_SEM_LOCK();
    mct_user.block_mode = mode;
    DLT_SEM_FREE();

    return DLT_RETURN_OK;
}

static int mct_user_get_blockmode()
{
    int bm;
    DLT_SEM_LOCK();
    bm = mct_user.block_mode;
    DLT_SEM_FREE();
    return bm;
}
