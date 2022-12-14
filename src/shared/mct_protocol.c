#include "mct_protocol.h"

const char *const mct_service_names[] = {
    "MCT_SERVICE_ID",
    "MCT_SERVICE_ID_SET_LOG_LEVEL",
    "MCT_SERVICE_ID_SET_TRACE_STATUS",
    "MCT_SERVICE_ID_GET_LOG_INFO",
    "MCT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL",
    "MCT_SERVICE_ID_STORE_CONFIG",
    "MCT_SERVICE_ID_RESET_TO_FACTORY_DEFAULT",
    "MCT_SERVICE_ID_SET_COM_INTERFACE_STATUS",
    "MCT_SERVICE_ID_SET_COM_INTERFACE_MAX_BANDWIDTH",
    "MCT_SERVICE_ID_SET_VERBOSE_MODE",
    "MCT_SERVICE_ID_SET_MESSAGE_FILTERING",
    "MCT_SERVICE_ID_SET_TIMING_PACKETS",
    "MCT_SERVICE_ID_GET_LOCAL_TIME",
    "MCT_SERVICE_ID_USE_ECU_ID",
    "MCT_SERVICE_ID_USE_SESSION_ID",
    "MCT_SERVICE_ID_USE_TIMESTAMP",
    "MCT_SERVICE_ID_USE_EXTENDED_HEADER",
    "MCT_SERVICE_ID_SET_DEFAULT_LOG_LEVEL",
    "MCT_SERVICE_ID_SET_DEFAULT_TRACE_STATUS",
    "MCT_SERVICE_ID_GET_SOFTWARE_VERSION",
    "MCT_SERVICE_ID_MESSAGE_BUFFER_OVERFLOW"
};
const char *const mct_user_service_names[] = {
    "MCT_USER_SERVICE_ID",
    "MCT_SERVICE_ID_UNREGISTER_CONTEXT",
    "MCT_SERVICE_ID_CONNECTION_INFO",
    "MCT_SERVICE_ID_TIMEZONE",
    "MCT_SERVICE_ID_MARKER",
    "MCT_SERVICE_ID_OFFLINE_LOGSTORAGE",
    "MCT_SERVICE_ID_PASSIVE_NODE_CONNECT",
    "MCT_SERVICE_ID_PASSIVE_NODE_CONNECTION_STATUS",
    "MCT_SERVICE_ID_SET_ALL_LOG_LEVEL",
    "MCT_SERVICE_ID_SET_ALL_TRACE_STATUS",
    "MCT_SERVICE_ID_UNDEFINED", /* 0xF0A is not defined */
    "MCT_SERVICE_ID_SET_BLOCK_MODE",
    "MCT_SERVICE_ID_GET_BLOCK_MODE",
    "MCT_SERVICE_ID_SET_FILTER_LEVEL",
    "MCT_SERVICE_ID_GET_FILTER_STATUS"
};

const char *mct_get_service_name(unsigned int id)
{
    if (id == MCT_SERVICE_ID_CALLSW_CINJECTION)
        return "MCT_SERVICE_ID_CALLSW_CINJECTION";
    else if ((id == MCT_SERVICE_ID) || (id >= MCT_USER_SERVICE_ID_LAST_ENTRY) ||
             ((id >= MCT_SERVICE_ID_LAST_ENTRY) && (id <= MCT_USER_SERVICE_ID)))
        return "UNDEFINED";
    else if ((id > MCT_SERVICE_ID) && (id < MCT_SERVICE_ID_LAST_ENTRY))
        return mct_service_names[id];
    else /* user services */
        return mct_user_service_names[id & 0xFF];
}
