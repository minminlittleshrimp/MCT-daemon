#ifndef MCT_USER_H
#   define MCT_USER_H

#ifndef MCT_NETWORK_TRACE_ENABLE
#cmakedefine MCT_NETWORK_TRACE_ENABLE
#endif

#cmakedefine01 MCT_DISABLE_MACRO

#ifdef MCT_NETWORK_TRACE_ENABLE
#   include <mqueue.h>
#else
#    include <sys/types.h>
#    include <fcntl.h>
#endif

#   include <pthread.h>

#   if !defined (__WIN32__)
#      include <semaphore.h>
#   endif

#   include "mct_types.h"

#   ifdef __cplusplus
extern "C" {
#   endif

#   define MCT_USER_BUF_MAX_SIZE 1390            /**< maximum size of each user buffer, also used for injection buffer */

#   define MCT_USER_RESENDBUF_MAX_SIZE (MCT_USER_BUF_MAX_SIZE + 100)    /**< Size of resend buffer; Max MCT message size is 1390 bytes plus some extra header space  */

/* Use a semaphore or mutex from your OS to prevent concurrent access to the MCT buffer. */
#define MCT_SEM_LOCK() do{\
    while ((sem_wait(&mct_mutex) == -1) && (errno == EINTR)) \
        continue;       /* Restart if interrupted */ \
    } while(0)
#define MCT_SEM_FREE() { sem_post(&mct_mutex); }

/**
 * This structure is used for every context used in an application.
 */
typedef struct
{
    char contextID[MCT_ID_SIZE];                  /**< context id */
    int32_t log_level_pos;                        /**< offset in user-application context field */
    int8_t *log_level_ptr;                        /**< pointer to the log level */
    int8_t *trace_status_ptr;                     /**< pointer to the trace status */
    uint8_t mcnt;                                 /**< message counter */
} DltContext;

/**
 * This structure is used for context data used in an application.
 */
typedef struct
{
    DltContext *handle;                           /**< pointer to DltContext */
    unsigned char *buffer;                        /**< buffer for building log message*/
    int32_t size;                                 /**< payload size */
    int32_t log_level;                            /**< log level */
    int32_t trace_status;                         /**< trace status */
    int32_t args_num;                             /**< number of arguments for extended header*/
    char *context_description;                    /**< description of context */
    DltTimestampType use_timestamp;               /**< whether to use user-supplied timestamps */
    uint32_t user_timestamp;                      /**< user-supplied timestamp to use */
    int8_t verbose_mode;                          /**< verbose mode: 1 enabled, 0 disabled */
} DltContextData;

typedef struct
{
    uint32_t service_id;
    int (*injection_callback)(uint32_t service_id, void *data, uint32_t length);
    int (*injection_callback_with_id)(uint32_t service_id, void *data, uint32_t length, void *priv_data);
    void *data;
} DltUserInjectionCallback;

typedef struct
{
    char contextID[MCT_ID_SIZE];      /**< Context ID */
    int8_t log_level;                 /**< Log level */
    int8_t trace_status;              /**< Trace status */
    void (*log_level_changed_callback)(char context_id[MCT_ID_SIZE], uint8_t log_level, uint8_t trace_status);
} DltUserLogLevelChangedCallback;

/**
 * This structure is used in a table managing all contexts and the corresponding log levels in an application.
 */
typedef struct
{
    char contextID[MCT_ID_SIZE];      /**< Context ID */
    int8_t log_level;                 /**< Log level */
    int8_t *log_level_ptr;             /**< Ptr to the log level */
    int8_t trace_status;              /**< Trace status */
    int8_t *trace_status_ptr;             /**< Ptr to the trace status */
    char *context_description;        /**< description of context */
    DltUserInjectionCallback *injection_table; /**< Table with pointer to injection functions and service ids */
    uint32_t nrcallbacks;

    /* Log Level changed callback */
    void (*log_level_changed_callback)(char context_id[MCT_ID_SIZE], uint8_t log_level, uint8_t trace_status);

} mct_ll_ts_type;

/**
 * @brief holds initial log-level for given appId:ctxId pair
 */
typedef struct
{
    char appId[MCT_ID_SIZE];
    char ctxId[MCT_ID_SIZE];
    int8_t ll;
} mct_env_ll_item;


/**
 * @brief holds all initial log-levels given via environment variable MCT_INITIAL_LOG_LEVEL
 */
typedef struct
{
    mct_env_ll_item *item;
    size_t array_size;
    size_t num_elem;
} mct_env_ll_set;


/**
 * This structure is used once for one application.
 */
typedef struct
{
    char ecuID[MCT_ID_SIZE];                   /**< ECU ID */
    char appID[MCT_ID_SIZE];                   /**< Application ID */
    int mct_log_handle;                        /**< Handle to fifo of mct daemon */
    int mct_user_handle;                       /**< Handle to own fifo */
#ifdef MCT_NETWORK_TRACE_ENABLE
    mqd_t mct_segmented_queue_read_handle;     /**< Handle message queue */
    mqd_t mct_segmented_queue_write_handle;    /**< Handle message queue */
    pthread_t mct_segmented_nwt_handle;        /**< thread handle of segmented sending */
#endif
    int8_t mct_is_file;                        /**< Target of logging: 1 to file, 0 to daemon */
    unsigned int filesize_max;                 /**< Maximum size of existing file in case mct_is_file=1 */

    mct_ll_ts_type *mct_ll_ts;                 /** [MAX_MCT_LL_TS_ENTRIES]; < Internal management struct for all
                                                *  contexts */
    uint32_t mct_ll_ts_max_num_entries;        /**< Maximum number of contexts */

    uint32_t mct_ll_ts_num_entries;            /**< Number of used contexts */

    int8_t overflow;                           /**< Overflow marker, set to 1 on overflow, 0 otherwise */
    uint32_t overflow_counter;                 /**< Counts the number of lost messages */

    char *application_description;             /**< description of application */

    DltReceiver receiver;                      /**< Receiver for internal user-defined messages from daemon */

    int8_t verbose_mode;                       /**< Verbose mode enabled: 1 enabled, 0 disabled */
    int8_t use_extended_header_for_non_verbose; /**< Use extended header for non verbose: 1 enabled, 0 disabled */
    int8_t with_session_id;                    /**< Send always session id: 1 enabled, 0 disabled */
    int8_t with_timestamp;                     /**< Send always timestamp: 1 enabled, 0 disabled */
    int8_t with_ecu_id;                        /**< Send always ecu id: 1 enabled, 0 disabled */

    int8_t enable_local_print;                 /**< Local printing of log messages: 1 enabled, 0 disabled */
    int8_t local_print_mode;                   /**< Local print mode, controlled by environment variable */
    int8_t disable_injection_msg;               /**< Injection msg availability: 1 disabled, 0 enabled (default) */

    int8_t log_state;                          /**< Log state of external connection:
                                                * 1 client connected,
                                                * 0 not connected,
                                                * -1 unknown */

    DltBuffer startup_buffer; /**< Ring-buffer for buffering messages during startup and missing connection */
    /* Buffer used for resending, locked by MCT semaphore */
    uint8_t *resend_buffer;

    uint32_t timeout_at_exit_handler; /**< timeout used in mct_user_atexit_blow_out_user_buffer, in 0.1 milliseconds */
    mct_env_ll_set initial_ll_set;

#   ifdef MCT_SHM_ENABLE
    DltShm mct_shm;
#   endif
#   ifdef MCT_TEST_ENABLE
    int corrupt_user_header;
    int corrupt_message_size;
    int16_t corrupt_message_size_size;
#   endif
#   if defined MCT_LIB_USE_UNIX_SOCKET_IPC || defined MCT_LIB_USE_VSOCK_IPC
    DltUserConnectionState connection_state;
#   endif
    uint16_t log_buf_len;        /**< length of message buffer, by default: MCT_USER_BUF_MAX_SIZE */
} DltUser;

typedef int (*mct_injection_callback_id)(uint32_t, void *, uint32_t, void *);
typedef int (*mct_injection_callback)(uint32_t, void *, uint32_t);

/**************************************************************************************************
* The following API functions define a low level function interface for MCT
**************************************************************************************************/

/**
 * Initialize the generation of a MCT log message (intended for usage in verbose mode)
 * This function has to be called first, when an application wants to send a new log messages.
 * Following functions like mct_user_log_write_string and mct_user_log_write_finish must only be called,
 * when return value is bigger than zero.
 * @param handle pointer to an object containing information about one special logging context
 * @param log pointer to an object containing information about logging context data
 * @param loglevel this is the current log level of the log message to be sent
 * @return Value from mct_return_value enum, MCT_RETURN_TRUE if log level is matching
 */
mct_return_value mct_user_log_write_start(DltContext *handle, DltContextData *log, DltLogLevelType loglevel);

/**
 * Initialize the generation of a MCT log message (intended for usage in non-verbose mode)
 * This function has to be called first, when an application wants to send a new log messages.
 * Following functions like mct_user_log_write_string and mct_user_log_write_finish must only be called,
 * when return value is bigger than zero.
 * @param handle pointer to an object containing information about one special logging context
 * @param log pointer to an object containing information about logging context data
 * @param loglevel this is the current log level of the log message to be sent
 * @param messageid message id of message
 * @return Value from mct_return_value enum, MCT_RETURN_TRUE if log level is matching
 */
mct_return_value mct_user_log_write_start_id(DltContext *handle,
                                           DltContextData *log,
                                           DltLogLevelType loglevel,
                                           uint32_t messageid);

/**
 * Initialize the generation of a MCT log message with given buffer from MCT application.
 * This can be considered as replacement of mct_user_log_write_start/mct_user_log_write_start_id
 * and other data functions like mct_user_log_write_string. The fourth, fifth, and sixth arguments
 * shall be prepared by MCT application; this function is only responsible for checking log
 * level and setting the given values to context data. This function has to be called first,
 * when an application is ready to send a new log message with given buffer. This function only
 * works with combination of mct_user_log_write_finish_w_given_buffer and the function must only be
 * called, when return value is bigger than zero. The function only supports verbose mode as of now.
 * @param handle pointer to an object containing information about one special logging context
 * @param log pointer to an object containing information about logging context data
 * @param loglevel this is the current log level of the log message to be sent
 * @param buffer data with log message
 * @param size buffer size
 * @param args_num number of arguments in buffer
 * @return Value from mct_return_value enum, MCT_RETURN_TRUE if log level is matching
 */
mct_return_value mct_user_log_write_start_w_given_buffer(DltContext *handle,
                                                       DltContextData *log,
                                                       DltLogLevelType loglevel,
                                                       char *buffer,
                                                       size_t size,
                                                       int32_t args_num);

/**
 * Finishing the generation of a MCT log message and sending it to the MCT daemon.
 * This function has to be called after writing all the log attributes of a log message.
 * @param log pointer to an object containing information about logging context data
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_finish(DltContextData *log);

/**
 * Finishing the generation of a MCT log message and sending it to the MCT daemon without
 * freeing log buffer. This function only works with combination of
 * mct_user_log_write_start_w_given_buffer. This function has to be called after writing all
 * the log attributes of a log message.
 * @param log pointer to an object containing information about logging context data
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_finish_w_given_buffer(DltContextData *log);

/**
 * Write a boolean parameter into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data boolean parameter written into log message (mapped to uint8)
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_bool(DltContextData *log, uint8_t data);

/**
 * Write a boolean parameter with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  boolean parameter written into log message (mapped to uint8)
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_bool_attr(DltContextData *log, uint8_t data, const char *name);

/**
 * Write a float parameter into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data float32_t parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_float32(DltContextData *log, float32_t data);

/**
 * Write a double parameter into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data float64_t parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_float64(DltContextData *log, double data);

/**
 * Write a float parameter with attributes into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name or @a unit is NULL, this function will add a corresponding attribute field with length 0
 * and no content to the message for that attribute.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  float32_t parameter written into log message
 * @param name  the "name" attribute (or NULL)
 * @param unit  the "unit" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_float32_attr(mct_context_data *log, float32_t data, const char *name, const char *unit);

/**
 * Write a double parameter with attributes into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name or @a unit is NULL, this function will add a corresponding attribute field with length 0
 * and no content to the message for that attribute.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  float64_t parameter written into log message
 * @param name  the "name" attribute (or NULL)
 * @param unit  the "unit" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_float64_attr(DltContextData *log, float64_t data, const char *name, const char *unit);

/**
 * Write a uint parameter into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data unsigned int parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_uint(DltContextData *log, unsigned int data);
mct_return_value mct_user_log_write_uint8(DltContextData *log, uint8_t data);
mct_return_value mct_user_log_write_uint16(DltContextData *log, uint16_t data);
mct_return_value mct_user_log_write_uint32(DltContextData *log, uint32_t data);
mct_return_value mct_user_log_write_uint64(DltContextData *log, uint64_t data);

/**
 * Write a uint parameter with attributes into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name or @a unit is NULL, this function will add a corresponding attribute field with length 0
 * and no content to the message for that attribute.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  unsigned int parameter written into log message
 * @param name  the "name" attribute (or NULL)
 * @param unit  the "unit" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_uint_attr(DltContextData *log, unsigned int data, const char *name, const char *unit);
mct_return_value mct_user_log_write_uint8_attr(DltContextData *log, uint8_t data, const char *name, const char *unit);
mct_return_value mct_user_log_write_uint16_attr(DltContextData *log, uint16_t data, const char *name, const char *unit);
mct_return_value mct_user_log_write_uint32_attr(DltContextData *log, uint32_t data, const char *name, const char *unit);
mct_return_value mct_user_log_write_uint64_attr(DltContextData *log, uint64_t data, const char *name, const char *unit);

/**
 * Write a uint parameter into a MCT log message. The output will be formatted as given by the parameter type.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data unsigned int parameter written into log message.
 * @param type The formatting type of the string output.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_uint8_formatted(DltContextData *log, uint8_t data, DltFormatType type);
mct_return_value mct_user_log_write_uint16_formatted(DltContextData *log, uint16_t data, DltFormatType type);
mct_return_value mct_user_log_write_uint32_formatted(DltContextData *log, uint32_t data, DltFormatType type);
mct_return_value mct_user_log_write_uint64_formatted(DltContextData *log, uint64_t data, DltFormatType type);

/**
 * Write a pointer value architecture independent.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data void* parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_ptr(DltContextData *log, void *data);

/**
 * Write a int parameter into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data int parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_int(DltContextData *log, int data);
mct_return_value mct_user_log_write_int8(DltContextData *log, int8_t data);
mct_return_value mct_user_log_write_int16(DltContextData *log, int16_t data);
mct_return_value mct_user_log_write_int32(DltContextData *log, int32_t data);
mct_return_value mct_user_log_write_int64(DltContextData *log, int64_t data);

/**
 * Write an int parameter with attributes into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name or @a unit is NULL, this function will add a corresponding attribute field with length 0
 * and no content to the message for that attribute.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  int parameter written into log message
 * @param name  the "name" attribute (or NULL)
 * @param unit  the "unit" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_int_attr(DltContextData *log, int data, const char *name, const char *unit);
mct_return_value mct_user_log_write_int8_attr(DltContextData *log, int8_t data, const char *name, const char *unit);
mct_return_value mct_user_log_write_int16_attr(DltContextData *log, int16_t data, const char *name, const char *unit);
mct_return_value mct_user_log_write_int32_attr(DltContextData *log, int32_t data, const char *name, const char *unit);
mct_return_value mct_user_log_write_int64_attr(DltContextData *log, int64_t data, const char *name, const char *unit);

/**
 * Write a null terminated ASCII string into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message containing null termination.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_string(DltContextData *log, const char *text);

/**
 * Write a potentially non-null-terminated ASCII string into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message
 * @param length length in bytes of @a text (without any termination character)
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_string(DltContextData *log, const char *text, uint16_t length);

/**
 * Write a constant null terminated ASCII string into a MCT log message.
 * In non verbose mode MCT parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message containing null termination.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_constant_string(DltContextData *log, const char *text);

/**
 * Write a constant, potentially non-null-terminated ASCII string into a MCT log message.
 * In non verbose mode MCT parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message
 * @param length length in bytes of @a text (without any termination character)
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_constant_string(DltContextData *log, const char *text, uint16_t length);

/**
 * Write a null terminated UTF8 string into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message containing null termination.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_utf8_string(DltContextData *log, const char *text);

/**
 * Write a potentially non-null-terminated UTF8 string into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message
 * @param length length in bytes of @a text (without any termination character)
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_utf8_string(DltContextData *log, const char *text, uint16_t length);

/**
 * Write a constant null terminated UTF8 string into a MCT log message.
 * In non verbose mode MCT parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message containing null termination.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_constant_utf8_string(DltContextData *log, const char *text);

/**
 * Write a constant, potentially non-null-terminated UTF8 string into a MCT log message.
 * In non verbose mode MCT parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param text pointer to the parameter written into log message
 * @param length length in bytes of @a text (without any termination character)
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_constant_utf8_string(DltContextData *log, const char *text, uint16_t length);

/**
 * Write a null-terminated ASCII string with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message containing null termination
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_string_attr(DltContextData *log, const char *text, const char *name);

/**
 * Write a potentially non-null-terminated ASCII string with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message
 * @param length  length in bytes of @a text (without any termination character)
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name);

/**
 * Write a constant, null-terminated ASCII string with "name" attribute into a MCT log message.
 * In non-verbose mode, this parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message containing null termination
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_constant_string_attr(DltContextData *log, const char *text, const char *name);

/**
 * Write a constant, potentially non-null-terminated ASCII string with "name" attribute into a MCT log message.
 * In non-verbose mode, this parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message
 * @param length  length in bytes of @a text (without any termination character)
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_constant_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name);

/**
 * Write a null-terminated UTF-8 string with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message containing null termination
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_utf8_string_attr(DltContextData *log, const char *text, const char *name);

/**
 * Write a potentially non-null-terminated UTF-8 string with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message
 * @param length  length in bytes of @a text (without any termination character)
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_utf8_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name);

/**
 * Write a constant, null-terminated UTF8 string with "name" attribute into a MCT log message.
 * In non-verbose mode, this parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message containing null termination
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_constant_utf8_string_attr(DltContextData *log, const char *text, const char *name);

/**
 * Write a constant, potentially non-null-terminated UTF8 string with "name" attribute into a MCT log message.
 * In non-verbose mode, this parameter will not be sent at all.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param text  pointer to the parameter written into log message
 * @param length  length in bytes of @a text (without any termination character)
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_sized_constant_utf8_string_attr(DltContextData *log, const char *text, uint16_t length, const char *name);

/**
 * Write a binary memory block into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data pointer to the parameter written into log message.
 * @param length length in bytes of the parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_raw(DltContextData *log, void *data, uint16_t length);

/**
 * Write a binary memory block into a MCT log message.
 * mct_user_log_write_start has to be called before adding any attributes to the log message.
 * Finish sending log message by calling mct_user_log_write_finish.
 * @param log pointer to an object containing information about logging context data
 * @param data pointer to the parameter written into log message.
 * @param length length in bytes of the parameter written into log message.
 * @param type the format information.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_write_raw_formatted(DltContextData *log, void *data, uint16_t length, DltFormatType type);

/**
 * Write a binary memory block with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  pointer to the parameter written into log message.
 * @param length  length in bytes of the parameter written into log message
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_raw_attr(DltContextData *log, const void *data, uint16_t length, const char *name);

/**
 * Write a binary memory block with "name" attribute into a MCT log message.
 * mct_user_log_write_start has to be called before adding any parameters to the log message.
 * Finish building a log message by calling mct_user_log_write_finish.
 *
 * If @a name is NULL, this function will add an attribute field with length 0
 * and no content to the message.
 *
 * @param log  pointer to an object containing information about logging context data
 * @param data  pointer to the parameter written into log message.
 * @param length  length in bytes of the parameter written into log message
 * @param type  the format information
 * @param name  the "name" attribute (or NULL)
 * @return value from mct_return_value enum
 */
mct_return_value mct_user_log_write_raw_formatted_attr(DltContextData *log, const void *data, uint16_t length, DltFormatType type, const char *name);

/**
 * Trace network message
 * @param handle pointer to an object containing information about one special logging context
 * @param nw_trace_type type of network trace (MCT_NW_TRACE_IPC, MCT_NW_TRACE_CAN, MCT_NW_TRACE_FLEXRAY, or MCT_NW_TRACE_MOST)
 * @param header_len length of network message header
 * @param header pointer to network message header
 * @param payload_len length of network message payload
 * @param payload pointer to network message payload
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_trace_network(DltContext *handle,
                                      DltNetworkTraceType nw_trace_type,
                                      uint16_t header_len,
                                      void *header,
                                      uint16_t payload_len,
                                      void *payload);

/**
 * Trace network message, truncated if necessary.
 * @param handle pointer to an object containing information about logging context
 * @param nw_trace_type type of network trace (MCT_NW_TRACE_IPC, MCT_NW_TRACE_CAN, MCT_NW_TRACE_FLEXRAY, or MCT_NW_TRACE_MOST)
 * @param header_len length of network message header
 * @param header pointer to network message header
 * @param payload_len length of network message payload
 * @param payload pointer to network message payload
 * @param allow_truncate Set to > 0 to allow truncating of the message if it is too large.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_trace_network_truncated(DltContext *handle,
                                                DltNetworkTraceType nw_trace_type,
                                                uint16_t header_len,
                                                void *header,
                                                uint16_t payload_len,
                                                void *payload,
                                                int allow_truncate);

/**
 * Trace network message in segmented asynchronous mode.
 * The sending of the data is done in a separate thread.
 * Please note that handle must exist for the lifetime of the application, because
 * data chunks are sent asynchronously in undetermined future time.
 * @param handle pointer to an object containing information about logging context
 * @param nw_trace_type type of network trace (MCT_NW_TRACE_IPC, MCT_NW_TRACE_CAN, MCT_NW_TRACE_FLEXRAY, or MCT_NW_TRACE_MOST)
 * @param header_len length of network message header
 * @param header pointer to network message header
 * @param payload_len length of network message payload
 * @param payload pointer to network message payload
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_trace_network_segmented(DltContext *handle,
                                                DltNetworkTraceType nw_trace_type,
                                                uint16_t header_len,
                                                void *header,
                                                uint16_t payload_len,
                                                void *payload);

/**************************************************************************************************
* The following API functions define a high level function interface for MCT
**************************************************************************************************/

/**
 * Initialize the user lib communication with daemon.
 * This function has to be called first, before using any MCT user lib functions.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_init();

/**
 * Initialize the user lib writing only to file.
 * This function has to be called first, before using any MCT user lib functions.
 * @param name name of an optional log file
 * @return Value from mct_return_value enum
 */
mct_return_value mct_init_file(const char *name);

/**
 * Set maximum file size if lib is configured to write only to file.
 * This function has to be called after mct_init_file().
 * @param filesize maximum file size
 * @return Value from mct_return_value enum
 */
mct_return_value mct_set_filesize_max(unsigned int filesize);

/**
 * Terminate the user lib.
 * This function has to be called when finishing using the MCT user lib.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_free();

/**
 * Check the library version of MCT library.
 * @param user_major_version the major version to be compared
 * @param user_minor_version the minor version to be compared
 * @return Value from mct_return_value enum, MCT_RETURN_ERROR if there is a mismatch
 */
mct_return_value mct_check_library_version(const char *user_major_version, const char *user_minor_version);

/**
 * Register an application in the daemon.
 * @param apid four byte long character array with the application id
 * @param description long name of the application
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_app(const char *apid, const char *description);

/**
 * Unregister an application in the daemon.
 * This function has to be called when finishing using an application.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_unregister_app(void);

/**
 * Unregister an application in the daemon and also flushes the buffered logs.
 * This function has to be called when finishing using an application.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_unregister_app_flush_buffered_logs(void);

/**
 * Get the application id
 * @param four byte long character array to store the application id
 * @return Value from mct_return_value enum
 */
mct_return_value mct_get_appid(char *appid);

/**
 * Register a context in the daemon.
 * This function has to be called before first usage of the context.
 * @param handle pointer to an object containing information about one special logging context
 * @param contextid four byte long character array with the context id
 * @param description long name of the context
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_context(DltContext *handle, const char *contextid, const char *description);

/**
 * Register a context in the daemon with pre-defined log level and pre-defined trace status.
 * This function has to be called before first usage of the context.
 * @param handle pointer to an object containing information about one special logging context
 * @param contextid four byte long character array with the context id
 * @param description long name of the context
 * @param loglevel This is the log level to be pre-set for this context
 *        (MCT_LOG_DEFAULT is not allowed here)
 * @param tracestatus This is the trace status to be pre-set for this context
 *        (MCT_TRACE_STATUS_DEFAULT is not allowed here)
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_context_ll_ts(DltContext *handle,
                                          const char *contextid,
                                          const char *description,
                                          int loglevel,
                                          int tracestatus);

/**
 * Register a context in the daemon with log level changed callback fn.
 * This function is introduced to avoid missing of LL change callback during registration
 * @param handle pointer to an object containing information about one special logging context
 * @param contextid four byte long character array with the context id
 * @param description long name of the context
 * @param *mct_log_level_changed_callback This is the fn which will be called when log level is changed
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_context_llccb(DltContext *handle,
                                          const char *contextid,
                                          const char *description,
                                          void (*mct_log_level_changed_callback)(char context_id[MCT_ID_SIZE],
                                                                                 uint8_t log_level,
                                                                                 uint8_t trace_status));

/**
 * Unregister a context in the MCT daemon.
 * This function has to be called when finishing using a context.
 * @param handle pointer to an object containing information about one special logging context
 * @return Value from mct_return_value enum
 */
mct_return_value mct_unregister_context(DltContext *handle);


/**
 * Set maximum timeout for re-sending at exit
 * @param timeout_in_milliseconds maximum time to wait until giving up re-sending, default 10000 (equals to 10 seconds)
 */
int mct_set_resend_timeout_atexit(uint32_t timeout_in_milliseconds);

/**
 * Set the logging mode used by the daemon.
 * The logging mode is stored persistantly by the daemon.
 * @see DltUserLogMode
 * @param mode the new logging mode used by the daemon: off, extern, internal, both.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_set_log_mode(DltUserLogMode mode);

/**
 * Get the state of the connected client to the daemon.
 * The user application gets a message, when client is connected or disconnected.
 * This value contains the last state.
 * It needs some time until the application gets state from the daemon.
 * Until then the state is "unknown state".
 * @return -1 = unknown state, 0 = client not connected, 1 = client connected
 */
int mct_get_log_state();

/**
 * Register callback function called when injection message was received
 * @param handle pointer to an object containing information about one special logging context
 * @param service_id the service id to be waited for
 * @param (*mct_injection_callback) function pointer to callback function
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_injection_callback(DltContext *handle, uint32_t service_id,
                                               int (*mct_injection_callback)(uint32_t service_id,
                                                                             void *data,
                                                                             uint32_t length));

/**
 * Register callback function with private data called when injection message was received
 * @param handle pointer to an object containing information about one special logging context
 * @param service_id the service id to be waited for
 * @param (*mct_injection_callback) function pointer to callback function
 * @param priv private data
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_injection_callback_with_id(DltContext *handle, uint32_t service_id,
                                                       int (*mct_injection_callback)(uint32_t service_id,
                                                                                     void *data,
                                                                                     uint32_t length,
                                                                                     void *priv_data), void *priv);

/**
 * Register callback function called when log level of context was changed
 * @param handle pointer to an object containing information about one special logging context
 * @param (*mct_log_level_changed_callback) function pointer to callback function
 * @return Value from mct_return_value enum
 */
mct_return_value mct_register_log_level_changed_callback(DltContext *handle,
                                                       void (*mct_log_level_changed_callback)(
                                                           char context_id[MCT_ID_SIZE],
                                                           uint8_t log_level,
                                                           uint8_t trace_status));

/**
 * Switch to verbose mode
 * @return Value from mct_return_value enum
 */
mct_return_value mct_verbose_mode(void);

/**
 * Check the version of mct library with library version used of the application.
 * @param user_major_version version number of application - see mct_version.h
 * @param user_minor_version version number of application - see mct_version.h
 *  @return Value from mct_return_value enum, MCT_RETURN_ERROR if there is a mismatch
 */
mct_return_value mct_user_check_library_version(const char *user_major_version, const char *user_minor_version);

/**
 * Switch to non-verbose mode
 *
 * This does not force all messages to be sent as Non-Verbose ones, as that does not make much sense.
 * Instead, it +allows+ the sending of both Verbose and Non-Verbose messages, depending on which APIs
 * are being called.
 */
mct_return_value mct_nonverbose_mode(void);

/**
 * Use extended header in non verbose mode.
 * Enabled by default.
 * @param use_extended_header_for_non_verbose Use extended header for non verbose mode if true
 * @return Value from mct_return_value enum
 */
mct_return_value mct_use_extended_header_for_non_verbose(int8_t use_extended_header_for_non_verbose);

/**
 * Send session id configuration.
 * Enabled by default.
 * @param with_session_id Send session id in each message if enabled
 * @return Value from mct_return_value enum
 */
mct_return_value mct_with_session_id(int8_t with_session_id);

/**
 * Send timestamp configuration.
 * Enabled by default.
 * @param with_timestamp Send timestamp id in each message if enabled
 * @return Value from mct_return_value enum
 */
mct_return_value mct_with_timestamp(int8_t with_timestamp);

/**
 * Send ecu id configuration.
 * Enabled by default.
 * @param with_ecu_id Send ecu id in each message if enabled
 * @return Value from mct_return_value enum
 */
mct_return_value mct_with_ecu_id(int8_t with_ecu_id);

/**
 * Set maximum logged log level and trace status of application
 *
 * @param loglevel This is the log level to be set for the whole application
 * @param tracestatus This is the trace status to be set for the whole application
 * @return Value from mct_return_value enum
 */
mct_return_value mct_set_application_ll_ts_limit(DltLogLevelType loglevel, DltTraceStatusType tracestatus);


/**
 * @brief adjust log-level based on values given through environment
 *
 * Iterate over the set of items, and find the best match.
 * For any item that matches, the one with the highest priority is selected and that
 * log-level is returned.
 *
 * Priorities are determined as follows:
 * - no apid, no ctid only ll given in item: use ll with prio 1
 * - no apid, ctid matches: use ll with prio 2
 * - no ctid, apid matches: use ll with prio 3
 * - apid, ctid matches: use ll with prio 4
 *
 * @param ll_set
 * @param apid
 * @param ctid
 * @param ll
 * If no item matches or in case of error, the original log-level (\param ll) is returned
 */
int mct_env_adjust_ll_from_env(mct_env_ll_set const *const ll_set,
                               char const *const apid,
                               char const *const ctid,
                               int const ll);

/**
 * @brief extract log-level settings from given string
 *
 * Scan \param env for setttings like apid:ctid:log-level and store them
 * in given \param ll_set
 *
 * @param env reference to a string to be parsed, after parsing env will point after the last parse character
 * @param ll_set set of log-level extracted from given string
 *
 * @return 0 on success
 * @return -1 on failure
 */
int mct_env_extract_ll_set(char **const env, mct_env_ll_set *const ll_set);

void mct_env_free_ll_set(mct_env_ll_set *const ll_set);

/**
 * Enable local printing of messages
 * @return Value from mct_return_value enum
 */
mct_return_value mct_enable_local_print(void);

/**
 * Disable local printing of messages
 * @return Value from mct_return_value enum
 */
mct_return_value mct_disable_local_print(void);

/**
 * Write a null terminated ASCII string into a MCT log message.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @param text pointer to the ASCII string written into log message containing null termination.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_string(DltContext *handle, DltLogLevelType loglevel, const char *text);

/**
 * Write a null terminated ASCII string and an integer value into a MCT log message.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @param text pointer to the ASCII string written into log message containing null termination.
 * @param data integer value written into the log message
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_string_int(DltContext *handle, DltLogLevelType loglevel, const char *text, int data);

/**
 * Write a null terminated ASCII string and an unsigned integer value into a MCT log message.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @param text pointer to the ASCII string written into log message containing null termination.
 * @param data unsigned integer value written into the log message
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_string_uint(DltContext *handle, DltLogLevelType loglevel, const char *text, unsigned int data);

/**
 * Write an integer value into a MCT log message.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @param data integer value written into the log message
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_int(DltContext *handle, DltLogLevelType loglevel, int data);

/**
 * Write an unsigned integer value into a MCT log message.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @param data unsigned integer value written into the log message
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_uint(DltContext *handle, DltLogLevelType loglevel, unsigned int data);

/**
 * Write an unsigned integer value into a MCT log message.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @param data pointer to the parameter written into log message.
 * @param length length in bytes of the parameter written into log message.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_raw(DltContext *handle, DltLogLevelType loglevel, void *data, uint16_t length);

/**
 * Write marker message to MCT.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_log_marker();

/**
 * Get the total size and available size of the shared memory buffer between daemon and applications.
 * This information is useful to control the flow control between applications and daemon.
 * For example only 50% of the buffer should be used for file transfer.
 * @param total_size total size of buffer in bytes
 * @param used_size used size of buffer in bytes
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_check_buffer(int *total_size, int *used_size);

/**
 * Try to resend log message in the user buffer. Stops if the mct_uptime is bigger than
 * mct_uptime() + MCT_USER_ATEXIT_RESEND_BUFFER_EXIT_TIMEOUT. A pause between the resending
 * attempts can be defined with MCT_USER_ATEXIT_RESEND_BUFFER_SLEEP
 * @return number of messages in the user buffer
 */
int mct_user_atexit_blow_out_user_buffer(void);

/**
 * Try to resend log message in the user buffer.
 * @return Value from mct_return_value enum
 */
mct_return_value mct_user_log_resend_buffer(void);

/**
 * Checks the log level passed by the log function if enabled for that context or not.
 * This function can be called by applications before generating their logs.
 * Also called before writing new log messages.
 * @param handle pointer to an object containing information about one special logging context
 * @param loglevel this is the current log level of the log message to be sent
 * @return Value from mct_return_value enum, MCT_RETURN_TRUE if log level is enabled
 */
static inline mct_return_value mct_user_is_logLevel_enabled(DltContext *handle, DltLogLevelType loglevel)
{
    if ((loglevel < MCT_LOG_DEFAULT) || (loglevel >= MCT_LOG_MAX))
        return MCT_RETURN_WRONG_PARAMETER;

    if ((handle == NULL) || (handle->log_level_ptr == NULL))
        return MCT_RETURN_WRONG_PARAMETER;

    if ((loglevel <= (DltLogLevelType)(*(handle->log_level_ptr))) && (loglevel != MCT_LOG_OFF))
        return MCT_RETURN_TRUE;

    return MCT_RETURN_LOGGING_DISABLED;
}

#   ifdef MCT_TEST_ENABLE
void mct_user_test_corrupt_user_header(int enable);
void mct_user_test_corrupt_message_size(int enable, int16_t size);
#   endif /* MCT_TEST_ENABLE */

#   ifdef __cplusplus
}
#   endif

/**
 \}
 */

#endif /* MCT_USER_H */
