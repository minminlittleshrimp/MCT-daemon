#ifndef DLT_USER_CFG_H
#define DLT_USER_CFG_H

/*************/
/* Changable */
/*************/

/* Size of receive buffer */
#define DLT_USER_RCVBUF_MAX_SIZE 10024

/* Size of ring buffer */
#define DLT_USER_RINGBUFFER_MIN_SIZE   50000
#define DLT_USER_RINGBUFFER_MAX_SIZE  500000
#define DLT_USER_RINGBUFFER_STEP_SIZE  50000

/* Name of environment variable for ringbuffer configuration */
#define DLT_USER_ENV_BUFFER_MIN_SIZE  "DLT_USER_BUFFER_MIN"
#define DLT_USER_ENV_BUFFER_MAX_SIZE  "DLT_USER_BUFFER_MAX"
#define DLT_USER_ENV_BUFFER_STEP_SIZE "DLT_USER_BUFFER_STEP"

/* Temporary buffer length */
#define DLT_USER_BUFFER_LENGTH               255

/* Number of context entries, which will be allocated,
 * if no more context entries are available */
#define DLT_USER_CONTEXT_ALLOC_SIZE          500

/* Maximu length of a filename string */
#define DLT_USER_MAX_FILENAME_LENGTH         255

/* Maximum length of a single version number */
#define DLT_USER_MAX_LIB_VERSION_LENGTH        3

/* Length of buffer for constructing text output */
#define DLT_USER_TEXT_LENGTH                10024

/* Stack size of receiver thread */
#define DLT_USER_RECEIVERTHREAD_STACKSIZE 100000

/* default value for storage to file, not used in daemon connection */
#define DLT_USER_DEFAULT_ECU_ID "ECU1"

/* Initial log level */
#define DLT_USER_INITIAL_LOG_LEVEL    DLT_LOG_INFO

/* Initial trace status */
#define DLT_USER_INITIAL_TRACE_STATUS DLT_TRACE_STATUS_OFF

/* send always session id: 0 - don't use, 1 - use */
#define DLT_USER_WITH_SESSION_ID 1

/* send always timestamp: 0 - don't use, 1 - use */
#define DLT_USER_WITH_TIMESTAMP 1

/* send always ecu id: 0 - don't use, 1 - use */
#define DLT_USER_WITH_ECU_ID 1

/* default message id for non-verbose mode, if no message id was provided */
#define DLT_USER_DEFAULT_MSGID 0xffff

/* delay for receiver thread (msec) */
#define DLT_USER_RECEIVE_MDELAY 500

/* delay for receiver thread (nsec) */
#define DLT_USER_RECEIVE_NDELAY (DLT_USER_RECEIVE_MDELAY * 1000 * 1000)

/* Name of environment variable for local print mode */
#define DLT_USER_ENV_LOCAL_PRINT_MODE "DLT_LOCAL_PRINT_MODE"

/* Name of environment variable to force block mode */
#define DLT_USER_ENV_FORCE_BLOCK_MODE "DLT_FORCE_BLOCKING"

/* Timeout offset for resending user buffer at exit in 10th milliseconds (10000 = 1s)*/
#define DLT_USER_ATEXIT_RESEND_BUFFER_EXIT_TIMEOUT 0

/* Sleeps between resending user buffer at exit in nsec (1000000 nsec = 1ms)*/
#define DLT_USER_ATEXIT_RESEND_BUFFER_SLEEP 100000000

/* Name of environment variable to disable extended header in non verbose mode */
#define DLT_USER_ENV_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE \
    "DLT_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE"

typedef enum
{
    DLT_USER_NO_USE_EXTENDED_HEADER_FOR_NONVERBOSE = 0,
    DLT_USER_USE_EXTENDED_HEADER_FOR_NONVERBOSE
} DltExtHeaderNonVer;

/* Retry interval for mq error in usec */
#define DLT_USER_MQ_ERROR_RETRY_INTERVAL 100000


/* Name of environment variable to change the mct log message buffer size */
#define DLT_USER_ENV_LOG_MSG_BUF_LEN "DLT_LOG_MSG_BUF_LEN"

/* Maximum msg size as per autosar standard */
#define DLT_LOG_MSG_BUF_MAX_SIZE 65535

/* Name of environment variable for specifying the APPID */
#define DLT_USER_ENV_APP_ID "DLT_APP_ID"

/* Name of environment variable for disabling the injection message at libmct */
#define DLT_USER_ENV_DISABLE_INJECTION_MSG "DLT_DISABLE_INJECTION_MSG_AT_USER"

/************************/
/* Don't change please! */
/************************/

/* Minimum valid ID of an injection message */
#define DLT_USER_INJECTION_MIN      0xFFF

/* Defines of the different local print modes */
#define DLT_PM_UNSET     0
#define DLT_PM_AUTOMATIC 1
#define    DLT_PM_FORCE_ON  2
#define    DLT_PM_FORCE_OFF 3

#endif /* DLT_USER_CFG_H */
