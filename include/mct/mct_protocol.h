#ifndef MCT_PROTOCOL_H
#define MCT_PROTOCOL_H

/**
 * \defgroup protocolapi MCT Protocol API
 * \addtogroup protocolapi
 \{
 */

/*
 * Definitions of the htyp parameter in standard header.
 */
#define MCT_HTYP_UEH  0x01 /**< use extended header */
#define MCT_HTYP_MSBF 0x02 /**< MSB first */
#define MCT_HTYP_WEID 0x04 /**< with ECU ID */
#define MCT_HTYP_WSID 0x08 /**< with session ID */
#define MCT_HTYP_WTMS 0x10 /**< with timestamp */
#define MCT_HTYP_VERS 0xe0 /**< version number, 0x1 */

#define MCT_IS_HTYP_UEH(htyp)  ((htyp) & MCT_HTYP_UEH)
#define MCT_IS_HTYP_MSBF(htyp) ((htyp) & MCT_HTYP_MSBF)
#define MCT_IS_HTYP_WEID(htyp) ((htyp) & MCT_HTYP_WEID)
#define MCT_IS_HTYP_WSID(htyp) ((htyp) & MCT_HTYP_WSID)
#define MCT_IS_HTYP_WTMS(htyp) ((htyp) & MCT_HTYP_WTMS)

#define MCT_HTYP_PROTOCOL_VERSION1 (1 << 5)

/*
 * Definitions of msin parameter in extended header.
 */
#define MCT_MSIN_VERB 0x01 /**< verbose */
#define MCT_MSIN_MSTP 0x0e /**< message type */
#define MCT_MSIN_MTIN 0xf0 /**< message type info */

#define MCT_MSIN_MSTP_SHIFT 1 /**< shift right offset to get mstp value */
#define MCT_MSIN_MTIN_SHIFT 4 /**< shift right offset to get mtin value */

#define MCT_IS_MSIN_VERB(msin)   ((msin) & MCT_MSIN_VERB)
#define MCT_GET_MSIN_MSTP(msin) (((msin) & MCT_MSIN_MSTP) >> MCT_MSIN_MSTP_SHIFT)
#define MCT_GET_MSIN_MTIN(msin) (((msin) & MCT_MSIN_MTIN) >> MCT_MSIN_MTIN_SHIFT)

/*
 * Definitions of mstp parameter in extended header.
 */
#define MCT_TYPE_LOG       0x00 /**< Log message type */
#define MCT_TYPE_APP_TRACE 0x01 /**< Application trace message type */
#define MCT_TYPE_NW_TRACE  0x02 /**< Network trace message type */
#define MCT_TYPE_CONTROL   0x03 /**< Control message type */

/*
 * Definitions of msti parameter in extended header.
 */
#define MCT_TRACE_VARIABLE     0x01 /**< tracing of a variable */
#define MCT_TRACE_FUNCTION_IN  0x02 /**< tracing of function calls */
#define MCT_TRACE_FUNCTION_OUT 0x03 /**< tracing of function return values */
#define MCT_TRACE_STATE        0x04 /**< tracing of states of a state machine */
#define MCT_TRACE_VFB          0x05 /**< tracing of virtual function bus */

/*
 * Definitions of msbi parameter in extended header.
 */

/* see file mct_user.h */

/*
 * Definitions of msci parameter in extended header.
 */
#define MCT_CONTROL_REQUEST    0x01 /**< Request message */
#define MCT_CONTROL_RESPONSE   0x02 /**< Response to request message */
#define MCT_CONTROL_TIME       0x03 /**< keep-alive message */

#define MCT_MSIN_CONTROL_REQUEST  ((MCT_TYPE_CONTROL << MCT_MSIN_MSTP_SHIFT) | \
                                   (MCT_CONTROL_REQUEST << MCT_MSIN_MTIN_SHIFT))
#define MCT_MSIN_CONTROL_RESPONSE ((MCT_TYPE_CONTROL << MCT_MSIN_MSTP_SHIFT) | \
                                   (MCT_CONTROL_RESPONSE << MCT_MSIN_MTIN_SHIFT))
#define MCT_MSIN_CONTROL_TIME     ((MCT_TYPE_CONTROL << MCT_MSIN_MSTP_SHIFT) | \
                                   (MCT_CONTROL_TIME << MCT_MSIN_MTIN_SHIFT))

/*
 * Definitions of types of arguments in payload.
 */
#define MCT_TYPE_INFO_TYLE 0x0000000f /**< Length of standard data: 1 = 8bit, 2 = 16bit, 3 = 32 bit, 4 = 64 bit, 5 = 128 bit */
#define MCT_TYPE_INFO_BOOL 0x00000010 /**< Boolean data */
#define MCT_TYPE_INFO_SINT 0x00000020 /**< Signed integer data */
#define MCT_TYPE_INFO_UINT 0x00000040 /**< Unsigned integer data */
#define MCT_TYPE_INFO_FLOA 0x00000080 /**< Float data */
#define MCT_TYPE_INFO_ARAY 0x00000100 /**< Array of standard types */
#define MCT_TYPE_INFO_STRG 0x00000200 /**< String */
#define MCT_TYPE_INFO_RAWD 0x00000400 /**< Raw data */
#define MCT_TYPE_INFO_VARI 0x00000800 /**< Set, if additional information to a variable is available */
#define MCT_TYPE_INFO_FIXP 0x00001000 /**< Set, if quantization and offset are added */
#define MCT_TYPE_INFO_TRAI 0x00002000 /**< Set, if additional trace information is added */
#define MCT_TYPE_INFO_STRU 0x00004000 /**< Struct */
#define MCT_TYPE_INFO_SCOD 0x00038000 /**< coding of the type string: 0 = ASCII, 1 = UTF-8 */

#define MCT_TYLE_8BIT      0x00000001
#define MCT_TYLE_16BIT     0x00000002
#define MCT_TYLE_32BIT     0x00000003
#define MCT_TYLE_64BIT     0x00000004
#define MCT_TYLE_128BIT    0x00000005

#define MCT_SCOD_ASCII      0x00000000
#define MCT_SCOD_UTF8       0x00008000
#define MCT_SCOD_HEX        0x00010000
#define MCT_SCOD_BIN        0x00018000

/*
 * Definitions of MCT services.
 */
#define MCT_SERVICE_ID_CALLSW_CINJECTION 0xFFF

enum mct_services {
    MCT_SERVICE_ID = 0x00,
    MCT_SERVICE_ID_SET_LOG_LEVEL = 0x01,
    MCT_SERVICE_ID_SET_TRACE_STATUS = 0x02,
    MCT_SERVICE_ID_GET_LOG_INFO = 0x03,
    MCT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL = 0x04,
    MCT_SERVICE_ID_STORE_CONFIG = 0x05,
    MCT_SERVICE_ID_RESET_TO_FACTORY_DEFAULT = 0x06,
    MCT_SERVICE_ID_SET_COM_INTERFACE_STATUS = 0x07,
    MCT_SERVICE_ID_SET_COM_INTERFACE_MAX_BANDWIDTH = 0x08,
    MCT_SERVICE_ID_SET_VERBOSE_MODE = 0x09,
    MCT_SERVICE_ID_SET_MESSAGE_FILTERING = 0x0A,
    MCT_SERVICE_ID_SET_TIMING_PACKETS = 0x0B,
    MCT_SERVICE_ID_GET_LOCAL_TIME = 0x0C,
    MCT_SERVICE_ID_USE_ECU_ID = 0x0D,
    MCT_SERVICE_ID_USE_SESSION_ID = 0x0E,
    MCT_SERVICE_ID_USE_TIMESTAMP = 0x0F,
    MCT_SERVICE_ID_USE_EXTENDED_HEADER = 0x10,
    MCT_SERVICE_ID_SET_DEFAULT_LOG_LEVEL = 0x11,
    MCT_SERVICE_ID_SET_DEFAULT_TRACE_STATUS = 0x12,
    MCT_SERVICE_ID_GET_SOFTWARE_VERSION = 0x13,
    MCT_SERVICE_ID_MESSAGE_BUFFER_OVERFLOW = 0x14,
    MCT_SERVICE_ID_LAST_ENTRY
};

enum mct_user_services {
    MCT_USER_SERVICE_ID = 0xF00,
    MCT_SERVICE_ID_UNREGISTER_CONTEXT = 0xF01,
    MCT_SERVICE_ID_CONNECTION_INFO = 0xF02,
    MCT_SERVICE_ID_TIMEZONE = 0xF03,
    MCT_SERVICE_ID_MARKER = 0xF04,
    MCT_SERVICE_ID_OFFLINE_LOGSTORAGE = 0xF05,
    MCT_SERVICE_ID_PASSIVE_NODE_CONNECT = 0xF06,
    MCT_SERVICE_ID_PASSIVE_NODE_CONNECTION_STATUS = 0xF07,
    MCT_SERVICE_ID_SET_ALL_LOG_LEVEL = 0xF08,
    MCT_SERVICE_ID_SET_ALL_TRACE_STATUS = 0xF09,
    MCT_SERVICE_ID_SET_BLOCK_MODE = 0xF0B,
    MCT_SERVICE_ID_GET_BLOCK_MODE = 0xF0C,
    MCT_SERVICE_ID_SET_FILTER_LEVEL = 0xF0D,
    MCT_SERVICE_ID_GET_FILTER_STATUS = 0xF0E,
    MCT_USER_SERVICE_ID_LAST_ENTRY
};

/* Need to be adapted if another service is added */
extern const char *const mct_service_names[];
extern const char *const mct_user_service_names[];

extern const char *mct_get_service_name(unsigned int id);

/*
 * Definitions of MCT service response status
 */
#define MCT_SERVICE_RESPONSE_OK            0x00 /**< Control message response: OK */
#define MCT_SERVICE_RESPONSE_NOT_SUPPORTED 0x01 /**< Control message response: Not supported */
#define MCT_SERVICE_RESPONSE_ERROR         0x02 /**< Control message response: Error */
#define MCT_SERVICE_RESPONSE_PERM_DENIED   0x03 /**< Control message response: Permission denied */
#define MCT_SERVICE_RESPONSE_WARNING       0x04 /**< Control message response: warning */
#define MCT_SERVICE_RESPONSE_LAST          0x05 /**< Used as max value */

/*
 * Definitions of MCT service connection state
 */
#define MCT_CONNECTION_STATUS_DISCONNECTED 0x01 /**< Client is disconnected */
#define MCT_CONNECTION_STATUS_CONNECTED    0x02 /**< Client is connected */

/*
 * Definitions of MCT GET_LOG_INFO status
 */
#define GET_LOG_INFO_STATUS_MIN 3
#define GET_LOG_INFO_STATUS_MAX 7
#define GET_LOG_INFO_STATUS_NO_MATCHING_CTX 8
#define GET_LOG_INFO_STATUS_RESP_DATA_OVERFLOW 9


/**
 \}
 */

#endif /* MCT_PROTOCOL_H */
