#ifndef MCT_TYPES_H
#define MCT_TYPES_H

#ifdef _MSC_VER
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;

typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;

typedef int pid_t;
typedef unsigned int speed_t;

#   define UINT16_MAX 0xFFFF

#   include <varargs.h>
#else
#   include <stdint.h>
#endif

/**
 * Definitions of MCT return values
 */
typedef enum
{
    MCT_RETURN_FILESZERR = -8,
    MCT_RETURN_LOGGING_DISABLED = -7,
    MCT_RETURN_USER_BUFFER_FULL = -6,
    MCT_RETURN_WRONG_PARAMETER = -5,
    MCT_RETURN_BUFFER_FULL = -4,
    MCT_RETURN_PIPE_FULL = -3,
    MCT_RETURN_PIPE_ERROR = -2,
    MCT_RETURN_ERROR = -1,
    MCT_RETURN_OK = 0,
    MCT_RETURN_TRUE = 1
} mct_return_value;

/**
 * Definitions of MCT log level
 */
typedef enum
{
    MCT_LOG_DEFAULT = -1,               /**< Default log level */
    MCT_LOG_OFF = 0x00,                 /**< Log level off */
    MCT_LOG_FATAL = 0x01,               /**< fatal system error */
    MCT_LOG_ERROR = 0x02,               /**< error with impact to correct functionality */
    MCT_LOG_WARN = 0x03,                /**< warning, correct behaviour could not be ensured */
    MCT_LOG_INFO = 0x04,                /**< informational */
    MCT_LOG_DEBUG = 0x05,               /**< debug  */
    MCT_LOG_VERBOSE = 0x06,             /**< highest grade of information */
    MCT_LOG_MAX                         /**< maximum value, used for range check */
} mct_log_level;

/**
 * Definitions of MCT Format
 */
typedef enum
{
    MCT_FORMAT_DEFAULT = 0x00,          /**< no sepecial format */
    MCT_FORMAT_HEX8 = 0x01,             /**< Hex 8 */
    MCT_FORMAT_HEX16 = 0x02,            /**< Hex 16 */
    MCT_FORMAT_HEX32 = 0x03,            /**< Hex 32 */
    MCT_FORMAT_HEX64 = 0x04,            /**< Hex 64 */
    MCT_FORMAT_BIN8 = 0x05,             /**< Binary 8 */
    MCT_FORMAT_BIN16 = 0x06,            /**< Binary 16  */
    MCT_FORMAT_MAX                      /**< maximum value, used for range check */
} mct_format;

/**
 * Definitions of MCT trace status
 */
typedef enum
{
    MCT_TRACE_STATUS_DEFAULT = -1,         /**< Default trace status */
    MCT_TRACE_STATUS_OFF = 0x00,           /**< Trace status: Off */
    MCT_TRACE_STATUS_ON = 0x01,            /**< Trace status: On */
    MCT_TRACE_STATUS_MAX                   /**< maximum value, used for range check */
} mct_trace_status;

/**
 * Definitions for  dlt_user_trace_network/MCT_TRACE_NETWORK()
 * as defined in the MCT protocol
 */
typedef enum
{
    MCT_NW_TRACE_IPC = 0x01,                /**< Interprocess communication */
    MCT_NW_TRACE_CAN = 0x02,                /**< Controller Area Network Bus */
    MCT_NW_TRACE_FLEXRAY = 0x03,            /**< Flexray Bus */
    MCT_NW_TRACE_MOST = 0x04,               /**< Media Oriented System Transport Bus */
    MCT_NW_TRACE_RESERVED0 = 0x05,
    MCT_NW_TRACE_RESERVED1 = 0x06,
    MCT_NW_TRACE_RESERVED2 = 0x07,
    MCT_NW_TRACE_USER_DEFINED0 = 0x08,
    MCT_NW_TRACE_USER_DEFINED1 = 0x09,
    MCT_NW_TRACE_USER_DEFINED2 = 0x0A,
    MCT_NW_TRACE_USER_DEFINED3 = 0x0B,
    MCT_NW_TRACE_USER_DEFINED4 = 0x0C,
    MCT_NW_TRACE_USER_DEFINED5 = 0x0D,
    MCT_NW_TRACE_USER_DEFINED6 = 0x0E,
    MCT_NW_TRACE_RESEND = 0x0F,             /**< Mark a resend */
    MCT_NW_TRACE_MAX                        /**< maximum value, used for range check */
} mct_network_trace;

/**
 * This are the log modes.
 */
typedef enum
{
    MCT_USER_MODE_UNDEFINED = -1,
    MCT_USER_MODE_OFF = 0,
    MCT_USER_MODE_EXTERNAL,
    MCT_USER_MODE_INTERNAL,
    MCT_USER_MODE_BOTH,
    MCT_USER_MODE_MAX                       /**< maximum value, used for range check */
} mct_publisher_logmode;

/**
 * Definition of Maintain Logstorage Loglevel modes
 */
#define MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_UNDEF -1
#define MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_OFF    0
#define MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_ON     1

typedef float float32_t;
typedef double float64_t;

/**
 * Definition of timestamp types
 */
typedef enum
{
	MCT_AUTO_TIMESTAMP = 0,
	MCT_USER_TIMESTAMP
} mct_timestamp;

#endif  /* MCT_TYPES_H */
