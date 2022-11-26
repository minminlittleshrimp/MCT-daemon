#ifndef MCT_COMMON_H
#   define MCT_COMMON_H

/**
 * \defgroup commonapi MCT Common API
 * \addtogroup commonapi
 \{
 */

#   include <netinet/in.h>
#   include <stdio.h>
#   include <stdbool.h>
#   ifdef __linux__
#      include <linux/limits.h>
#      include <sys/socket.h>
#   else
#      include <limits.h>
#   endif

#   if !defined(_MSC_VER)
#      include <unistd.h>
#      include <time.h>
#   endif

#   if defined(__GNUC__)
#      define PURE_FUNCTION __attribute__((pure))
#      define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, a, b)))
#   else
#      define PURE_FUNCTION /* nothing */
#      define PRINTF_FORMAT(a,b) /* nothing */
#   endif

#   if !defined (__WIN32__) && !defined(_MSC_VER)
#      include <termios.h>
#   endif

#   include "mct_types.h"
#   include "mct_protocol.h"

#   define MCT_PACKED __attribute__((aligned(1), packed))

#   if defined (__MSDOS__) || defined (_MSC_VER)
/* set instead /Zp8 flag in Visual C++ configuration */
#      undef MCT_PACKED
#      define MCT_PACKED
#   endif

/*
 * Macros to swap the byte order.
 */
#   define MCT_SWAP_64(value) ((((uint64_t)MCT_SWAP_32((value) & 0xffffffffull)) << 32) | (MCT_SWAP_32((value) >> 32)))

#   define MCT_SWAP_16(value) ((((value) >> 8) & 0xff) | (((value) << 8) & 0xff00))
#   define MCT_SWAP_32(value) ((((value) >> 24) & 0xff) | (((value) << 8) & 0xff0000) | (((value) >> 8) & 0xff00) | \
                               (((value) << 24) & 0xff000000))

/* Set Big Endian and Little Endian to a initial value, if not defined */
#   if !defined __USE_BSD
#      ifndef LITTLE_ENDIAN
#         define LITTLE_ENDIAN 1234
#      endif

#      ifndef BIG_ENDIAN
#         define BIG_ENDIAN    4321
#      endif
#   endif /* __USE_BSD */

/* If byte order is not defined, default to little endian */
#   if !defined __USE_BSD
#      ifndef BYTE_ORDER
#         define BYTE_ORDER LITTLE_ENDIAN
#      endif
#   endif /* __USE_BSD */

/* Check for byte-order */
#   if (BYTE_ORDER == BIG_ENDIAN)
/* #warning "Big Endian Architecture!" */
#      define MCT_HTOBE_16(x) ((x))
#      define MCT_HTOLE_16(x) MCT_SWAP_16((x))
#      define MCT_BETOH_16(x) ((x))
#      define MCT_LETOH_16(x) MCT_SWAP_16((x))

#      define MCT_HTOBE_32(x) ((x))
#      define MCT_HTOLE_32(x) MCT_SWAP_32((x))
#      define MCT_BETOH_32(x) ((x))
#      define MCT_LETOH_32(x) MCT_SWAP_32((x))

#      define MCT_HTOBE_64(x) ((x))
#      define MCT_HTOLE_64(x) MCT_SWAP_64((x))
#      define MCT_BETOH_64(x) ((x))
#      define MCT_LETOH_64(x) MCT_SWAP_64((x))
#   else
/* #warning "Litte Endian Architecture!" */
#      define MCT_HTOBE_16(x) MCT_SWAP_16((x))
#      define MCT_HTOLE_16(x) ((x))
#      define MCT_BETOH_16(x) MCT_SWAP_16((x))
#      define MCT_LETOH_16(x) ((x))

#      define MCT_HTOBE_32(x) MCT_SWAP_32((x))
#      define MCT_HTOLE_32(x) ((x))
#      define MCT_BETOH_32(x) MCT_SWAP_32((x))
#      define MCT_LETOH_32(x) ((x))

#      define MCT_HTOBE_64(x) MCT_SWAP_64((x))
#      define MCT_HTOLE_64(x) ((x))
#      define MCT_BETOH_64(x) MCT_SWAP_64((x))
#      define MCT_LETOH_64(x) ((x))
#   endif

#   define MCT_ENDIAN_GET_16(htyp, x) ((((htyp) & MCT_HTYP_MSBF) > 0) ? MCT_BETOH_16(x) : MCT_LETOH_16(x))
#   define MCT_ENDIAN_GET_32(htyp, x) ((((htyp) & MCT_HTYP_MSBF) > 0) ? MCT_BETOH_32(x) : MCT_LETOH_32(x))
#   define MCT_ENDIAN_GET_64(htyp, x) ((((htyp) & MCT_HTYP_MSBF) > 0) ? MCT_BETOH_64(x) : MCT_LETOH_64(x))

#   if defined (__WIN32__) || defined (_MSC_VER)
#      define LOG_EMERG     0
#      define LOG_ALERT     1
#      define LOG_CRIT      2
#      define LOG_ERR       3
#      define LOG_WARNING   4
#      define LOG_NOTICE    5
#      define LOG_INFO      6
#      define LOG_DEBUG     7

#      define LOG_PID     0x01
#      define LOG_DAEMON  (3 << 3)
#   endif

enum {
    MCT_LOG_TO_CONSOLE = 0,
    MCT_LOG_TO_SYSLOG = 1,
    MCT_LOG_TO_FILE = 2,
    MCT_LOG_TO_STDERR = 3,
    MCT_LOG_DROPPED = 4
};

/**
 * The standard TCP Port used for MCT daemon, can be overwritten via -p \<port\> when starting mct-daemon
 */
#   define MCT_DAEMON_TCP_PORT 3490


/* Initial value for file descriptor */
#   define MCT_FD_INIT -1

/* Minimum value for a file descriptor except the POSIX Standards: stdin=0, stdout=1, stderr=2 */
#   define MCT_FD_MINIMUM 3

/**
 * The size of a MCT ID
 */
#   define MCT_ID_SIZE 4

#   define MCT_SIZE_WEID MCT_ID_SIZE
#   define MCT_SIZE_WSID (sizeof(uint32_t))
#   define MCT_SIZE_WTMS (sizeof(uint32_t))

/* Size of buffer for text output */
#define MCT_CONVERT_TEXTBUFSIZE  10024

/**
 * Definitions for GET_LOG_INFO
 */
#   define MCT_GET_LOG_INFO_HEADER 18  /*Get log info header size in response text */
#   define GET_LOG_INFO_LENGTH 13
#   define SERVICE_OPT_LENGTH 3

/**
 * Get the size of extra header parameters, depends on htyp.
 */
#   define MCT_STANDARD_HEADER_EXTRA_SIZE(htyp) ((MCT_IS_HTYP_WEID(htyp) ? MCT_SIZE_WEID : 0) + \
                                                 (MCT_IS_HTYP_WSID(htyp) ? MCT_SIZE_WSID : 0) + \
                                                 (MCT_IS_HTYP_WTMS(htyp) ? MCT_SIZE_WTMS : 0))


#   if defined (__MSDOS__) || defined (_MSC_VER)
#      define __func__ __FUNCTION__
#   endif

#   define PRINT_FUNCTION_VERBOSE(_verbose) \
    if (_verbose) \
        mct_vlog(LOG_INFO, "%s()\n", __func__)

#   ifndef NULL
#      define NULL (char *)0
#   endif

#   define MCT_MSG_IS_CONTROL(MSG)          ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                             (MCT_GET_MSIN_MSTP((MSG)->extendedheader->msin) == MCT_TYPE_CONTROL))

#   define MCT_MSG_IS_CONTROL_REQUEST(MSG)  ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                             (MCT_GET_MSIN_MSTP((MSG)->extendedheader->msin) == MCT_TYPE_CONTROL) && \
                                             (MCT_GET_MSIN_MTIN((MSG)->extendedheader->msin) == MCT_CONTROL_REQUEST))

#   define MCT_MSG_IS_CONTROL_RESPONSE(MSG) ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                             (MCT_GET_MSIN_MSTP((MSG)->extendedheader->msin) == MCT_TYPE_CONTROL) && \
                                             (MCT_GET_MSIN_MTIN((MSG)->extendedheader->msin) == MCT_CONTROL_RESPONSE))

#   define MCT_MSG_IS_CONTROL_TIME(MSG)     ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                             (MCT_GET_MSIN_MSTP((MSG)->extendedheader->msin) == MCT_TYPE_CONTROL) && \
                                             (MCT_GET_MSIN_MTIN((MSG)->extendedheader->msin) == MCT_CONTROL_TIME))

#   define MCT_MSG_IS_NW_TRACE(MSG)         ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                             (MCT_GET_MSIN_MSTP((MSG)->extendedheader->msin) == MCT_TYPE_NW_TRACE))

#   define MCT_MSG_IS_TRACE_MOST(MSG)       ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                             (MCT_GET_MSIN_MSTP((MSG)->extendedheader->msin) == MCT_TYPE_NW_TRACE) && \
                                             (MCT_GET_MSIN_MTIN((MSG)->extendedheader->msin) == MCT_NW_TRACE_MOST))

#   define MCT_MSG_IS_NONVERBOSE(MSG)       (!(MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) || \
                                             ((MCT_IS_HTYP_UEH((MSG)->standardheader->htyp)) && \
                                              (!(MCT_IS_MSIN_VERB((MSG)->extendedheader->msin)))))

/*
 *
 * Definitions of MCT message buffer overflow
 */
#   define MCT_MESSAGE_BUFFER_NO_OVERFLOW     0x00/**< Buffer overflow has not occured */
#   define MCT_MESSAGE_BUFFER_OVERFLOW        0x01/**< Buffer overflow has occured */

/*
 * Definition of MCT output variants
 */
#   define MCT_OUTPUT_HEX              1
#   define MCT_OUTPUT_ASCII            2
#   define MCT_OUTPUT_MIXED_FOR_PLAIN  3
#   define MCT_OUTPUT_MIXED_FOR_HTML   4
#   define MCT_OUTPUT_ASCII_LIMITED    5

#   define MCT_FILTER_MAX 30 /**< Maximum number of filters */
#   define JSON_FILTER_NAME_SIZE 16 /* Size of buffer for the filter names in json filter files */
#   define JSON_FILTER_SIZE 200     /* Size in bytes, that the definition of one filter with all parameters needs */

#   define MCT_MSG_READ_VALUE(dst, src, length, type) \
    do { \
        if ((length < 0) || ((length) < ((int32_t)sizeof(type)))) \
        { length = -1; } \
        else \
        { dst = *((type *)src); src += sizeof(type); length -= sizeof(type); } \
    } while(0)

#   define MCT_MSG_READ_ID(dst, src, length) \
    do { \
        if ((length < 0) || ((length) < MCT_ID_SIZE)) \
        { length = -1; } \
        else \
        { memcpy(dst, src, MCT_ID_SIZE); src += MCT_ID_SIZE; length -= MCT_ID_SIZE; } \
    } while(0)

#   define MCT_MSG_READ_STRING(dst, src, maxlength, dstlength, length) \
    do { \
        if ((maxlength < 0) || (length <= 0) || (dstlength < length) || (maxlength < length)) \
        { \
            maxlength = -1; \
        } \
        else \
        { \
            memcpy(dst, src, length); \
            mct_clean_string(dst, length); \
            dst[length] = 0; \
            src += length; \
            maxlength -= length; \
        } \
    } while(0)

#   define MCT_MSG_READ_NULL(src, maxlength, length) \
    do { \
        if (((maxlength) < 0) || ((length) < 0) || ((maxlength) < (length))) \
        { length = -1; } \
        else \
        { src += length; maxlength -= length; } \
    } while(0)

#   define MCT_HEADER_SHOW_NONE       0x0000
#   define MCT_HEADER_SHOW_TIME       0x0001
#   define MCT_HEADER_SHOW_TMSTP      0x0002
#   define MCT_HEADER_SHOW_MSGCNT     0x0004
#   define MCT_HEADER_SHOW_ECUID      0x0008
#   define MCT_HEADER_SHOW_APID       0x0010
#   define MCT_HEADER_SHOW_CTID       0x0020
#   define MCT_HEADER_SHOW_MSGTYPE    0x0040
#   define MCT_HEADER_SHOW_MSGSUBTYPE 0x0080
#   define MCT_HEADER_SHOW_VNVSTATUS  0x0100
#   define MCT_HEADER_SHOW_NOARG      0x0200
#   define MCT_HEADER_SHOW_ALL        0xFFFF

/* mct_receiver_check_and_get flags */
#   define MCT_RCV_NONE        0
#   define MCT_RCV_SKIP_HEADER (1 << 0)
#   define MCT_RCV_REMOVE      (1 << 1)

/**
 * Maximal length of path in MCT
 * MCT limits the path length and does not do anything else to determine
 * the actual value, because the least that is supported on any system
 * that MCT runs on is 1024 bytes.
 */
#   define MCT_PATH_MAX 1024

/**
 * Maximal length of mounted path
 */
#   define MCT_MOUNT_PATH_MAX  1024

/**
 * Maximal length of an entry
 */
#   define MCT_ENTRY_MAX 100

/**
 * Maximal IPC path len
 */
#   define MCT_IPC_PATH_MAX 100

/**
 * Maximal receiver buffer size for application messages
 */
#   define MCT_RECEIVE_BUFSIZE 65535

/**
 * Maximal line length
 */
#   define MCT_LINE_LEN 1024

/**
 * Macros for network trace
 */
#define MCT_TRACE_NW_TRUNCATED "NWTR"
#define MCT_TRACE_NW_START "NWST"
#define MCT_TRACE_NW_SEGMENT "NWCH"
#define MCT_TRACE_NW_END "NWEN"

/**
 * Type to specify whether received data is from socket or file/fifo
 */
typedef enum
{
    MCT_RECEIVE_SOCKET,
    MCT_RECEIVE_UDP_SOCKET,
    MCT_RECEIVE_FD
} MctReceiverType;

/**
 * The definition of the serial header containing the characters "DLS" + 0x01.
 */
extern const char mctSerialHeader[MCT_ID_SIZE];

/**
 * The definition of the serial header containing the characters "DLS" + 0x01 as char.
 */
extern char mctSerialHeaderChar[MCT_ID_SIZE];

#if defined MCT_DAEMON_USE_FIFO_IPC || defined MCT_LIB_USE_FIFO_IPC
/**
 * The common base-path of the mct-daemon-fifo and application-generated fifos
 */
extern char mctFifoBaseDir[MCT_PATH_MAX];
#endif

/**
 * The type of a MCT ID (context id, application id, etc.)
 */
typedef char ID4[MCT_ID_SIZE];

/**
 * The structure of the MCT file storage header. This header is used before each stored MCT message.
 */
typedef struct
{
    char pattern[MCT_ID_SIZE]; /**< This pattern should be MCT0x01 */
    uint32_t seconds;          /**< seconds since 1.1.1970 */
    int32_t microseconds;      /**< Microseconds */
    char ecu[MCT_ID_SIZE];     /**< The ECU id is added, if it is not already in the MCT message itself */
} MCT_PACKED MctStorageHeader;

/**
 * The structure of the MCT standard header. This header is used in each MCT message.
 */
typedef struct
{
    uint8_t htyp;           /**< This parameter contains several informations, see definitions below */
    uint8_t mcnt;           /**< The message counter is increased with each sent MCT message */
    uint16_t len;           /**< Length of the complete message, without storage header */
} MCT_PACKED MctStandardHeader;

/**
 * The structure of the MCT extra header parameters. Each parameter is sent only if enabled in htyp.
 */
typedef struct
{
    char ecu[MCT_ID_SIZE];       /**< ECU id */
    uint32_t seid;               /**< Session number */
    uint32_t tmsp;               /**< Timestamp since system start in 0.1 milliseconds */
} MCT_PACKED MctStandardHeaderExtra;

/**
 * The structure of the MCT extended header. This header is only sent if enabled in htyp parameter.
 */
typedef struct
{
    uint8_t msin;              /**< messsage info */
    uint8_t noar;              /**< number of arguments */
    char apid[MCT_ID_SIZE];    /**< application id */
    char ctid[MCT_ID_SIZE];    /**< context id */
} MCT_PACKED MctExtendedHeader;

/**
 * The structure to organise the MCT messages.
 * This structure is used by the corresponding functions.
 */
typedef struct sMctMessage
{
    /* flags */
    int8_t found_serialheader;

    /* offsets */
    int32_t resync_offset;

    /* size parameters */
    int32_t headersize;    /**< size of complete header including storage header */
    int32_t datasize;      /**< size of complete payload */

    /* buffer for current loaded message */
    uint8_t headerbuffer[sizeof(MctStorageHeader) +
                         sizeof(MctStandardHeader) + sizeof(MctStandardHeaderExtra) + sizeof(MctExtendedHeader)]; /**< buffer for loading complete header */
    uint8_t *databuffer;         /**< buffer for loading payload */
    int32_t databuffersize;

    /* header values of current loaded message */
    MctStorageHeader *storageheader;        /**< pointer to storage header of current loaded header */
    MctStandardHeader *standardheader;      /**< pointer to standard header of current loaded header */
    MctStandardHeaderExtra headerextra;     /**< extra parameters of current loaded header */
    MctExtendedHeader *extendedheader;      /**< pointer to extended of current loaded header */
} MctMessage;

/**
 * The structure of the MCT Service Get Log Info.
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t options;                /**< type of request */
    char apid[MCT_ID_SIZE];         /**< application id */
    char ctid[MCT_ID_SIZE];         /**< context id */
    char com[MCT_ID_SIZE];          /**< communication interface */
} MCT_PACKED MctServiceGetLogInfoRequest;

typedef struct
{
    uint32_t service_id;            /**< service ID */
} MCT_PACKED MctServiceGetDefaultLogLevelRequest;

/**
 * The structure of the MCT Service Get Log Info response.
 */
typedef struct
{
    char context_id[MCT_ID_SIZE];
    int16_t log_level;
    int16_t trace_status;
    uint16_t len_context_description;
    char *context_description;
} ContextIDsInfoType;

typedef struct
{
    char app_id[MCT_ID_SIZE];
    uint16_t count_context_ids;
    ContextIDsInfoType *context_id_info; /**< holds info about a specific con id */
    uint16_t len_app_description;
    char *app_description;
} AppIDsType;

typedef struct
{
    uint16_t count_app_ids;
    AppIDsType *app_ids;            /**< holds info about a specific app id */
} LogInfoType;

typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< type of request */
    LogInfoType log_info_type;      /**< log info type */
    char com[MCT_ID_SIZE];      /**< communication interface */
} MctServiceGetLogInfoResponse;

/**
 * The structure of the MCT Service Set Log Level.
 */
typedef struct
{

    uint32_t service_id;            /**< service ID */
    char apid[MCT_ID_SIZE];         /**< application id */
    char ctid[MCT_ID_SIZE];         /**< context id */
    uint8_t log_level;              /**< log level to be set */
    char com[MCT_ID_SIZE];          /**< communication interface */
} MCT_PACKED MctServiceSetLogLevel;

/**
 * The structure of the MCT Service Set Default Log Level.
 */
typedef struct
{
    uint32_t service_id;                /**< service ID */
    uint8_t log_level;                  /**< default log level to be set */
    char com[MCT_ID_SIZE];              /**< communication interface */
} MCT_PACKED MctServiceSetDefaultLogLevel;

/**
 * The structure of the MCT Service Set Verbose Mode
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t new_status;             /**< new status to be set */
} MCT_PACKED MctServiceSetVerboseMode;

/**
 * The structure of the MCT Service Set Communication Interface Status
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    char com[MCT_ID_SIZE];          /**< communication interface */
    uint8_t new_status;             /**< new status to be set */
} MCT_PACKED MctServiceSetCommunicationInterfaceStatus;

/**
 * The structure of the MCT Service Set Communication Maximum Bandwidth
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    char com[MCT_ID_SIZE];          /**< communication interface */
    uint32_t max_bandwidth;         /**< maximum bandwith */
} MCT_PACKED MctServiceSetCommunicationMaximumBandwidth;

typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
} MCT_PACKED MctServiceResponse;

typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
    uint8_t log_level;              /**< log level */
} MCT_PACKED MctServiceGetDefaultLogLevelResponse;

typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
    uint8_t overflow;               /**< overflow status */
    uint32_t overflow_counter;      /**< overflow counter */
} MCT_PACKED MctServiceMessageBufferOverflowResponse;

typedef struct
{
    uint32_t service_id;            /**< service ID */
} MCT_PACKED MctServiceGetSoftwareVersion;

typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
    uint32_t length;                /**< length of following payload */
    char *payload;                  /**< payload */
} MCT_PACKED MctServiceGetSoftwareVersionResponse;

/**
 * The structure of the MCT Service Unregister Context.
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
    char apid[MCT_ID_SIZE];         /**< application id */
    char ctid[MCT_ID_SIZE];         /**< context id */
    char comid[MCT_ID_SIZE];        /**< communication interface */
} MCT_PACKED MctServiceUnregisterContext;

/**
 * The structure of the MCT Service Connection Info
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
    uint8_t state;                  /**< new state */
    char comid[MCT_ID_SIZE];        /**< communication interface */
} MCT_PACKED MctServiceConnectionInfo;

/**
 * The structure of the MCT Service Timezone
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
    int32_t timezone;               /**< Timezone in seconds */
    uint8_t isdst;                  /**< Is daylight saving time */
} MCT_PACKED MctServiceTimezone;

/**
 * The structure of the MCT Service Marker
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< reponse status */
} MCT_PACKED MctServiceMarker;

/***
 * The structure of the MCT Service Offline Logstorage
 */
typedef struct
{
    uint32_t service_id;                  /**< service ID */
    char mount_point[MCT_MOUNT_PATH_MAX]; /**< storage device mount point */
    uint8_t connection_type;              /**< connection status of the connected device connected/disconnected */
    char comid[MCT_ID_SIZE];              /**< communication interface */
} MCT_PACKED MctServiceOfflineLogstorage;

/**
 * The structure of MCT Service Set Filter level
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint32_t level;                 /**< level */
} MCT_PACKED MctServiceSetFilterLevel;

/**
 * The structure of MCT Service Get Filter Config
 */
#define MCT_NUM_SERVICE_ID      (MCT_SERVICE_ID_LAST_ENTRY / 8) + 1
#define MCT_NUM_USER_SERVICE_ID ((MCT_USER_SERVICE_ID_LAST_ENTRY&0xFF) / 8) + 1

typedef struct
{
    uint32_t service_id;                      /**< service ID */
    uint8_t status;                           /**< response status */
    char name[MCT_ENTRY_MAX];                 /**< config name */
    uint32_t level_min;                       /**< minimum filter level */
    uint32_t level_max;                       /**< maximum filter level */
    uint32_t client_mask;                     /**< client mask */
    uint8_t ctrl_mask_lower[MCT_NUM_SERVICE_ID]; /**< lower control message mask */
    uint8_t ctrl_mask_upper[MCT_NUM_USER_SERVICE_ID]; /**< upper control message mask */
    char injections[MCT_ENTRY_MAX];           /**< list of injections */
} MCT_PACKED MctServiceGetCurrentFilterInfo;

/**
 * The structure of MCT Service Set Block mode
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    char apid[MCT_ID_SIZE];         /**< specific app to change blockmode */
    uint32_t mode;                  /**< blockmode value */
} MCT_PACKED MctServiceSetBlockMode;

/**
 * The structure of MCT Service Get Block mode
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint8_t status;                 /**< response status */
    char apid[MCT_ID_SIZE];         /**< specific app to get blockmode */
    uint8_t mode;                   /**< blockmode value */
} MCT_PACKED MctServiceGetBlockMode;

/**
 * The structure of MCT Service Passive Node Connect
 */
typedef struct
{
    uint32_t service_id;            /**< service ID */
    uint32_t connection_status;     /**< connect/disconnect */
    char node_id[MCT_ID_SIZE];      /**< passive node ID */
} MCT_PACKED MctServicePassiveNodeConnect;

/**
 * The structure of MCT Service Passive Node Connection Status
 */
typedef struct
{
    uint32_t service_id;                       /**< service ID */
    uint8_t status;                            /**< response status */
    uint32_t num_connections;                  /**< number of connections */
    uint8_t connection_status[MCT_ENTRY_MAX];  /**< list of connection status */
    char node_id[MCT_ENTRY_MAX];               /**< list of passive node IDs */
} MCT_PACKED MctServicePassiveNodeConnectionInfo;

/**
 * Structure to store filter parameters.
 * ID are maximal four characters. Unused values are filled with zeros.
 * If every value as filter is valid, the id should be empty by having only zero values.
 */
typedef struct
{
    char apid[MCT_FILTER_MAX][MCT_ID_SIZE]; /**< application id */
    char ctid[MCT_FILTER_MAX][MCT_ID_SIZE]; /**< context id */
    int log_level[MCT_FILTER_MAX];          /**< log level */
    int32_t payload_max[MCT_FILTER_MAX];        /**< upper border for payload */
    int32_t payload_min[MCT_FILTER_MAX];        /**< lower border for payload */
    int counter;                            /**< number of filters */
} MctFilter;

/**
 * The structure to organise the access to MCT files.
 * This structure is used by the corresponding functions.
 */
typedef struct sMctFile
{
    /* file handle and index for fast access */
    FILE *handle;      /**< file handle of opened MCT file */
    long *index;       /**< file positions of all MCT messages for fast access to file, only filtered messages */

    /* size parameters */
    int32_t counter;       /**< number of messages in MCT file with filter */
    int32_t counter_total; /**< number of messages in MCT file without filter */
    int32_t position;      /**< current index to message parsed in MCT file starting at 0 */
    uint64_t file_length;    /**< length of the file */
    uint64_t file_position;  /**< current position in the file */

    /* error counters */
    int32_t error_messages; /**< number of incomplete MCT messages found during file parsing */

    /* filter parameters */
    MctFilter *filter;      /**< pointer to filter list. Zero if no filter is set. */
    int32_t filter_counter; /**< number of filter set */

    /* current loaded message */
    MctMessage msg;     /**< pointer to message */

} MctFile;

/**
 * The structure is used to organise the receiving of data
 * including buffer handling.
 * This structure is used by the corresponding functions.
 */
typedef struct
{
    int32_t lastBytesRcvd;    /**< bytes received in last receive call */
    int32_t bytesRcvd;        /**< received bytes */
    int32_t totalBytesRcvd;   /**< total number of received bytes */
    char *buffer;         /**< pointer to receiver buffer */
    char *buf;            /**< pointer to position within receiver buffer */
    char *backup_buf;     /** pointer to the buffer with partial messages if any **/
    int fd;               /**< connection handle */
    MctReceiverType type;     /**< type of connection handle */
    int32_t buffersize;       /**< size of receiver buffer */
    struct sockaddr_in addr;  /**< socket address information */
} MctReceiver;

typedef struct
{
    unsigned char *shm; /* pointer to beginning of shared memory */
    unsigned int size;  /* size of data area in shared memory */
    unsigned char *mem; /* pointer to data area in shared memory */

    uint32_t min_size;     /**< Minimum size of buffer */
    uint32_t max_size;     /**< Maximum size of buffer */
    uint32_t step_size;    /**< Step size of buffer */
} MctBuffer;

typedef struct
{
    int write;
    int read;
    int count;
} MctBufferHead;

#   define MCT_BUFFER_HEAD "SHM"

typedef struct
{
    char head[4];
    unsigned char status;
    int size;
} MctBufferBlockHead;

#   ifdef MCT_USE_IPv6
#      define MCT_IP_SIZE (INET6_ADDRSTRLEN)
#   else
#      define MCT_IP_SIZE (INET_ADDRSTRLEN)
#   endif
typedef struct MctBindAddress
{
    char ip[MCT_IP_SIZE];
    struct MctBindAddress *next;
} MctBindAddress_t;

#   define MCT_MESSAGE_ERROR_OK       0
#   define MCT_MESSAGE_ERROR_UNKNOWN -1
#   define MCT_MESSAGE_ERROR_SIZE    -2
#   define MCT_MESSAGE_ERROR_CONTENT -3

#   ifdef __cplusplus
extern "C"
{
#   endif

/**
 * Helper function to print a byte array in hex.
 * @param ptr pointer to the byte array.
 * @param size number of bytes to be printed.
 */
void mct_print_hex(uint8_t *ptr, int size);
/**
 * Helper function to print a byte array in hex into a string.
 * @param text pointer to a ASCII string, in which the text is written
 * @param textlength maximal size of text buffer
 * @param ptr pointer to the byte array.
 * @param size number of bytes to be printed.
 * @return negative value if there was an error
 */
MctReturnValue mct_print_hex_string(char *text, int textlength, uint8_t *ptr, int size);
/**
 * Helper function to print a byte array in hex and ascii into a string.
 * @param text pointer to a ASCII string, in which the text is written
 * @param textlength maximal size of text buffer
 * @param ptr pointer to the byte array.
 * @param size number of bytes to be printed.
 * @param html output is html? 0 - false, 1 - true
 * @return negative value if there was an error
 */
MctReturnValue mct_print_mixed_string(char *text, int textlength, uint8_t *ptr, int size, int html);
/**
 * Helper function to print a byte array in ascii into a string.
 * @param text pointer to a ASCII string, in which the text is written
 * @param textlength maximal size of text buffer
 * @param ptr pointer to the byte array.
 * @param size number of bytes to be printed.
 * @return negative value if there was an error
 */
MctReturnValue mct_print_char_string(char **text, int textlength, uint8_t *ptr, int size);

/**
 * Helper function to determine a bounded length of a string.
 * This function returns zero if @a str is a null pointer,
 * and it returns @a maxsize if the null character was not found in the first @a maxsize bytes of @a str.
 * This is a re-implementation of C11's strnlen_s, which we cannot yet assume to be available.
 * @param str pointer to string whose length is to be determined
 * @param maxsize maximal considered length of @a str
 * @return the bounded length of the string
 */
PURE_FUNCTION size_t mct_strnlen_s(const char* str, size_t maxsize);

/**
 * Helper function to print an id.
 * @param text pointer to ASCII string where to write the id
 * @param id four byte char array as used in MCT mesages as IDs.
 */
void mct_print_id(char *text, const char *id);

/**
 * Helper function to set an ID parameter.
 * @param id four byte char array as used in MCT mesages as IDs.
 * @param text string to be copied into char array.
 */
void mct_set_id(char *id, const char *text);

/**
 * Helper function to remove not nice to print characters, e.g. NULL or carage return.
 * @param text pointer to string to be cleaned.
 * @param length length of string excluding terminating zero.
 */
void mct_clean_string(char *text, int length);

/**
 * Initialise the filter list.
 * This function must be called before using further mct filter.
 * @param filter pointer to structure of organising MCT filter
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_filter_init(MctFilter *filter, int verbose);
/**
 * Free the used memory by the organising structure of filter.
 * @param filter pointer to structure of organising MCT filter
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_filter_free(MctFilter *filter, int verbose);
/**
 * Load filter list from file.
 * @param filter pointer to structure of organising MCT filter
 * @param filename filename to load filters from
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_filter_load(MctFilter *filter, const char *filename, int verbose);
/**
 * Save filter in space separated list to text file.
 * @param filter pointer to structure of organising MCT filter
 * @param filename filename to safe filters into
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_filter_save(MctFilter *filter, const char *filename, int verbose);
/**
 * Find index of filter in filter list
 * @param filter pointer to structure of organising MCT filter
 * @param apid application id to be found in filter list
 * @param ctid context id to be found in filter list
 * @param log_level log level to be found in filter list
 * @param payload_min minimum payload lenght to be found in filter list
 * @param payload_max maximum payload lenght to be found in filter list
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error (or not found), else return index of filter
 */
int mct_filter_find(MctFilter *filter, const char *apid, const char *ctid, const int log_level,
                                const int32_t payload_min, const int32_t payload_max, int verbose);
/**
 * Add new filter to filter list.
 * @param filter pointer to structure of organising MCT filter
 * @param apid application id to be added to filter list (must always be set).
 * @param ctid context id to be added to filter list. empty equals don't care.
 * @param log_level log level to be added to filter list. 0 equals don't care.
 * @param payload_min min lenght of payload to be added to filter list. 0 equals don't care.
 * @param payload_max max lenght of payload to be added to filter list. INT32_MAX equals don't care.
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_filter_add(MctFilter *filter, const char *apid, const char *ctid, const int log_level,
                                const int32_t payload_min, const int32_t payload_max, int verbose);
/**
 * Delete filter from filter list
 * @param filter pointer to structure of organising MCT filter
 * @param apid application id to be deleted from filter list
 * @param ctid context id to be deleted from filter list
 * @param log_level log level to be deleted from filter list
 * @param payload_min minimum payload lenght to be deleted from filter list
 * @param payload_max maximum payload lenght to be deleted from filter list
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_filter_delete(MctFilter *filter, const char *apid, const char *ctid, const int log_level,
                                const int32_t payload_min, const int32_t payload_max, int verbose);

/**
 * Initialise the structure used to access a MCT message.
 * This function must be called before using further mct_message functions.
 * @param msg pointer to structure of organising access to MCT messages
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_init(MctMessage *msg, int verbose);
/**
 * Free the used memory by the organising structure of file.
 * @param msg pointer to structure of organising access to MCT messages
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_free(MctMessage *msg, int verbose);
/**
 * Print Header into an ASCII string.
 * This function calls mct_message_header_flags() with flags=MCT_HEADER_SHOW_ALL
 * @param msg pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the header is written
 * @param textlength maximal size of text buffer
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_header(MctMessage *msg, char *text, size_t textlength, int verbose);
/**
 * Print Header into an ASCII string, selective.
 * @param msg pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the header is written
 * @param textlength maximal size of text buffer
 * @param flags select, bit-field to select, what should be printed (MCT_HEADER_SHOW_...)
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_header_flags(MctMessage *msg, char *text, size_t textlength, int flags, int verbose);
/**
 * Print Payload into an ASCII string.
 * @param msg pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the header is written
 * @param textlength maximal size of text buffer
 * @param type 1 = payload as hex, 2 = payload as ASCII.
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_payload(MctMessage *msg, char *text, size_t textlength, int type, int verbose);
/**
 * Check if message is filtered or not. All filters are applied (logical OR).
 * @param msg pointer to structure of organising access to MCT messages
 * @param filter pointer to filter
 * @param verbose if set to true verbose information is printed out.
 * @return 1 = filter matches, 0 = filter does not match, negative value if there was an error
 */
MctReturnValue mct_message_filter_check(MctMessage *msg, MctFilter *filter, int verbose);

/**
 * Read message from memory buffer.
 * Message in buffer has no storage header.
 * @param msg pointer to structure of organising access to MCT messages
 * @param buffer pointer to memory buffer
 * @param length length of message in buffer
 * @param resync if set to true resync to serial header is enforced
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_message_read(MctMessage *msg, uint8_t *buffer, unsigned int length, int resync, int verbose);

/**
 * Get standard header extra parameters
 * @param msg pointer to structure of organising access to MCT messages
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_get_extraparameters(MctMessage *msg, int verbose);

/**
 * Set standard header extra parameters
 * @param msg pointer to structure of organising access to MCT messages
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_set_extraparameters(MctMessage *msg, int verbose);

/**
 * Initialise the structure used to access a MCT file.
 * This function must be called before using further mct_file functions.
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_init(MctFile *file, int verbose);
/**
 * Set a list to filters.
 * This function should be called before loading a MCT file, if filters should be used.
 * A filter list is an array of filters. Several filters are combined logically by or operation.
 * The filter list is not copied, so take care to keep list in memory.
 * @param file pointer to structure of organising access to MCT file
 * @param filter pointer to filter list array
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_set_filter(MctFile *file, MctFilter *filter, int verbose);
/**
 * Initialising loading a MCT file.
 * @param file pointer to structure of organising access to MCT file
 * @param filename filename of MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_open(MctFile *file, const char *filename, int verbose);
/**
 * This function reads MCT file and parse MCT message one by one.
 * Each message will be written into new file.
 * If a filter is set, the filter list is used.
 * @param file pointer to structure of organizing access to MCT file
 * @param filename file to contain parsed MCT messages.
 * @param type 1 = payload as hex, 2 = payload as ASCII.
 * @param verbose if set to true verbose information is printed out.
 * @return 0 = message does not match filter, 1 = message was read, negative value if there was an error
 */
MctReturnValue mct_file_quick_parsing(MctFile *file, const char *filename, int type, int verbose);
/**
 * Find next message in the MCT file and parse them.
 * This function finds the next message in the MCT file.
 * If a filter is set, the filter list is used.
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return 0 = message does not match filter, 1 = message was read, negative value if there was an error
 */
MctReturnValue mct_file_read(MctFile *file, int verbose);
/**
 * Find next message in the MCT file in RAW format (without storage header) and parse them.
 * This function finds the next message in the MCT file.
 * If a filter is set, the filter list is used.
 * @param file pointer to structure of organising access to MCT file
 * @param resync Resync to serial header when set to true
 * @param verbose if set to true verbose information is printed out.
 * @return 0 = message does not match filter, 1 = message was read, negative value if there was an error
 */
MctReturnValue mct_file_read_raw(MctFile *file, int resync, int verbose);
/**
 * Closing loading a MCT file.
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_close(MctFile *file, int verbose);
/**
 * Load standard header of a message from file
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_read_header(MctFile *file, int verbose);
/**
 * Load standard header of a message from file in RAW format (without storage header)
 * @param file pointer to structure of organising access to MCT file
 * @param resync Resync to serial header when set to true
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_read_header_raw(MctFile *file, int resync, int verbose);
/**
 * Load, if available in message, extra standard header fields and
 * extended header of a message from file
 * (mct_file_read_header() must have been called before this call!)
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_read_header_extended(MctFile *file, int verbose);
/**
 * Load payload of a message from file
 * (mct_file_read_header() must have been called before this call!)
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_read_data(MctFile *file, int verbose);
/**
 * Load headers and payload of a message selected by the index.
 * If filters are set, index is based on the filtered list.
 * @param file pointer to structure of organising access to MCT file
 * @param index position of message in the files beginning from zero
 * @param verbose if set to true verbose information is printed out.
 * @return number of messages loaded, negative value if there was an error
 */
MctReturnValue mct_file_message(MctFile *file, int index, int verbose);
/**
 * Free the used memory by the organising structure of file.
 * @param file pointer to structure of organising access to MCT file
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_file_free(MctFile *file, int verbose);

/**
 * Set internal logging filename if mode 2
 * @param filename the filename
 */
void mct_log_set_filename(const char *filename);
#if defined MCT_DAEMON_USE_FIFO_IPC || defined MCT_LIB_USE_FIFO_IPC
/**
 * Set FIFO base direction
 * @param pipe_dir the pipe direction
 */
void mct_log_set_fifo_basedir(const char *pipe_dir);
#endif
/**
 * Set internal logging level
 * @param level the level
 */
void mct_log_set_level(int level);

/**
 * Set whether to print "name" and "unit" attributes in console output
 * @param state  true = with attributes, false = without attributes
 */
void mct_print_with_attributes(bool state);

/**
 * Initialize (external) logging facility
 * @param mode positive, 0 = log to stdout, 1 = log to syslog, 2 = log to file, 3 = log to stderr
 */
void mct_log_init(int mode);
/**
 * Print with variable arguments to specified file descriptor by MCT_LOG_MODE environment variable (like fprintf)
 * @param format format string for message
 * @return negative value if there was an error or the total number of characters written is returned on success
 */
int mct_user_printf(const char *format, ...) PRINTF_FORMAT(1, 2);
/**
 * Log ASCII string with null-termination to (external) logging facility
 * @param prio priority (see syslog() call)
 * @param s Pointer to ASCII string with null-termination
 * @return negative value if there was an error
 */
MctReturnValue mct_log(int prio, char *s);
/**
 * Log with variable arguments to (external) logging facility (like printf)
 * @param prio priority (see syslog() call)
 * @param format format string for log message
 * @return negative value if there was an error
 */
MctReturnValue mct_vlog(int prio, const char *format, ...) PRINTF_FORMAT(2, 3);
/**
 * Log size bytes with variable arguments to (external) logging facility (similar to snprintf)
 * @param prio priority (see syslog() call)
 * @param size number of bytes to log
 * @param format format string for log message
 * @return negative value if there was an error
 */
MctReturnValue mct_vnlog(int prio, size_t size, const char *format, ...) PRINTF_FORMAT(3, 4);
/**
 * De-Initialize (external) logging facility
 */
void mct_log_free(void);

/**
 * Initialising a mct receiver structure
 * @param receiver pointer to mct receiver structure
 * @param _fd handle to file/socket/fifo, fram which the data should be received
 * @param type specify whether received data is from socket or file/fifo
 * @param _buffersize size of data buffer for storing the received data
 * @return negative value if there was an error
 */
MctReturnValue mct_receiver_init(MctReceiver *receiver, int _fd, MctReceiverType type, int _buffersize);
/**
 * De-Initialize a mct receiver structure
 * @param receiver pointer to mct receiver structure
 * @return negative value if there was an error
 */
MctReturnValue mct_receiver_free(MctReceiver *receiver);
/**
 * Initialising a mct receiver structure
 * @param receiver pointer to mct receiver structure
 * @param fd handle to file/socket/fifo, fram which the data should be received
 * @param type specify whether received data is from socket or file/fifo
 * @param buffer data buffer for storing the received data
 * @return negative value if there was an error and zero if success
 */
MctReturnValue mct_receiver_init_global_buffer(MctReceiver *receiver, int fd, MctReceiverType type, char **buffer);
/**
 * De-Initialize a mct receiver structure
 * @param receiver pointer to mct receiver structure
 * @return negative value if there was an error and zero if success
 */
MctReturnValue mct_receiver_free_global_buffer(MctReceiver *receiver);
/**
 * Receive data from socket or file/fifo using the mct receiver structure
 * @param receiver pointer to mct receiver structure
 * @return number of received bytes or negative value if there was an error
 */
int mct_receiver_receive(MctReceiver *receiver);
/**
 * Remove a specific size of bytes from the received data
 * @param receiver pointer to mct receiver structure
 * @param size amount of bytes to be removed
 * @return negative value if there was an error
 */
MctReturnValue mct_receiver_remove(MctReceiver *receiver, int size);
/**
 * Move data from last receive call to front of receive buffer
 * @param receiver pointer to mct receiver structure
 * @return negative value if there was an error
 */
MctReturnValue mct_receiver_move_to_begin(MctReceiver *receiver);

/**
 * Check whether to_get amount of data is available in receiver and
 * copy it to dest. Skip the MctUserHeader if skip_header is set to 1.
 * @param receiver pointer to mct receiver structure
 * @param dest pointer to the destination buffer
 * @param to_get size of the data to copy in dest
 * @param skip_header whether if the MctUserHeader must be skipped.
 */
int mct_receiver_check_and_get(MctReceiver *receiver,
                               void *dest,
                               unsigned int to_get,
                               unsigned int skip_header);

/**
 * Fill out storage header of a mct message
 * @param storageheader pointer to storage header of a mct message
 * @param ecu name of ecu to be set in storage header
 * @return negative value if there was an error
 */
MctReturnValue mct_set_storageheader(MctStorageHeader *storageheader, const char *ecu);
/**
 * Check if a storage header contains its marker
 * @param storageheader pointer to storage header of a mct message
 * @return 0 no, 1 yes, negative value if there was an error
 */
MctReturnValue mct_check_storageheader(MctStorageHeader *storageheader);

/**
 * Checks if received size is big enough for expected data
 * @param received size
 * @param required size
 * @return negative value if required size is not sufficient
 * */
MctReturnValue mct_check_rcv_data_size(int received, int required);

/**
 * Initialise static ringbuffer with a size of size.
 * Initialise as server. Init counters.
 * Memory is already allocated.
 * @param buf Pointer to ringbuffer structure
 * @param ptr Ptr to ringbuffer memory
 * @param size Maximum size of buffer in bytes
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_init_static_server(MctBuffer *buf, const unsigned char *ptr, uint32_t size);

/**
 * Initialize static ringbuffer with a size of size.
 * Initialise as a client. Do not change counters.
 * Memory is already allocated.
 * @param buf Pointer to ringbuffer structure
 * @param ptr Ptr to ringbuffer memory
 * @param size Maximum size of buffer in bytes
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_init_static_client(MctBuffer *buf, const unsigned char *ptr, uint32_t size);

/**
 * Initialize dynamic ringbuffer with a size of size.
 * Initialise as a client. Do not change counters.
 * Memory will be allocated starting with min_size.
 * If more memory is needed size is increased wit step_size.
 * The maximum size is max_size.
 * @param buf Pointer to ringbuffer structure
 * @param min_size Minimum size of buffer in bytes
 * @param max_size Maximum size of buffer in bytes
 * @param step_size size of which ringbuffer is increased
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_init_dynamic(MctBuffer *buf, uint32_t min_size, uint32_t max_size, uint32_t step_size);

/**
 * Deinitilaise usage of static ringbuffer
 * @param buf Pointer to ringbuffer structure
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_free_static(MctBuffer *buf);

/**
 * Release and free memory used by dynamic ringbuffer
 * @param buf Pointer to ringbuffer structure
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_free_dynamic(MctBuffer *buf);

/**
 * Check if message fits into buffer.
 * @param buf Pointer to buffer structure
 * @param needed Needed size
 * @return MCT_RETURN_OK if enough space, MCT_RETURN_ERROR otherwise
 */
MctReturnValue mct_buffer_check_size(MctBuffer *buf, int needed);

/**
 * Write one entry to ringbuffer
 * @param buf Pointer to ringbuffer structure
 * @param data Pointer to data to be written to ringbuffer
 * @param size Size of data in bytes to be written to ringbuffer
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_push(MctBuffer *buf, const unsigned char *data, unsigned int size);

/**
 * Write up to three entries to ringbuffer.
 * Entries are joined to one block.
 * @param buf Pointer to ringbuffer structure
 * @param data1 Pointer to data to be written to ringbuffer
 * @param size1 Size of data in bytes to be written to ringbuffer
 * @param data2 Pointer to data to be written to ringbuffer
 * @param size2 Size of data in bytes to be written to ringbuffer
 * @param data3 Pointer to data to be written to ringbuffer
 * @param size3 Size of data in bytes to be written to ringbuffer
 * @return negative value if there was an error
 */
MctReturnValue mct_buffer_push3(MctBuffer *buf,
                                const unsigned char *data1,
                                unsigned int size1,
                                const unsigned char *data2,
                                unsigned int size2,
                                const unsigned char *data3,
                                unsigned int size3);

/**
 * Read one entry from ringbuffer.
 * Remove it from ringbuffer.
 * @param buf Pointer to ringbuffer structure
 * @param data Pointer to data read from ringbuffer
 * @param max_size Max size of read data in bytes from ringbuffer
 * @return size of read data, zero if no data available, negative value if there was an error
 */
int mct_buffer_pull(MctBuffer *buf, unsigned char *data, int max_size);

/**
 * Read one entry from ringbuffer.
 * Do not remove it from ringbuffer.
 * @param buf Pointer to ringbuffer structure
 * @param data Pointer to data read from ringbuffer
 * @param max_size Max size of read data in bytes from ringbuffer
 * @return size of read data, zero if no data available, negative value if there was an error
 */
int mct_buffer_copy(MctBuffer *buf, unsigned char *data, int max_size);

/**
 * Remove entry from ringbuffer.
 * @param buf Pointer to ringbuffer structure
 * @return size of read data, zero if no data available, negative value if there was an error
 */
int mct_buffer_remove(MctBuffer *buf);

/**
 * Print information about buffer and log to internal MCT log.
 * @param buf Pointer to ringbuffer structure
 */
void mct_buffer_info(MctBuffer *buf);

/**
 * Print status of buffer and log to internal MCT log.
 * @param buf Pointer to ringbuffer structure
 */
void mct_buffer_status(MctBuffer *buf);

/**
 * Get total size in bytes of ringbuffer.
 * If buffer is dynamic, max size is returned.
 * @param buf Pointer to ringbuffer structure
 * @return total size of buffer
 */
uint32_t mct_buffer_get_total_size(MctBuffer *buf);

/**
 * Get used size in bytes of ringbuffer.
 * @param buf Pointer to ringbuffer structure
 * @return used size of buffer
 */
int mct_buffer_get_used_size(MctBuffer *buf);

/**
 * Get number of entries in ringbuffer.
 * @param buf Pointer to ringbuffer structure
 * @return number of entries
 */
int mct_buffer_get_message_count(MctBuffer *buf);

#   if !defined (__WIN32__)

/**
 * Helper function: Setup serial connection
 * @param fd File descriptor of serial tty device
 * @param speed Serial line speed, as defined in termios.h
 * @return negative value if there was an error
 */
MctReturnValue mct_setup_serial(int fd, speed_t speed);

/**
 * Helper function: Convert serial line baudrate (as number) to line speed (as defined in termios.h)
 * @param baudrate Serial line baudrate (as number)
 * @return Serial line speed, as defined in termios.h
 */
speed_t mct_convert_serial_speed(int baudrate);

/**
 * Print mct version and mct svn version to buffer
 * @param buf Pointer to buffer
 * @param size size of buffer
 */
void mct_get_version(char *buf, size_t size);

/**
 * Print mct major version to buffer
 * @param buf Pointer to buffer
 * @param size size of buffer
 */
void mct_get_major_version(char *buf, size_t size);

/**
 * Print mct minor version to buffer
 * @param buf Pointer to buffer
 * @param size size of buffer
 */
void mct_get_minor_version(char *buf, size_t size);

#   endif

/* Function prototypes which should be used only internally */
/*                                                          */

/**
 * Common part of initialisation. Evaluates the following environment variables
 * and stores them in mct_user struct:
 * - MCT_DISABLE_EXTENDED_HEADER_FOR_NONVERBOSE
 * - MCT_LOCAL_PRINT_MODE (AUTOMATIC: 0, FORCE_ON: 2, FORCE_OFF: 3)
 * - MCT_INITIAL_LOG_LEVEL (e.g. APPx:CTXa:6;APPx:CTXb:5)
 * - MCT_FORCE_BLOCKING
 * - MCT_USER_BUFFER_MIN
 * - MCT_USER_BUFFER_MAX
 * - MCT_USER_BUFFER_STEP
 * - MCT_LOG_MSG_BUF_LEN
 * - MCT_DISABLE_INJECTION_MSG_AT_USER
 * @return negative value if there was an error
 */
MctReturnValue mct_init_common(void);

/**
 * Return the uptime of the system in 0.1 ms resolution
 * @return 0 if there was an error
 */
uint32_t mct_uptime(void);

/**
 * Print header of a MCT message
 * @param message pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the header is written
 * @param size maximal size of text buffer
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_print_header(MctMessage *message, char *text, uint32_t size, int verbose);

/**
 * Print payload of a MCT message as Hex-Output
 * @param message pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the output is written
 * @param size maximal size of text buffer
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_print_hex(MctMessage *message, char *text, uint32_t size, int verbose);

/**
 * Print payload of a MCT message as ASCII-Output
 * @param message pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the output is written
 * @param size maximal size of text buffer
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_print_ascii(MctMessage *message, char *text, uint32_t size, int verbose);

/**
 * Print payload of a MCT message as Mixed-Ouput (Hex and ASCII), for plain text output
 * @param message pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the output is written
 * @param size maximal size of text buffer
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_print_mixed_plain(MctMessage *message, char *text, uint32_t size, int verbose);

/**
 * Print payload of a MCT message as Mixed-Ouput (Hex and ASCII), for HTML text output
 * @param message pointer to structure of organising access to MCT messages
 * @param text pointer to a ASCII string, in which the output is written
 * @param size maximal size of text buffer
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_print_mixed_html(MctMessage *message, char *text, uint32_t size, int verbose);

/**
 * Decode and print a argument of a MCT message
 * @param msg pointer to structure of organising access to MCT messages
 * @param type_info Type of argument
 * @param ptr pointer to pointer to data (pointer to data is changed within this function)
 * @param datalength pointer to datalength (datalength is changed within this function)
 * @param text pointer to a ASCII string, in which the output is written
 * @param textlength maximal size of text buffer
 * @param byteLength If argument is a string, and this value is 0 or greater, this value will be taken as string length
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
MctReturnValue mct_message_argument_print(MctMessage *msg,
                                          uint32_t type_info,
                                          uint8_t **ptr,
                                          int32_t *datalength,
                                          char *text,
                                          size_t textlength,
                                          int byteLength,
                                          int verbose);

/**
 * Check environment variables.
 */
void mct_check_envvar(void);

/**
 * Parse the response text and identifying service id and its options.
 *
 * @param resp_text   char *
 * @param service_id  int *
 * @param service_opt int *
 * @return pointer to resp_text
 */
int mct_set_loginfo_parse_service_id(char *resp_text, uint32_t *service_id, uint8_t *service_opt);

/**
 * Convert get log info from ASCII to uint16
 *
 * @param rp        char
 * @param rp_count  int
 * @return length
 */
int16_t mct_getloginfo_conv_ascii_to_uint16_t(char *rp, int *rp_count);

/**
 * Convert get log info from ASCII to int16
 *
 * @param rp        char
 * @param rp_count  int
 * @return length
 */
int16_t mct_getloginfo_conv_ascii_to_int16_t(char *rp, int *rp_count);

/**
 * Convert get log info from ASCII to ID
 *
 * @param rp        char
 * @param rp_count  int
 * @param wp        char
 * @param len       int
 */
void mct_getloginfo_conv_ascii_to_id(char *rp, int *rp_count, char *wp, int len);

/**
 * Convert from hex ASCII to binary
 * @param ptr    const char
 * @param binary uint8_t
 * @param size   int
 */
void mct_hex_ascii_to_binary(const char *ptr, uint8_t *binary, int *size);

/**
 * Helper function to execute the execvp function in a new child process.
 * @param filename file path to store the stdout of command (NULL if not required)
 * @param command execution command followed by arguments with NULL-termination
 * @return negative value if there was an error
 */
int mct_execute_command(char *filename, char *command, ...);

#   ifdef __cplusplus
}
#   endif

/**
 \}
 */

#endif /* MCT_COMMON_H */
