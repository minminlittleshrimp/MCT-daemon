#ifndef MCT_subscriber_H
#   define MCT_CLIMCT_subscriber_H

#   include "mct_types.h"
#   include "mct_common.h"
#   include <stdbool.h>

typedef enum
{
    MCT_SUBSCRIBER_MODE_UNDEFINED = -1,
    MCT_SUBSCRIBER_MODE_TCP,
    MCT_SUBSCRIBER_MODE_SERIAL,
    MCT_SUBSCRIBER_MODE_UNIX
} mct_subscriber_mode;

typedef struct
{
    mct_subscriber subscriber;  /**< subscriber pointer to MCT subscriber structure */
    int sock;              /**< sock Connection handle/socket */
    char *servIP;          /**< servIP IP adress/Hostname of interface */
    char *hostip;          /**< hostip IP address of UDP host subscriber interface */
    int port;              /**< Port for TCP connections (optional) */
    char *serialDevice;    /**< serialDevice Devicename of serial device */
    char *socketPath;      /**< socketPath Unix socket path */
    char ctrl_unit_id[4];         /**< ECUiD */
    speed_t baudrate;      /**< baudrate Baudrate of serial interface, as speed_t */
    mct_subscriber_mode mode;    /**< mode mct_subscriberMode */
} mct_subscriber;

#   ifdef __cplusplus
extern "C" {
#   endif

void mct_subscriber_register_message_callback(int (*registerd_callback)(MCTMessage *message, void *data));
void mct_subscriber_register_fetch_next_message_callback(bool (*registerd_callback)(void *data));

/**
 * Initialising MCT subscriber structure with a specific port
 * @param subscriber pointer to MCT subscriber structure
 * @param port The port for the tcp connection
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int MCT_subscriber_init_port(mct_subscriber *subscriber, int port, int verbose);

/**
 * Initialising MCT subscriber structure
 * @param subscriber pointer to MCT subscriber structure
 * @param verbose if set to true verbose information is printed out.
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_init(mct_subscriber *subscriber, int verbose);
/**
 * Connect to MCT daemon using the information from the MCT subscriber structure
 * @param subscriber pointer to MCT subscriber structure
 * @param verbose if set to true verbose information is printed out.
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_connect(mct_subscriber *subscriber, int verbose);
/**
 * Cleanup MCT subscriber structure
 * @param subscriber pointer to MCT subscriber structure
 * @param verbose if set to true verbose information is printed out.
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_cleanup(mct_subscriber *subscriber, int verbose);
/**
 * Main Loop of MCT subscriber application
 * @param subscriber pointer to MCT subscriber structure
 * @param data pointer to data to be provided to the main loop
 * @param verbose if set to true verbose information is printed out.
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_main_loop(mct_subscriber *subscriber, void *data, int verbose);

/**
 * Send a message to the daemon through the socket.
 * @param subscriber pointer to MCT subscriber structure.
 * @param msg The message to be send in MCT format.
 * @return Value from mct_return_value enum.
 */
mct_return_value MCT_subscriber_send_message_to_socket(mct_subscriber *subscriber, MCTMessage *msg);

/**
 * Send ancontrol message to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param apid application id
 * @param ctid context id
 * @param payload Buffer filled with control message data
 * @param size Size of control message data
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_ctrl_msg(mct_subscriber *subscriber, char *apid, char *ctid, uint8_t *payload, uint32_t size);
/**
 * Send an injection message to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param apid application id
 * @param ctid context id
 * @param serviceID service id
 * @param buffer Buffer filled with injection message data
 * @param size Size of injection data within buffer
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_inject_msg(mct_subscriber *subscriber,
                                          char *apid,
                                          char *ctid,
                                          uint32_t serviceID,
                                          uint8_t *buffer,
                                          uint32_t size);
/**
 * Send an set  log level message to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param apid application id
 * @param ctid context id
 * @param logLevel Log Level
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_log_level(mct_subscriber *subscriber, char *apid, char *ctid, uint8_t logLevel);
/**
 * Send an request to get log info message to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @return negative value if there was an error
 */
int MCT_subscriber_get_log_info(mct_subscriber *subscriber);
/**
 * Send an request to get default log level to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @return negative value if there was an error
 */
mct_return_value MCT_subscriber_get_default_log_level(mct_subscriber *subscriber);
/**
 * Send an request to get software version to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @return negative value if there was an error
 */
int MCT_subscriber_get_software_version(mct_subscriber *subscriber);
/**
 * Initialise get log info structure
 * @return void
 */
void MCT_getloginfo_init(void);
/**
 * To free the memory allocated for app description in get log info
 * @return void
 */
void MCT_getloginfo_free(void);
/**
 * Send a set trace status message to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param apid application id
 * @param ctid context id
 * @param traceStatus Default Trace Status
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_trace_status(mct_subscriber *subscriber, char *apid, char *ctid, uint8_t traceStatus);
/**
 * Send the default log level to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param defaultLogLevel Default Log Level
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_default_log_level(mct_subscriber *subscriber, uint8_t defaultLogLevel);
/**
 * Send the log level to all contexts registered with MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param LogLevel Log Level to be set
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_all_log_level(mct_subscriber *subscriber, uint8_t LogLevel);
/**
 * Send the default trace status to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param defaultTraceStatus Default Trace Status
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_default_trace_status(mct_subscriber *subscriber, uint8_t defaultTraceStatus);
/**
 * Send the trace status to all contexts registered with MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param traceStatus trace status to be set
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_all_trace_status(mct_subscriber *subscriber, uint8_t traceStatus);
/**
 * Send the timing pakets status to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @param timingPakets Timing pakets enabled
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_timing_pakets(mct_subscriber *subscriber, uint8_t timingPakets);
/**
 * Send the store config command to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_store_config(mct_subscriber *subscriber);
/**
 * Send the reset to factory default command to the MCT daemon
 * @param subscriber pointer to MCT subscriber structure
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_send_reset_to_factory_default(mct_subscriber *subscriber);

/**
 * Set baudrate within MCT subscriber structure
 * @param subscriber pointer to MCT subscriber structure
 * @param baudrate Baudrate
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_setbaudrate(mct_subscriber *subscriber, int baudrate);

/**
 * Set mode within MCT subscriber structure
 * @param subscriber pointer to MCT subscriber structure
 * @param mode mct_subscriberMode
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_set_mode(mct_subscriber *subscriber, mct_subscriberMode mode);

/**
 * Set server ip
 * @param subscriber pointer to MCT subscriber structure
 * @param ipaddr pointer to command line argument
 * @return negative value if there was an error
 */
int MCT_subscriber_set_server_ip(mct_subscriber *subscriber, char *ipaddr);

/**
 * Set server UDP host subscriber interface address
 * @param subscriber pointer to MCT subscriber structure
 * @param hostip pointer to multicast group address
 * @return negative value if there was an error
 */
int MCT_subscriber_set_host_if_address(mct_subscriber *subscriber, char *hostip);

/**
 * Set serial device
 * @param subscriber pointer to MCT subscriber structure
 * @param serial_device pointer to command line argument
 * @return negative value if there was an error
 */
int MCT_subscriber_set_serial_device(mct_subscriber *subscriber, char *serial_device);

/**
 * Set socket path
 * @param subscriber pointer to MCT subscriber structure
 * @param socket_path pointer to socket path string
 * @return negative value if there was an error
 */
int MCT_subscriber_set_socket_path(mct_subscriber *subscriber, char *socket_path);

/**
 * Parse GET_LOG_INFO response text
 * @param resp      GET_LOG_INFO response
 * @param resp_text response text represented by ASCII
 * @return Value from mct_return_value enum
 */
mct_return_value MCT_subscriber_parse_get_log_info_resp_text(MCTServiceGetLogInfoResponse *resp,
                                                       char *resp_text);

/**
 * Free memory allocated for get log info message
 * @param resp response
 * @return 0 on success, -1 otherwise
 */
int MCT_subscriber_cleanup_get_log_info(MCTServiceGetLogInfoResponse *resp);
#   ifdef __cplusplus
}
#   endif

#endif /* MCT_subscriber_H */
