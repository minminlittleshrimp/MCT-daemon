#ifndef MCT_USER_CFG_H
#define MCT_USER_CFG_H

/*************/
/* Changable */
/*************/

/* Size of receive buffer */
#define MCT_USER_RCVBUF_MAX_SIZE 10024

/* Size of ring buffer */
#define MCT_USER_RINGBUFFER_MIN_SIZE   50000
#define MCT_USER_RINGBUFFER_MAX_SIZE  500000
#define MCT_USER_RINGBUFFER_STEP_SIZE  50000

/* Name of environment variable for ringbuffer configuration */
#define MCT_USER_ENV_BUFFER_MIN_SIZE  "MCT_USER_BUFFER_MIN"
#define MCT_USER_ENV_BUFFER_MAX_SIZE  "MCT_USER_BUFFER_MAX"
#define MCT_USER_ENV_BUFFER_STEP_SIZE "MCT_USER_BUFFER_STEP"

/* Temporary buffer length */
#define MCT_USER_BUFFER_LENGTH               255

/* Number of context entries, which will be allocated,
 * if no more context entries are available */
#define MCT_USER_CONTEXT_ALLOC_SIZE          500

/* Maximu length of a filename string */
#define MCT_USER_MAX_FILENAME_LENGTH         255

/* Maximum length of a single version number */
#define MCT_USER_MAX_LIB_VERSION_LENGTH        3

/* Length of buffer for constructing text output */
#define MCT_USER_TEXT_LENGTH                10024

/* Stack size of receiver thread */
#define MCT_USER_RECEIVERTHREAD_STACKSIZE 100000

/* default value for storage to file, not used in daemon connection */
#define MCT_USER_DEFAULT_ECU_ID "ECU1"

/* Initial log level */
#define MCT_USER_INITIAL_LOG_LEVEL    MCT_LOG_INFO

/* Initial trace status */
#define MCT_USER_INITIAL_TRACE_STATUS MCT_TRACE_STATUS_OFF

/* send always session id: 0 - don't use, 1 - use */
#define MCT_USER_WITH_SESSION_ID 1

/* send always timestamp: 0 - don't use, 1 - use */
#define MCT_USER_WITH_TIMESTAMP 1

/* send always ecu id: 0 - don't use, 1 - use */
#define MCT_USER_WITH_ECU_ID 1

/* default message id for non-verbose mode, if no message id was provided */
#define MCT_USER_DEFAULT_MSGID 0xffff

/* delay for receiver thread (msec) */
#define MCT_USER_RECEIVE_MDELAY 500

/* delay for receiver thread (nsec) */
#define MCT_USER_RECEIVE_NDELAY (MCT_USER_RECEIVE_MDELAY * 1000 * 1000)

/* Name of environment variable for local print mode */
#define MCT_USER_ENV_LOCAL_PRINT_MODE "MCT_LOCAL_PRINT_MODE"

/* Name of environment variable to force block mode */
#define MCT_USER_ENV_FORCE_BLOCK_MODE "MCT_FORCE_BLOCKING"

/* Timeout offset for resending user buffer at exit in 10th milliseconds (10000 = 1s)*/
#define MCT_USER_ATEXIT_RESEND_BUFFER_EXIT_TIMEOUT 0

/* Sleeps between resending user buffer at exit in nsec (1000000 nsec = 1ms)*/
#define MCT_USER_ATEXIT_RESEND_BUFFER_SLEEP 100000000

/* Name of environment variable to disable extended header in non verbose mode */
#define MCT_USER_ENV_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE \
    "MCT_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE"

typedef enum
{
    MCT_USER_NO_USE_EXTENDED_HEADER_FOR_NONVERBOSE = 0,
    MCT_USER_USE_EXTENDED_HEADER_FOR_NONVERBOSE
} MctExtHeaderNonVer;

/* Retry interval for mq error in usec */
#define MCT_USER_MQ_ERROR_RETRY_INTERVAL 100000


/* Name of environment variable to change the mct log message buffer size */
#define MCT_USER_ENV_LOG_MSG_BUF_LEN "MCT_LOG_MSG_BUF_LEN"

/* Maximum msg size as per autosar standard */
#define MCT_LOG_MSG_BUF_MAX_SIZE 65535

/* Name of environment variable for specifying the APPID */
#define MCT_USER_ENV_APP_ID "MCT_APP_ID"

/* Name of environment variable for disabling the injection message at libmct */
#define MCT_USER_ENV_DISABLE_INJECTION_MSG "MCT_DISABLE_INJECTION_MSG_AT_USER"

/************************/
/* Don't change please! */
/************************/

/* Minimum valid ID of an injection message */
#define MCT_USER_INJECTION_MIN      0xFFF

/* Defines of the different local print modes */
#define MCT_PM_UNSET     0
#define MCT_PM_AUTOMATIC 1
#define    MCT_PM_FORCE_ON  2
#define    MCT_PM_FORCE_OFF 3

#endif /* MCT_USER_CFG_H */
