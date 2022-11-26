#ifndef MCT_USER_MACROS_H
#define MCT_USER_MACROS_H

#include "mct_version.h"
#include "mct_types.h"

/**
 * \defgroup userapi MCT User API
 * \addtogroup userapi
 \{
 */

/**************************************************************************************************
* The folowing macros define a macro interface for MCT
**************************************************************************************************/

/**
 * Create an object for a new context.
 * This macro has to be called first for every.
 * @param CONTEXT object containing information about one special logging context
 * @note To avoid the MISRA warning "Null statement is located close to other code or comments"
 *       remove the semicolon when using the macro.
 *       Example: MCT_DECLARE_CONTEXT(hContext)
 */
#define MCT_DECLARE_CONTEXT(CONTEXT) \
    MctContext CONTEXT;

/**
 * Use an object of a new context created in another module.
 * This macro has to be called first for every.
 * @param CONTEXT object containing information about one special logging context
 * @note To avoid the MISRA warning "Null statement is located close to other code or comments"
 *       remove the semicolon when using the macro.
 *       Example: MCT_IMPORT_CONTEXT(hContext)
 */
#define MCT_IMPORT_CONTEXT(CONTEXT) \
    extern MctContext CONTEXT;

/**
 * Register application.
 * @param APPID application id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 */
#define MCT_REGISTER_APP(APPID, DESCRIPTION) do { \
        (void)mct_check_library_version(_MCT_PACKAGE_MAJOR_VERSION, _MCT_PACKAGE_MINOR_VERSION); \
        (void)mct_register_app(APPID, DESCRIPTION); } while (0)


/**
 * Unregister application.
 */
#define MCT_UNREGISTER_APP() do { \
        (void)mct_unregister_app(); } while (0)

/**
 * Unregister application and flush the logs buffered in startup buffer if any.
 */
#define MCT_UNREGISTER_APP_FLUSH_BUFFERED_LOGS() do { \
        (void)mct_unregister_app_flush_buffered_logs(); } while (0)

/**
 * To Get application ID.
 * @param APPID character pointer of minimum 4 bytes
 */
#define MCT_GET_APPID(APPID) do{\
    mct_get_appid(APPID);} while(0)

/**
 * Register context (with default log level and default trace status)
 * @param CONTEXT object containing information about one special logging context
 * @param CONTEXTID context id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 */
#define MCT_REGISTER_CONTEXT(CONTEXT, CONTEXTID, DESCRIPTION) do { \
        (void)mct_register_context(&(CONTEXT), CONTEXTID, DESCRIPTION); } while (0)

/**
 * Register context with pre-defined log level and pre-defined trace status.
 * @param CONTEXT object containing information about one special logging context
 * @param CONTEXTID context id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 * @param LOGLEVEL log level to be pre-set for this context
 * (MCT_LOG_DEFAULT is not allowed here)
 * @param TRACESTATUS trace status to be pre-set for this context
 * (MCT_TRACE_STATUS_DEFAULT is not allowed here)
 */
#define MCT_REGISTER_CONTEXT_LL_TS(CONTEXT, CONTEXTID, DESCRIPTION, LOGLEVEL, TRACESTATUS) do { \
        (void)mct_register_context_ll_ts(&(CONTEXT), CONTEXTID, DESCRIPTION, LOGLEVEL, TRACESTATUS); } while (0)

/**
 * Register context (with default log level and default trace status and log level change callback)
 * @param CONTEXT object containing information about one special logging context
 * @param CONTEXTID context id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 * @param CBK log level change callback to be registered
 */
#define MCT_REGISTER_CONTEXT_LLCCB(CONTEXT, CONTEXTID, DESCRIPTION, CBK) do { \
        (void)mct_register_context_llccb(&(CONTEXT), CONTEXTID, DESCRIPTION, CBK); } while (0)

/**
 * Unregister context.
 * @param CONTEXT object containing information about one special logging context
 */
#define MCT_UNREGISTER_CONTEXT(CONTEXT) do { \
        (void)mct_unregister_context(&(CONTEXT)); } while (0)

/**
 * Register callback function called when injection message was received
 * @param CONTEXT object containing information about one special logging context
 * @param SERVICEID service id of the injection message
 * @param CALLBACK function pointer to callback function
 */
#define MCT_REGISTER_INJECTION_CALLBACK(CONTEXT, SERVICEID, CALLBACK) do { \
        (void)mct_register_injection_callback(&(CONTEXT), SERVICEID, CALLBACK); } while (0)

/**
 * Register callback function called when injection message was received
 * @param CONTEXT object containing information about one special logging context
 * @param SERVICEID service id of the injection message
 * @param CALLBACK function pointer to callback function
 * @param PRIV_DATA data specific to context
 */
#define MCT_REGISTER_INJECTION_CALLBACK_WITH_ID(CONTEXT, SERVICEID, CALLBACK, PRIV_DATA) do { \
        (void)mct_register_injection_callback_with_id(&(CONTEXT), SERVICEID, CALLBACK, PRIV_DATA); } while (0)

/**
 * Register callback function called when log level of context was changed
 * @param CONTEXT object containing information about one special logging context
 * @param CALLBACK function pointer to callback function
 */
#define MCT_REGISTER_LOG_LEVEL_CHANGED_CALLBACK(CONTEXT, CALLBACK) do { \
        (void)mct_register_log_level_changed_callback(&(CONTEXT), CALLBACK); } while (0)

/**
 * Send log message with variable list of messages (intended for verbose mode)
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param ... variable list of arguments
 * @note To avoid the MISRA warning "The comma operator has been used outside a for statement"
 *       use a semicolon instead of a comma to separate the __VA_ARGS__.
 *       Example: MCT_LOG(hContext, MCT_LOG_INFO, MCT_STRING("Hello world"); MCT_INT(123));
 */
#ifdef _MSC_VER
/* MCT_LOG is not supported by MS Visual C++ */
/* use function interface instead            */
#else
#   define MCT_LOG(CONTEXT, LOGLEVEL, ...) \
    do { \
        MctContextData log_local; \
        int mct_local; \
        mct_local = mct_user_log_write_start(&CONTEXT, &log_local, LOGLEVEL); \
        if (mct_local == MCT_RETURN_TRUE) \
        { \
            __VA_ARGS__; \
            (void)mct_user_log_write_finish(&log_local); \
        } \
    } while (0)
#endif

/**
 * Send log message with variable list of messages (intended for verbose mode)
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param TS timestamp to be used for log message
 * @param ... variable list of arguments
 * @note To avoid the MISRA warning "The comma operator has been used outside a for statement"
 *       use a semicolon instead of a comma to separate the __VA_ARGS__.
 *       Example: MCT_LOG_TS(hContext, MCT_LOG_INFO, timestamp, MCT_STRING("Hello world"); MCT_INT(123));
 */
#ifdef _MSC_VER
/* MCT_LOG_TS is not supported by MS Visual C++ */
/* use function interface instead            */
#else
#   define MCT_LOG_TS(CONTEXT, LOGLEVEL, TS, ...) \
    do { \
        MctContextData log_local; \
        int mct_local; \
        mct_local = mct_user_log_write_start(&CONTEXT, &log_local, LOGLEVEL); \
        if (mct_local == MCT_RETURN_TRUE) \
        { \
            __VA_ARGS__; \
            log_local.use_timestamp = MCT_USER_TIMESTAMP; \
            log_local.user_timestamp = (uint32_t) TS; \
            (void)mct_user_log_write_finish(&log_local); \
        } \
    } while (0)
#endif

/**
 * Send log message with variable list of messages (intended for non-verbose mode)
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param MSGID the message id of log message
 * @param ... variable list of arguments
 * calls to MCT_STRING(), MCT_BOOL(), MCT_FLOAT32(), MCT_FLOAT64(),
 * MCT_INT(), MCT_UINT(), MCT_RAW()
 * @note To avoid the MISRA warning "The comma operator has been used outside a for statement"
 *       use a semicolon instead of a comma to separate the __VA_ARGS__.
 *       Example: MCT_LOG_ID(hContext, MCT_LOG_INFO, 0x1234, MCT_STRING("Hello world"); MCT_INT(123));
 */
#ifdef _MSC_VER
/* MCT_LOG_ID is not supported by MS Visual C++ */
/* use function interface instead               */
#else
#   define MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ...) \
    do { \
        MctContextData log_local; \
        int mct_local; \
        mct_local = mct_user_log_write_start_id(&CONTEXT, &log_local, LOGLEVEL, MSGID); \
        if (mct_local == MCT_RETURN_TRUE) \
        { \
            __VA_ARGS__; \
            (void)mct_user_log_write_finish(&log_local); \
        } \
    } while (0)
#endif

/**
 * Send log message with variable list of messages (intended for non-verbose mode)
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param MSGID the message id of log message
 * @param TS timestamp to be used for log message
 * @param ... variable list of arguments
 * calls to MCT_STRING(), MCT_BOOL(), MCT_FLOAT32(), MCT_FLOAT64(),
 * MCT_INT(), MCT_UINT(), MCT_RAW()
 * @note To avoid the MISRA warning "The comma operator has been used outside a for statement"
 *       use a semicolon instead of a comma to separate the __VA_ARGS__.
 *       Example: MCT_LOG_ID_TS(hContext, MCT_LOG_INFO, 0x1234, timestamp, MCT_STRING("Hello world"); MCT_INT(123));
 */
#ifdef _MSC_VER
/* MCT_LOG_ID_TS is not supported by MS Visual C++ */
/* use function interface instead               */
#else
#   define MCT_LOG_ID_TS(CONTEXT, LOGLEVEL, MSGID, TS, ...) \
    do { \
        MctContextData log_local; \
        int mct_local; \
        mct_local = mct_user_log_write_start_id(&CONTEXT, &log_local, LOGLEVEL, MSGID); \
        if (mct_local == MCT_RETURN_TRUE) \
        { \
            __VA_ARGS__; \
            log_local.use_timestamp = MCT_USER_TIMESTAMP; \
            log_local.user_timestamp = (uint32_t) TS; \
            (void)mct_user_log_write_finish(&log_local); \
        } \
    } while (0)
#endif

/**
 * Add string parameter to the log messsage.
 * @param TEXT ASCII string
 */
#define MCT_STRING(TEXT) \
    (void)mct_user_log_write_string(&log_local, TEXT)

/**
 * Add string parameter with given length to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT ASCII string
 * @param LEN length in bytes to take from @a TEXT
 */
#define MCT_SIZED_STRING(TEXT, LEN) \
    (void)mct_user_log_write_sized_string(&log_local, TEXT, LEN)

/**
 * Add constant string parameter to the log messsage.
 * @param TEXT Constant ASCII string
 */
#define MCT_CSTRING(TEXT) \
    (void)mct_user_log_write_constant_string(&log_local, TEXT)

/**
 * Add constant string parameter with given length to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT Constant ASCII string
 * @param LEN length in bytes to take from @a TEXT
 */
#define MCT_SIZED_CSTRING(TEXT, LEN) \
    (void)mct_user_log_write_sized_constant_string(&log_local, TEXT, LEN)

/**
 * Add utf8-encoded string parameter to the log messsage.
 * @param TEXT UTF8-encoded string
 */
#define MCT_UTF8(TEXT) \
    (void)mct_user_log_write_utf8_string(&log_local, TEXT)

/**
 * Add utf8-encoded string parameter with given length to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT UTF8-encoded string
 * @param LEN length in bytes to take from @a TEXT
 */
#define MCT_SIZED_UTF8(TEXT, LEN) \
    (void)mct_user_log_write_sized_utf8_string(&log_local, TEXT, LEN)

/**
 * Add constant utf8-encoded string parameter to the log messsage.
 * @param TEXT Constant UTF8-encoded string
 */
#define MCT_CUTF8(TEXT) \
    (void)mct_user_log_write_constant_utf8_string(&log_local, TEXT)

/**
 * Add constant utf8-encoded string parameter with given length to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT Constant UTF8-encoded string
 * @param LEN length in bytes to take from @a TEXT
 */
#define MCT_SIZED_CUTF8(TEXT, LEN) \
    (void)mct_user_log_write_sized_constant_utf8_string(&log_local, TEXT, LEN)

/**
 * Add string parameter with "name" attribute to the log messsage.
 * @param TEXT ASCII string
 * @param NAME "name" attribute
 */
#define MCT_STRING_ATTR(TEXT, NAME) \
    (void)mct_user_log_write_string_attr(&log_local, TEXT, NAME)

/**
 * Add string parameter with given length and "name" attribute to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT ASCII string
 * @param LEN length in bytes to take from @a TEXT
 * @param NAME "name" attribute
 */
#define MCT_SIZED_STRING_ATTR(TEXT, LEN, NAME) \
    (void)mct_user_log_write_sized_string_attr(&log_local, TEXT, LEN, NAME)

/**
 * Add constant string parameter with "name" attribute to the log messsage.
 * @param TEXT Constant ASCII string
 * @param NAME "name" attribute
 */
#define MCT_CSTRING_ATTR(TEXT, NAME) \
    (void)mct_user_log_write_constant_string_attr(&log_local, TEXT, NAME)

/**
 * Add constant string parameter with given length and "name" attribute to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT Constant ASCII string
 * @param LEN length in bytes to take from @a TEXT
 * @param NAME "name" attribute
 */
#define MCT_SIZED_CSTRING_ATTR(TEXT, LEN, NAME) \
    (void)mct_user_log_write_sized_constant_string_attr(&log_local, TEXT, LEN, NAME)

/**
 * Add utf8-encoded string parameter with "name" attribute to the log messsage.
 * @param TEXT UTF8-encoded string
 * @param NAME "name" attribute
 */
#define MCT_UTF8_ATTR(TEXT, NAME) \
    (void)mct_user_log_write_utf8_string_attr(&log_local, TEXT, NAME)

/**
 * Add utf8-encoded string parameter with given length and "name" attribute to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT UTF8-encoded string
 * @param LEN length in bytes to take from @a TEXT
 * @param NAME "name" attribute
 */
#define MCT_SIZED_UTF8_ATTR(TEXT, LEN, NAME) \
    (void)mct_user_log_write_sized_utf8_string_attr(&log_local, TEXT, LEN, ATTR)

/**
 * Add constant utf8-encoded string parameter with "name" attribute to the log messsage.
 * @param TEXT Constant UTF8-encoded string
 * @param NAME "name" attribute
 */
#define MCT_CUTF8_ATTR(TEXT, NAME) \
    (void)mct_user_log_write_constant_utf8_string_attr(&log_local, TEXT, NAME)

/**
 * Add constant utf8-encoded string parameter with given length and "name" attribute to the log messsage.
 * The string in @a TEXT does not need to be null-terminated, but
 * the copied string will be null-terminated at its destination
 * in the message buffer.
 * @param TEXT Constant UTF8-encoded string
 * @param LEN length in bytes to take from @a TEXT
 * @param NAME "name" attribute
 */
#define MCT_SIZED_CUTF8_ATTR(TEXT, LEN, NAME) \
    (void)mct_user_log_write_sized_constant_utf8_string_attr(&log_local, TEXT, LEN, NAME)

/**
 * Add boolean parameter to the log messsage.
 * @param BOOL_VAR Boolean value (mapped to uint8)
 */
#define MCT_BOOL(BOOL_VAR) \
    (void)mct_user_log_write_bool(&log_local, BOOL_VAR)

/**
 * Add boolean parameter with "name" attribute to the log messsage.
 * @param BOOL_VAR Boolean value (mapped to uint8)
 * @param NAME "name" attribute
 */
#define MCT_BOOL_ATTR(BOOL_VAR, NAME) \
    (void)mct_user_log_write_bool_attr(&log_local, BOOL_VAR, NAME)

/**
 * Add float32 parameter to the log messsage.
 * @param FLOAT32_VAR Float32 value (mapped to float)
 */
#define MCT_FLOAT32(FLOAT32_VAR) \
    (void)mct_user_log_write_float32(&log_local, FLOAT32_VAR)

/**
 * Add float64 parameter to the log messsage.
 * @param FLOAT64_VAR Float64 value (mapped to double)
 */
#define MCT_FLOAT64(FLOAT64_VAR) \
    (void)mct_user_log_write_float64(&log_local, FLOAT64_VAR)

/**
 * Add float32 parameter with attributes to the log messsage.
 * @param FLOAT32_VAR Float32 value (mapped to float)
 * @param NAME "name" attribute
 * @param UNIT "unit" attribute
 */
#define MCT_FLOAT32_ATTR(FLOAT32_VAR, NAME, UNIT) \
    (void)mct_user_log_write_float32_attr(&log_local, FLOAT32_VAR, NAME, UNIT)

/**
 * Add float64 parameter with attributes to the log messsage.
 * @param FLOAT64_VAR Float64 value (mapped to double)
 * @param NAME "name" attribute
 * @param UNIT "unit" attribute
 */
#define MCT_FLOAT64_ATTR(FLOAT64_VAR, NAME, UNIT) \
    (void)mct_user_log_write_float64_attr(&log_local, FLOAT64_VAR, NAME, UNIT)

/**
 * Add integer parameter to the log messsage.
 * @param INT_VAR integer value
 */
#define MCT_INT(INT_VAR) \
    (void)mct_user_log_write_int(&log_local, INT_VAR)

#define MCT_INT8(INT_VAR) \
    (void)mct_user_log_write_int8(&log_local, INT_VAR)

#define MCT_INT16(INT_VAR) \
    (void)mct_user_log_write_int16(&log_local, INT_VAR)

#define MCT_INT32(INT_VAR) \
    (void)mct_user_log_write_int32(&log_local, INT_VAR)

#define MCT_INT64(INT_VAR) \
    (void)mct_user_log_write_int64(&log_local, INT_VAR)

/**
 * Add integer parameter with attributes to the log messsage.
 * @param INT_VAR integer value
 * @param NAME "name" attribute
 * @param UNIT "unit" attribute
 */
#define MCT_INT_ATTR(INT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_int_attr(&log_local, INT_VAR, NAME, UNIT)

#define MCT_INT8_ATTR(INT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_int8_attr(&log_local, INT_VAR, NAME, UNIT)

#define MCT_INT16_ATTR(INT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_int16_attr(&log_local, INT_VAR, NAME, UNIT)

#define MCT_INT32_ATTR(INT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_int32_attr(&log_local, INT_VAR, NAME, UNIT)

#define MCT_INT64_ATTR(INT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_int64_attr(&log_local, INT_VAR, NAME, UNIT)

/**
 * Add unsigned integer parameter to the log messsage.
 * @param UINT_VAR unsigned integer value
 */
#define MCT_UINT(UINT_VAR) \
    (void)mct_user_log_write_uint(&log_local, UINT_VAR)

#define MCT_UINT8(UINT_VAR) \
    (void)mct_user_log_write_uint8(&log_local, UINT_VAR)

#define MCT_UINT16(UINT_VAR) \
    (void)mct_user_log_write_uint16(&log_local, UINT_VAR)

#define MCT_UINT32(UINT_VAR) \
    (void)mct_user_log_write_uint32(&log_local, UINT_VAR)

#define MCT_UINT64(UINT_VAR) \
    (void)mct_user_log_write_uint64(&log_local, UINT_VAR)

/**
 * Add unsigned integer parameter with attributes to the log messsage.
 * @param UINT_VAR unsigned integer value
 * @param NAME "name" attribute
 * @param UNIT "unit" attribute
 */
#define MCT_UINT_ATTR(UINT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_uint_attr(&log_local, UINT_VAR, NAME, UNIT)

#define MCT_UINT8_ATTR(UINT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_uint8_attr(&log_local, UINT_VAR, NAME, UNIT)

#define MCT_UINT16_ATTR(UINT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_uint16_attr(&log_local, UINT_VAR, NAME, UNIT)

#define MCT_UINT32_ATTR(UINT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_uint32_attr(&log_local, UINT_VAR, NAME, UNIT)

#define MCT_UINT64_ATTR(UINT_VAR, NAME, UNIT) \
    (void)mct_user_log_write_uint64_attr(&log_local, UINT_VAR, NAME, UNIT)

/**
 * Add binary memory block to the log messages.
 * @param BUF pointer to memory block
 * @param LEN length of memory block
 */
#define MCT_RAW(BUF, LEN) \
    (void)mct_user_log_write_raw(&log_local, BUF, LEN)
#define MCT_HEX8(UINT_VAR) \
    (void)mct_user_log_write_uint8_formatted(&log_local, UINT_VAR, MCT_FORMAT_HEX8)
#define MCT_HEX16(UINT_VAR) \
    (void)mct_user_log_write_uint16_formatted(&log_local, UINT_VAR, MCT_FORMAT_HEX16)
#define MCT_HEX32(UINT_VAR) \
    (void)mct_user_log_write_uint32_formatted(&log_local, UINT_VAR, MCT_FORMAT_HEX32)
#define MCT_HEX64(UINT_VAR) \
    (void)mct_user_log_write_uint64_formatted(&log_local, UINT_VAR, MCT_FORMAT_HEX64)
#define MCT_BIN8(UINT_VAR) \
    (void)mct_user_log_write_uint8_formatted(&log_local, UINT_VAR, MCT_FORMAT_BIN8)
#define MCT_BIN16(UINT_VAR) \
    (void)mct_user_log_write_uint16_formatted(&log_local, UINT_VAR, MCT_FORMAT_BIN16)

/**
 * Add binary memory block with "name" attribute to the log messages.
 * @param BUF pointer to memory block
 * @param LEN length of memory block
 * @param NAME "name" attribute
 */
#define MCT_RAW_ATTR(BUF, LEN, NAME) \
    (void)mct_user_log_write_raw_attr(&log_local, BUF, LEN, NAME)

/**
 * Architecture independent macro to print pointers
 */
#define MCT_PTR(PTR_VAR) \
    (void)mct_user_log_write_ptr(&log_local, PTR_VAR)

/**
 * Trace network message
 * @param CONTEXT object containing information about one special logging context
 * @param TYPE type of network trace message
 * @param HEADERLEN length of network message header
 * @param HEADER pointer to network message header
 * @param PAYLOADLEN length of network message payload
 * @param PAYLOAD pointer to network message payload
 */
#define MCT_TRACE_NETWORK(CONTEXT, TYPE, HEADERLEN, HEADER, PAYLOADLEN, PAYLOAD) \
    do { \
        if ((CONTEXT).trace_status_ptr && *((CONTEXT).trace_status_ptr) == MCT_TRACE_STATUS_ON) \
        { \
            (void)mct_user_trace_network(&(CONTEXT), TYPE, HEADERLEN, HEADER, PAYLOADLEN, PAYLOAD); \
        } \
    } while (0)

/**
 * Trace network message, allow truncation
 * @param CONTEXT object containing information about one special logging context
 * @param TYPE type of network trace message
 * @param HEADERLEN length of network message header
 * @param HEADER pointer to network message header
 * @param PAYLOADLEN length of network message payload
 * @param PAYLOAD pointer to network message payload
 */
#define MCT_TRACE_NETWORK_TRUNCATED(CONTEXT, TYPE, HEADERLEN, HEADER, PAYLOADLEN, PAYLOAD) \
    do { \
        if ((CONTEXT).trace_status_ptr && *((CONTEXT).trace_status_ptr) == MCT_TRACE_STATUS_ON) \
        { \
            (void)mct_user_trace_network_truncated(&(CONTEXT), TYPE, HEADERLEN, HEADER, PAYLOADLEN, PAYLOAD, 1); \
        } \
    } while (0)

/**
 * Trace network message, segment large messages
 * @param CONTEXT object containing information about one special logging context
 * @param TYPE type of network trace message
 * @param HEADERLEN length of network message header
 * @param HEADER pointer to network message header
 * @param PAYLOADLEN length of network message payload
 * @param PAYLOAD pointer to network message payload
 */
#define MCT_TRACE_NETWORK_SEGMENTED(CONTEXT, TYPE, HEADERLEN, HEADER, PAYLOADLEN, PAYLOAD) \
    do { \
        if ((CONTEXT).trace_status_ptr && *((CONTEXT).trace_status_ptr) == MCT_TRACE_STATUS_ON) \
        { \
            (void)mct_user_trace_network_segmented(&(CONTEXT), TYPE, HEADERLEN, HEADER, PAYLOADLEN, PAYLOAD); \
        } \
    } while (0)

/**
 * Send log message with string parameter.
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param TEXT ASCII string
 */
#define MCT_LOG_STRING(CONTEXT, LOGLEVEL, TEXT) \
    do { \
        if (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE) \
        { \
            (void)mct_log_string(&(CONTEXT), LOGLEVEL, TEXT); \
        } \
    } while (0)

/**
 * Send log message with string parameter and integer parameter.
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log messages
 * @param TEXT ASCII string
 * @param INT_VAR integer value
 */
#define MCT_LOG_STRING_INT(CONTEXT, LOGLEVEL, TEXT, INT_VAR) \
    do { \
        if (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE) \
        { \
            (void)mct_log_string_int(&(CONTEXT), LOGLEVEL, TEXT, INT_VAR); \
        } \
    } while (0)

/**
 * Send log message with string parameter and unsigned integer parameter.
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param TEXT ASCII string
 * @param UINT_VAR unsigned integer value
 */
#define MCT_LOG_STRING_UINT(CONTEXT, LOGLEVEL, TEXT, UINT_VAR) \
    do { \
        if (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE) \
        { \
            (void)mct_log_string_uint(&(CONTEXT), LOGLEVEL, TEXT, UINT_VAR); \
        } \
    } while (0)

/**
 * Send log message with unsigned integer parameter.
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param UINT_VAR unsigned integer value
 */
#define MCT_LOG_UINT(CONTEXT, LOGLEVEL, UINT_VAR) \
    do { \
        if (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE) \
        { \
            (void)mct_log_uint(&(CONTEXT), LOGLEVEL, UINT_VAR); \
        } \
    } while (0)

/**
 * Send log message with integer parameter.
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param INT_VAR integer value
 */
#define MCT_LOG_INT(CONTEXT, LOGLEVEL, INT_VAR) \
    do { \
        if (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE) \
        { \
            (void)mct_log_int(&(CONTEXT), LOGLEVEL, INT_VAR); \
        } \
    } while (0)

/**
 * Send log message with binary memory block.
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param BUF pointer to memory block
 * @param LEN length of memory block
 */
#define MCT_LOG_RAW(CONTEXT, LOGLEVEL, BUF, LEN) \
    do { \
        if (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE) \
        { \
            (void)mct_log_raw(&(CONTEXT), LOGLEVEL, BUF, LEN); \
        } \
    } while (0)

/**
 * Send log message with marker.
 */
#define MCT_LOG_MARKER() \
    do { \
        (void)mct_log_marker(); \
    } while (0)

/**
 * Switch to verbose mode
 *
 */
#define MCT_VERBOSE_MODE() do { \
        (void)mct_verbose_mode(); } while (0)

/**
 * Switch to non-verbose mode
 *
 */
#define MCT_NONVERBOSE_MODE() do { \
        (void)mct_nonverbose_mode(); } while (0)

/**
 * Set maximum logged log level and trace status of application
 *
 * @param LOGLEVEL This is the log level to be set for the whole application
 * @param TRACESTATUS This is the trace status to be set for the whole application
 */
#define MCT_SET_APPLICATION_LL_TS_LIMIT(LOGLEVEL, TRACESTATUS) do { \
        (void)mct_set_application_ll_ts_limit(LOGLEVEL, TRACESTATUS); } while (0)

/**
 * Enable local printing of messages
 *
 */
#define MCT_ENABLE_LOCAL_PRINT() do { \
        (void)mct_enable_local_print(); } while (0)

/**
 * Disable local printing of messages
 *
 */
#define MCT_DISABLE_LOCAL_PRINT() do { \
        (void)mct_disable_local_print(); } while (0)

/**
 * Register context for HP (with default log level and default trace status)
 * @param CONTEXT object containing information about one special logging context
 * @param CONTEXTID context id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 * @param TYPE type of ring buffer
 */
#define MCT_REGISTER_CONTEXT_HP(CONTEXT,CONTEXTID,DESCRIPTION,TYPE) do{\
    mct_register_context_hp(&(CONTEXT),CONTEXTID,DESCRIPTION,TYPE);} while(0)

/**
 * HP network trace message
 * @param CONTEXT object containing information about one special logging context
 * @param TYPE type of hp network trace message
 * @param HEADERLEN length of hp network message header
 * @param HEADER pointer to hp network message header
 * @param PAYLOADLEN length of hp network message payload
 * @param PAYLOAD pointer to hp network message payload
 */
#define MCT_TRACE_NETWORK_HP(CONTEXT,TYPE,HEADERLEN,HEADER,PAYLOADLEN,PAYLOAD) \
    do { \
        mct_user_trace_network_hp(&(CONTEXT),TYPE,HEADERLEN,HEADER,PAYLOADLEN,PAYLOAD); \
    }while(0)

/**
 * Check if log level is enabled
 *
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 */
#define MCT_IS_LOG_LEVEL_ENABLED(CONTEXT, LOGLEVEL) \
    (mct_user_is_logLevel_enabled(&CONTEXT, LOGLEVEL) == MCT_RETURN_TRUE)

/**
 \}
 */

#endif /* MCT_USER_MACROS_H */
